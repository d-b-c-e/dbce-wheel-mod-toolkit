using System.Text;
using System.Text.Json;
using Dbce.Wheel.Shim;
using Xunit;

public sealed class ShimExtrasTests
{
    private static readonly byte[] Declaration = Encoding.ASCII.GetBytes("game fixture\ncontexts observed driving\nfixture.look stick circle contexts=driving\nfixture.reset digital contexts=driving\n");
    private static readonly ShimExtraDefinition[] Definitions = {
        new("fixture.look", "stick", "circle", new[] { "driving" }), new("fixture.reset", "digital", null, new[] { "driving" }) };
    private static string Report(int firstReset = 2, string? hash = null, int firstLook = 0) => JsonSerializer.Serialize(new {
        abi = 1, acceptedSchema = 1, game = "fixture", declaration = hash ?? Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(Declaration)),
        contextMode = "observed", contexts = new[] { "driving" }, extras = new object[] {
            new { id = "fixture.look", shape = "stick", range = "circle", firstSlot = firstLook, components = 2, contexts = new[] { "driving" } },
            new { id = "fixture.reset", shape = "digital", firstSlot = firstReset, components = 1, contexts = new[] { "driving" } } } });
    private static ShimExtrasInfo Info() => ShimExtrasInfo.Verify(Report(), Declaration, "fixture", "observed", new[] { "driving" }, Definitions);
    private static void Fill(byte[] common, byte[] extras, ulong sequence, uint contexts = 1)
    {
        BitConverter.GetBytes(ShimAbi2.FrameSize).CopyTo(common, 0); BitConverter.GetBytes(2).CopyTo(common, 4);
        BitConverter.GetBytes(sequence).CopyTo(common, 8);
        foreach (string action in new[] { "steer", "throttle", "brake", "handbrake" })
        {
            int i = Array.IndexOf(ShimAbi2.ActionNames.ToArray(), action);
            BitConverter.GetBytes(.25f).CopyTo(common,48+i*8); common[52+i*8] = (byte)ShimStatus.Live;
        }
        BitConverter.GetBytes(ShimExtrasAbi.FrameSize).CopyTo(extras, 0); BitConverter.GetBytes(1).CopyTo(extras, 4);
        BitConverter.GetBytes(sequence).CopyTo(extras, 8); BitConverter.GetBytes(3).CopyTo(extras, 32);
        BitConverter.GetBytes(contexts).CopyTo(extras, 36);
        for (int i = 0; i < 3; i++) extras[40 + i * 8 + 4] = (byte)ShimStatus.Live;
        BitConverter.GetBytes(.5f).CopyTo(extras, 40);
    }

    [Fact] public void MissingOptionalExportDoesNotCreateAHost()
    {
        var library = ShimExtrasNative.TryBind(_ => IntPtr.Zero, out var why);
        Assert.Null(library); Assert.Contains("dbce_shim_extras_abi_version", why);
    }
    [Theory] [InlineData(2,296)] [InlineData(1,304)]
    public void ExtensionVersionAndSizeMustMatch(int abi, int size) => Assert.Throws<InvalidDataException>(() => ShimExtrasAbi.Validate(abi, size));
    [Fact] public void DeclarationAndSlotMapMustMatch()
    {
        Assert.Equal(3, Info().SlotCount);
        var mapped = ShimExtrasInfo.Verify(Report(firstReset:0,firstLook:1), Declaration, "fixture", "observed", new[] { "driving" }, Definitions);
        Assert.Equal(1,mapped.Destinations["fixture.look"].FirstSlot);
        Assert.Equal(0,mapped.Destinations["fixture.reset"].FirstSlot);
        Assert.Throws<InvalidDataException>(() => ShimExtrasInfo.Verify(Report(hash: new string('0',64)), Declaration, "fixture", "observed", new[] { "driving" }, Definitions));
        Assert.Throws<InvalidDataException>(() => ShimExtrasInfo.Verify(Report(firstReset: 1), Declaration, "fixture", "observed", new[] { "driving" }, Definitions));
        Assert.Throws<InvalidDataException>(() => ShimExtrasInfo.Verify(Report().Replace("observed", "advisory"), Declaration, "fixture", "observed", new[] { "driving" }, Definitions));
    }
    [Theory] [InlineData(33)] [InlineData(2)]
    public void MalformedCountClearsBothFramesAndStopsOnce(int count)
    {
        int stops = 0;
        var session = new ShimExtrasSession(Info(), (mask,c,e) => { Fill(c,e,1,mask); BitConverter.GetBytes(count).CopyTo(e,32); return 0; }, () => stops++);
        session.Tick(1,0,1); session.Stop(); session.Tick(2,.01,1);
        Assert.True(session.Stopped); Assert.Null(session.Common); Assert.Null(session.Extras); Assert.Equal(1,stops);
    }
    [Fact] public void OneTickContainsCommonAndExtrasAndDeduplicates()
    {
        int ticks = 0, stops = 0;
        var session = new ShimExtrasSession(Info(), (mask,c,e) => { Fill(c,e,(ulong)++ticks,mask); return 0; }, () => stops++);
        session.Tick(10,1,1); session.Tick(10,1.01,1);
        Assert.Equal(1,ticks); Assert.Equal(session.Common!.Sequence,session.Extras!.Sequence);
        foreach (string action in new[] { "steer", "throttle", "brake", "handbrake" })
            Assert.Equal(.25f,session.Common.Actions[Array.IndexOf(ShimAbi2.ActionNames.ToArray(),action)].Applied);
        Assert.True(session.TrySample("fixture.look",10,1.01,1,out float x,out float y)); Assert.Equal(.5f,x); Assert.Equal(0,y);
        Assert.False(session.TrySample("fixture.look",10,1.01,0,out _,out _));
        session.Invalidate(); session.Tick(10,1.02,1); Assert.False(session.Fresh(10,1.02));
        session.Tick(11,1.03,1); Assert.True(session.Fresh(11,1.03)); Assert.Equal(2,ticks);
        session.Stop(); session.Stop(); Assert.Equal(1,stops);
    }
    [Fact] public void StaleSequenceNeverSuppliesCameraValues()
    {
        var session = new ShimExtrasSession(Info(), (mask,c,e) => { Fill(c,e,1,mask); return 0; }, () => {});
        session.Tick(1,1,1); session.Tick(2,1.3,1);
        Assert.False(session.Fresh(2,1.3)); Assert.False(session.TrySample("fixture.look",2,1.3,1,out _,out _));
        Assert.False(session.Fresh(3,1.3));
    }
    [Theory] [InlineData("sequence")] [InlineData("contexts")] [InlineData("status")] [InlineData("injected")] [InlineData("nan")] [InlineData("circle")] [InlineData("latched")]
    public void InvalidFrameCannotLeakPreviousValues(string fault)
    {
        int ticks = 0, stops = 0;
        var session = new ShimExtrasSession(Info(), (mask,c,e) => {
            Fill(c,e,(ulong)++ticks,mask);
            if (ticks == 2) switch (fault) {
                case "sequence": BitConverter.GetBytes(90UL).CopyTo(e,8); break;
                case "contexts": BitConverter.GetBytes(0u).CopyTo(e,36); break;
                case "status": e[44] = 15; break;
                case "injected": e[45] = 1; break;
                case "nan": BitConverter.GetBytes(float.NaN).CopyTo(e,40); break;
                case "circle": BitConverter.GetBytes(1f).CopyTo(e,40); BitConverter.GetBytes(1f).CopyTo(e,48); break;
                case "latched": c[41] = 1; break;
            }
            return 0;
        }, () => stops++);
        session.Tick(1,0,1); Assert.True(session.Fresh(1,0)); session.Tick(2,.01,1);
        Assert.True(session.Stopped); Assert.Null(session.Common); Assert.Null(session.Extras); Assert.Equal(1,stops);
    }
    [Fact] public void ContextInactiveOrOneUnavailableStickComponentRefusesWholeStick()
    {
        var session = new ShimExtrasSession(Info(), (mask,c,e) => { Fill(c,e,1,mask); e[52] = ShimExtrasAbi.ContextInactive; return 0; }, () => {});
        session.Tick(1,0,1); Assert.True(session.Fresh(1,0)); Assert.False(session.TrySample("fixture.look",1,0,1,out _,out _));
    }
    [Fact] public void NativeFailureStopsExactlyOnceEvenAfterExplicitStop()
    {
        int stops = 0;
        var session = new ShimExtrasSession(Info(), (mask,c,e) => -3, () => stops++);
        session.Tick(1,0,1); session.Stop(); Assert.Equal(1,stops); Assert.Null(session.Common);
    }
}
