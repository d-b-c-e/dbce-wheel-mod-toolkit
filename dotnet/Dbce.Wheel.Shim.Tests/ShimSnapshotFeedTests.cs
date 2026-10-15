using System.Buffers.Binary;
using Dbce.Wheel.Shim;
using Xunit;

// The shared per-row rules (ShimSnapshotFeed) and the completing-frame guard, from bytes built by ShimSnapshotTests.
public class ShimSnapshotFeedTests
{
    static (string Name, string IdentityKey)? Names(ShimSnapshotDevice row) => row.Index == 0 ? ("MOZA R12 Base", @"\\?\hid#wheel") : ("Pedals", "row:1");

    static ShimSnapshot Snap(Action<byte[], byte[], byte[]>? change = null)
    {
        var (h, d, s) = ShimSnapshotTests.Frame(change);
        return ShimSnapshot.Decode(h, d, s);
    }

    [Fact]
    public void ValidReadObservesWithCapsAppliedAndUnopenedRowsAreRemoved()
    {
        var items = new ShimSnapshotFeed().Read(Snap(), Names)!;
        Assert.Equal(2, items.Count);
        var wheel = items.Single(i => !i.IsRemoval);
        Assert.Equal(new Guid(ShimSnapshotTests.Wheel), wheel.Instance); Assert.Equal(new Guid(ShimSnapshotTests.WheelProduct), wheel.Product);
        Assert.Equal("MOZA R12 Base", wheel.Name); Assert.Equal(@"\\?\hid#wheel", wheel.IdentityKey);
        Assert.Equal(3UL, wheel.Generation); Assert.Equal(77UL, wheel.ReadSequence);
        Assert.Equal(new[] { 32767, 0, 65535, -100, 40000, 0, 0, 0 }, wheel.Axes);
        Assert.Equal(new[] { 9000, -1, 18000, -1 }, wheel.Hats);
        Assert.True(wheel.Buttons[5]); Assert.False(wheel.Buttons[6]); Assert.Equal(511, wheel.Maximum[3]);
        var pedals = items.Single(i => i.IsRemoval);
        Assert.Equal(ShimSnapshotFeed.Disconnected, pedals.Reason);   // never opened
    }

    [Fact]
    public void TheSameOrAnOlderFrameIsNoNewObservation()
    {
        var feed = new ShimSnapshotFeed();
        Assert.NotNull(feed.Read(Snap(), Names));
        Assert.Null(feed.Read(Snap(), Names));
        Assert.Null(feed.Read(Snap((h, d, s) => BinaryPrimitives.WriteUInt64LittleEndian(h.AsSpan(16), 41)), Names));
        Assert.NotNull(feed.Read(Snap((h, d, s) => BinaryPrimitives.WriteUInt64LittleEndian(h.AsSpan(16), 43)), Names));
    }

    public static IEnumerable<object[]> BadReads() => new (string, string, Action<byte[], byte[], byte[]>)[] {
        ("pending read", ShimSnapshotFeed.ReadFailed, (h, d, s) => { s[40] = (byte)ShimSnapshotCall.Pending; s[41] = (byte)ShimSnapshotCall.NotCalled; s[42] = 0; }),
        ("lost acquisition", ShimSnapshotFeed.ReadFailed, (h, d, s) => { s[40] = (byte)ShimSnapshotCall.AcquisitionLost; s[41] = (byte)ShimSnapshotCall.NotCalled; s[42] = 0; }),
        ("stale read", ShimSnapshotFeed.ReadStale, (h, d, s) => s[43] = 1),
        ("ended read, row closed", ShimSnapshotFeed.Disconnected, (h, d, s) => { s[40] = (byte)ShimSnapshotCall.Gone; s[41] = (byte)ShimSnapshotCall.NotCalled; s[42] = 0; s[44] = 1; d[16] = 0; }),
        ("read of another epoch", ShimSnapshotFeed.Disconnected, (h, d, s) => s[12] = 2),
    }.Select(c => new object[] { c.Item1, c.Item2, c.Item3 });

    [Theory]
    [MemberData(nameof(BadReads))]
    public void AnExplicitlyBadReadRemovesAtOnce(string why, string reason, Action<byte[], byte[], byte[]> change)
    {
        var items = new ShimSnapshotFeed().Read(Snap(change), Names)!;
        var wheel = items.Single(i => i.Instance == new Guid(ShimSnapshotTests.Wheel));
        Assert.True(wheel.IsRemoval, why); Assert.Equal(reason, wheel.Reason);
    }

    [Fact]
    public void AMissingReadAgesOutAndAMissingIdentityRemoves()
    {
        var items = new ShimSnapshotFeed().Read(Snap((h, d, s) => BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(52), 0)), Names)!;
        Assert.DoesNotContain(items, i => i.Instance == new Guid(ShimSnapshotTests.Wheel));   // opened, no read this frame: no item
        items = new ShimSnapshotFeed().Read(Snap(), _ => null)!;
        Assert.Equal(ShimSnapshotFeed.NoIdentity, items.Single(i => i.Instance == new Guid(ShimSnapshotTests.Wheel)).Reason);
    }

    [Fact]
    public void GuardOwnsListeningFramesAndTheFrameThatEndedTheCapture()
    {
        var guard = new SharedBindingFrameGuard();
        Assert.False(guard.Owns(10, false));
        Assert.True(guard.Owns(10, true));
        guard.Note(11);                         // noted before the snapshot that completed or refused the capture
        Assert.True(guard.Owns(11, false));
        Assert.False(guard.Owns(12, false));
    }
}
