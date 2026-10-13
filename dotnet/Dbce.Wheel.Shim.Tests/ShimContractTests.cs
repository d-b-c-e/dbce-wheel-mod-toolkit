using System.Buffers.Binary;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using Dbce.Wheel.Shim;
using Xunit;

public class ShimContractTests
{
    [Fact]
    public void ManagedContractMatchesCanonicalNativeHeaders()
    {
        string header = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "dbce_shim.h"));
        string actions = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "dbce_controls.hpp"));
        Assert.Contains("#define DBCE_SHIM_ABI 2", header);
        Assert.Contains("#define DBCE_SHIM_ACTIONS 31", header);
        var list = Regex.Match(actions, @"kActions\[\]\s*=\s*\{(?<ids>[^}]*)\}", RegexOptions.Singleline);
        Assert.True(list.Success);
        var ids = Regex.Matches(list.Groups["ids"].Value, "\"([a-zA-Z0-9]+)\"")
            .Select(m => m.Groups[1].Value).ToArray();
        Assert.Equal(ShimAbi2.ActionNames.ToArray(), ids);
        Assert.Equal(ShimAbi2.OpenArgsSize, Marshal.SizeOf<ShimOpenArgs>());
        Assert.Equal((IntPtr)120, Marshal.OffsetOf<ShimOpenArgs>(nameof(ShimOpenArgs.Seconds)));
    }

    [Fact]
    public void ExportContractRejectsChangedActionOrExtraAction()
    {
        ShimAbi2.Validate(2, 296, 128, i => i < 31 ? ShimAbi2.ActionNames[i] : null);
        Assert.Throws<InvalidDataException>(() => ShimAbi2.Validate(3, 296, 128, i => null));
        Assert.Throws<InvalidDataException>(() => ShimAbi2.Validate(2, 296, 128,
            i => i == 4 ? "wrong" : i < 31 ? ShimAbi2.ActionNames[i] : null));
        Assert.Throws<InvalidDataException>(() => ShimAbi2.Validate(2, 296, 128,
            i => i < 31 ? ShimAbi2.ActionNames[i] : "extra"));
    }

    [Fact]
    public void FrameDecodeUsesAbiLayoutAndNeutralizesUnavailableActions()
    {
        var bytes = new byte[ShimAbi2.FrameSize];
        BinaryPrimitives.WriteUInt32LittleEndian(bytes, 296);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(4), 2);
        BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(8), 17);
        BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(24), 1UL << 4);
        bytes[40] = (byte)ShimRun.Running;
        bytes[41] = 1;
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(44), 2);
        BitConverter.GetBytes(.75f).CopyTo(bytes, 48);
        bytes[52] = (byte)ShimStatus.Live;
        bytes[53] = 1;
        BitConverter.GetBytes(float.NaN).CopyTo(bytes, 56);
        bytes[60] = (byte)ShimStatus.Live;
        var frame = ShimAbi2.Decode(bytes);
        Assert.Equal((ulong)17, frame.Sequence);
        Assert.Equal(1UL << 4, frame.Down);
        Assert.Equal(ShimRun.Running, frame.Run);
        Assert.Equal((uint)2, frame.SampleCount);
        Assert.Equal(.75f, frame.Actions[0].Applied);
        Assert.True(frame.Actions[0].Injected);
        Assert.Equal(0f, frame.Actions[1].Applied);
        bytes[4] = 3;
        Assert.Throws<InvalidDataException>(() => ShimAbi2.Decode(bytes));
    }

    [Fact]
    public void ProfileUsesExactSectionBytesAndCoreHash()
    {
        byte[] file = Encoding.UTF8.GetBytes("[Other]\r\nx=1\r\n[Controls]\r\nSchema=1\r\nsteer=foo\r\n[After]\r\ny=2\r\n");
        byte[] section = ShimProfile.ControlsSection(file);
        Assert.Equal("[Controls]\r\nSchema=1\r\nsteer=foo\r\n", Encoding.UTF8.GetString(section));
        Assert.Equal(section, ShimProfile.ControlsSection(new byte[] { 0xef, 0xbb, 0xbf }.Concat(file).ToArray()));
        string hash = Convert.ToHexString(SHA256.HashData(section));
        string coreInfo = "ok=1\nprofile=Rig\nrevision=rev1\ntransmission=Sequential\nsha256=" + hash + "\n";
        var info = ShimProfile.Verify(section, coreInfo);
        Assert.Equal("Sequential", info.Transmission);
        Assert.Equal(hash, info.Sha256);
        Assert.Equal("foo", ShimProfile.SteeringBinding(section, coreInfo));
        Assert.Throws<InvalidDataException>(() => ShimProfile.SteeringBinding(section, "ok=1\nsha256=wrong\n"));
        Assert.Throws<InvalidDataException>(() => ShimProfile.Verify(section, "ok=1\nsha256=wrong\n"));
        Assert.Throws<InvalidDataException>(() => ShimProfile.ControlsSection(Encoding.UTF8.GetBytes("[Controls]\nx=1\n[Controls]\ny=2\n")));
    }

    [Fact]
    public void HashPinRefusesBeforeLoadingAnyLibrary()
    {
        string path = typeof(ShimContractTests).Assembly.Location;
        Assert.Throws<InvalidDataException>(() => ShimNativeLibrary.Load(path, new string('0', 64)));
    }
}
