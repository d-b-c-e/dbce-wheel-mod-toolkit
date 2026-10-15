using System.Buffers.Binary;
using System.Text;
using System.Text.RegularExpressions;
using Dbce.Wheel.Shim;
using Xunit;

// Extension 3 (dbce_shim_snapshot.h): the portable managed decoder, from bytes laid out by the C header.
public class ShimSnapshotTests
{
    const string Wheel = "{d71b8350-61b7-11f1-8001-444553540000}", WheelProduct = "{0006346e-0000-0000-0000-504944564944}";
    const string Pedals = "{d71b8350-61b7-11f1-8001-444553540099}", PedalProduct = "{0006346f-0000-0000-0000-504944564944}";

    // Field offsets as the decoder reads them; ContractMatchesNativeHeader proves them against the C structs.
    static readonly (string Struct, string Field, int Offset)[] Offsets = {
        ("dbce_shim_snapshot_header", "struct_size", 0), ("dbce_shim_snapshot_header", "abi", 4), ("dbce_shim_snapshot_header", "device_struct_size", 8),
        ("dbce_shim_snapshot_header", "sample_struct_size", 12), ("dbce_shim_snapshot_header", "frame_seq", 16), ("dbce_shim_snapshot_header", "tick_us", 24),
        ("dbce_shim_snapshot_header", "boundary_us", 32), ("dbce_shim_snapshot_header", "fresh_us", 40), ("dbce_shim_snapshot_header", "device_count", 48),
        ("dbce_shim_snapshot_header", "sample_count", 52),
        ("dbce_shim_snapshot_device", "struct_size", 0), ("dbce_shim_snapshot_device", "abi", 4), ("dbce_shim_snapshot_device", "index", 8),
        ("dbce_shim_snapshot_device", "epoch", 12), ("dbce_shim_snapshot_device", "flags", 16), ("dbce_shim_snapshot_device", "axis_mask", 20),
        ("dbce_shim_snapshot_device", "range_mask", 24), ("dbce_shim_snapshot_device", "pov_mask", 28), ("dbce_shim_snapshot_device", "button_mask", 32),
        ("dbce_shim_snapshot_device", "minimum", 48), ("dbce_shim_snapshot_device", "maximum", 80), ("dbce_shim_snapshot_device", "instance", 112),
        ("dbce_shim_snapshot_device", "product", 152),
        ("dbce_shim_snapshot_sample", "struct_size", 0), ("dbce_shim_snapshot_sample", "abi", 4), ("dbce_shim_snapshot_sample", "index", 8),
        ("dbce_shim_snapshot_sample", "epoch", 12), ("dbce_shim_snapshot_sample", "device_seq", 16), ("dbce_shim_snapshot_sample", "read_start_us", 24),
        ("dbce_shim_snapshot_sample", "read_end_us", 32), ("dbce_shim_snapshot_sample", "poll_call", 40), ("dbce_shim_snapshot_sample", "state_call", 41),
        ("dbce_shim_snapshot_sample", "valid", 42), ("dbce_shim_snapshot_sample", "stale", 43), ("dbce_shim_snapshot_sample", "ended", 44),
        ("dbce_shim_snapshot_sample", "axes", 48), ("dbce_shim_snapshot_sample", "buttons", 80), ("dbce_shim_snapshot_sample", "pov", 208),
    };
    static int At(string s, string f) => Offsets.Single(o => o.Struct == s && o.Field == f).Offset;

    [Fact]
    public void ContractMatchesNativeHeader()
    {
        string snapshot = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "dbce_shim_snapshot.h"));
        string shim = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "dbce_shim.h"));
        Assert.Contains("#define DBCE_SHIM_SNAPSHOT_ABI " + ShimSnapshotAbi.Version + "\n", snapshot.Replace("\r\n", "\n"));
        Assert.Contains("#define DBCE_SHIM_SNAPSHOT_MAX_DEVICES " + ShimSnapshotAbi.MaxDevices + "\n", snapshot.Replace("\r\n", "\n"));
        Assert.Contains("DBCE_SHIM_SNAPSHOT_OPENED = " + ShimSnapshotAbi.FlagOpened + ",", snapshot);
        Assert.Contains("DBCE_SHIM_SNAPSHOT_PATH = " + ShimSnapshotAbi.FlagPath + " ", snapshot);
        foreach (var (name, value) in new[] { ("OK", ShimSnapshotCall.Ok), ("ACQUISITION_LOST", ShimSnapshotCall.AcquisitionLost),
            ("GONE", ShimSnapshotCall.Gone), ("NOT_CALLED", ShimSnapshotCall.NotCalled), ("PENDING", ShimSnapshotCall.Pending) })
            Assert.Matches(@"DBCE_SHIM_CALL_" + name + @" = " + (int)value + @"\b", snapshot);
        foreach (var (name, value) in new[] { ("OK", ShimSnapshotAbi.Ok), ("BAD_ARGUMENT", ShimSnapshotAbi.BadArgument),
            ("WRONG_THREAD", ShimSnapshotAbi.WrongThread), ("REFUSED", ShimSnapshotAbi.Refused), ("STALE", ShimSnapshotAbi.Stale) })
            Assert.Matches(@"DBCE_SHIM_" + name + @" = " + value + @"\b", shim);

        var sizes = new Dictionary<string, int> { ["dbce_shim_snapshot_header"] = ShimSnapshotAbi.HeaderSize,
            ["dbce_shim_snapshot_device"] = ShimSnapshotAbi.DeviceSize, ["dbce_shim_snapshot_sample"] = ShimSnapshotAbi.SampleSize };
        foreach (var (structName, size) in sizes)
        {
            var layout = Layout(snapshot, structName, out int total);
            Assert.Equal(size, total);
            foreach (var o in Offsets.Where(o => o.Struct == structName)) Assert.Equal(o.Offset, layout[o.Field]);
            Assert.Equal(Offsets.Count(o => o.Struct == structName), layout.Count(f => !f.Key.StartsWith("reserved", StringComparison.Ordinal)));
        }
    }

    // Natural-alignment layout of one C struct in the header (no #pragma pack there), from its declarations.
    static Dictionary<string, int> Layout(string header, string name, out int total)
    {
        string text = Regex.Replace(header, @"/\*.*?\*/", "", RegexOptions.Singleline);
        var body = Regex.Match(text, @"typedef struct " + name + @"\s*\{(?<b>[^}]*)\}", RegexOptions.Singleline);
        Assert.True(body.Success, name);
        var sizes = new Dictionary<string, int> { ["uint8_t"] = 1, ["char"] = 1, ["uint32_t"] = 4, ["int32_t"] = 4, ["uint64_t"] = 8, ["int64_t"] = 8 };
        var offsets = new Dictionary<string, int>(); int at = 0, align = 1;
        foreach (var decl in body.Groups["b"].Value.Split(';').Select(d => d.Trim()).Where(d => d.Length > 0))
        {
            var m = Regex.Match(decl, @"^(?<t>\w+)\s+(?<names>.+)$", RegexOptions.Singleline);
            int size = sizes[m.Groups["t"].Value]; align = Math.Max(align, size);
            foreach (var part in m.Groups["names"].Value.Split(',').Select(p => p.Trim()))
            {
                var n = Regex.Match(part, @"^(?<n>\w+)(\[(?<c>\d+)\])?$");
                Assert.True(n.Success, part);
                at = (at + size - 1) / size * size; offsets[n.Groups["n"].Value] = at;
                at += size * (n.Groups["c"].Success ? int.Parse(n.Groups["c"].Value) : 1);
            }
        }
        total = (at + align - 1) / align * align;
        return offsets;
    }

    // Builders write through the same offset table.
    static byte[] Header(ulong seq, int devices, int samples)
    {
        var b = ShimSnapshot.NewHeader(); const string h = "dbce_shim_snapshot_header";
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(h, "abi")), 1);
        BinaryPrimitives.WriteUInt64LittleEndian(b.AsSpan(At(h, "frame_seq")), seq);
        BinaryPrimitives.WriteInt64LittleEndian(b.AsSpan(At(h, "tick_us")), 1_000_000);
        BinaryPrimitives.WriteInt64LittleEndian(b.AsSpan(At(h, "boundary_us")), 1_000_400);
        BinaryPrimitives.WriteInt64LittleEndian(b.AsSpan(At(h, "fresh_us")), 100_000);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(h, "device_count")), (uint)devices);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(h, "sample_count")), (uint)samples);
        return b;
    }

    static byte[] Device(int index, uint epoch, uint flags, uint axisMask, uint rangeMask, uint povMask, int[] buttons, string instance, string product)
    {
        var b = new byte[ShimSnapshotAbi.DeviceSize]; const string d = "dbce_shim_snapshot_device";
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "struct_size")), ShimSnapshotAbi.DeviceSize);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "abi")), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "index")), (uint)index);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "epoch")), epoch);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "flags")), flags);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "axis_mask")), axisMask);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "range_mask")), rangeMask);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(d, "pov_mask")), povMask);
        foreach (int button in buttons) b[At(d, "button_mask") + button / 8] |= (byte)(1 << (button % 8));
        for (int a = 0; a < 8; a++)
        {
            BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(At(d, "minimum") + a * 4), a == 3 ? -512 : 0);
            BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(At(d, "maximum") + a * 4), a == 3 ? 511 : 65535);
        }
        Encoding.ASCII.GetBytes(instance).CopyTo(b, At(d, "instance"));
        Encoding.ASCII.GetBytes(product).CopyTo(b, At(d, "product"));
        return b;
    }

    static byte[] Sample(int index, uint epoch, ulong deviceSeq, ShimSnapshotCall poll, ShimSnapshotCall state, bool valid, bool stale, bool ended,
        int[] axes, int[] pressed, uint[] pov)
    {
        var b = new byte[ShimSnapshotAbi.SampleSize]; const string s = "dbce_shim_snapshot_sample";
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(s, "struct_size")), ShimSnapshotAbi.SampleSize);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(s, "abi")), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(s, "index")), (uint)index);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(s, "epoch")), epoch);
        BinaryPrimitives.WriteUInt64LittleEndian(b.AsSpan(At(s, "device_seq")), deviceSeq);
        BinaryPrimitives.WriteInt64LittleEndian(b.AsSpan(At(s, "read_start_us")), 1_000_100);
        BinaryPrimitives.WriteInt64LittleEndian(b.AsSpan(At(s, "read_end_us")), 1_000_300);
        b[At(s, "poll_call")] = (byte)poll; b[At(s, "state_call")] = (byte)state;
        b[At(s, "valid")] = (byte)(valid ? 1 : 0); b[At(s, "stale")] = (byte)(stale ? 1 : 0); b[At(s, "ended")] = (byte)(ended ? 1 : 0);
        for (int a = 0; a < 8; a++) BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(At(s, "axes") + a * 4), axes[a]);
        foreach (int button in pressed) b[At(s, "buttons") + button] = 0x80;
        b[At(s, "buttons") + 120] = 0x7F;   // a set byte without 0x80 is up
        for (int p = 0; p < 4; p++) BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(At(s, "pov") + p * 4), pov[p]);
        return b;
    }

    static readonly int[] Axes = { 32767, 0, 65535, -100, 40000, 1, 2, 3 };
    static readonly uint[] Pov = { 9000, 0xFFFFFFFF, 18000, 0 };

    // The wheel (row 0) is opened with a path; the pedals (row 1) never opened. One valid wheel read.
    static (byte[] Header, byte[] Devices, byte[] Samples) Frame(Action<byte[], byte[], byte[]>? change = null)
    {
        var header = Header(42, 2, 1);
        var devices = Device(0, 3, ShimSnapshotAbi.FlagOpened | ShimSnapshotAbi.FlagPath, 0b1001_1111, 0b0001_1111, 0b0101, new[] { 0, 5, 120, 127 }, Wheel, WheelProduct)
            .Concat(Device(1, 0, 0, 0, 0, 0, Array.Empty<int>(), Pedals, PedalProduct)).ToArray();
        var samples = Sample(0, 3, 77, ShimSnapshotCall.Ok, ShimSnapshotCall.Ok, true, false, false, Axes, new[] { 5, 6, 127 }, Pov);
        change?.Invoke(header, devices, samples);
        return (header, devices, samples);
    }

    [Fact]
    public void DecodeReadsEveryFieldOfTheCLayout()
    {
        var (h, d, s) = Frame();
        var snap = ShimSnapshot.Decode(h, d, s);
        Assert.Equal(42UL, snap.FrameSeq); Assert.Equal(1_000_000, snap.TickUs); Assert.Equal(1_000_400, snap.BoundaryUs); Assert.Equal(100_000, snap.FreshUs);
        Assert.Equal(2, snap.Devices.Count); Assert.Single(snap.Samples);
        var wheel = snap.Devices[0];
        Assert.True(wheel.Opened); Assert.True(wheel.HasPath); Assert.Equal(3u, wheel.Epoch); Assert.Equal(new Guid(Wheel), wheel.Instance); Assert.Equal(new Guid(WheelProduct), wheel.Product);
        Assert.Equal(0b1001_1111u, wheel.AxisMask); Assert.Equal(0b0001_1111u, wheel.RangeMask); Assert.Equal(0b0101u, wheel.PovMask);
        Assert.True(wheel.HasButton(5)); Assert.True(wheel.HasButton(127)); Assert.False(wheel.HasButton(6)); Assert.False(wheel.HasButton(128));
        Assert.Equal(-512, wheel.Minimum(3)); Assert.Equal(511, wheel.Maximum(3));
        var pedals = snap.Devices[1];
        Assert.False(pedals.Opened); Assert.False(pedals.HasPath); Assert.Equal(0u, pedals.Epoch); Assert.Equal(new Guid(Pedals), pedals.Instance);
        var read = snap.Samples[0];
        Assert.Equal(0u, read.Index); Assert.Equal(3u, read.Epoch); Assert.Equal(77UL, read.DeviceSeq); Assert.Equal(1_000_100, read.ReadStartUs); Assert.Equal(1_000_300, read.ReadEndUs);
        Assert.True(read.Valid); Assert.False(read.Stale); Assert.False(read.Ended); Assert.Equal(ShimSnapshotCall.Ok, read.PollCall);
        Assert.Equal(-100, read.Axis(3)); Assert.Equal(0x80, read.Button(6)); Assert.Equal(0x7F, read.Button(120)); Assert.Equal(0xFFFFFFFFu, read.Pov(1));
    }

    public static IEnumerable<object[]> Refusals() => new (string, Action<byte[], byte[], byte[]>)[] {
        ("header ABI", (h, d, s) => h[4] = 2),
        ("header size field", (h, d, s) => h[0] = 55),
        ("device size field", (h, d, s) => h[8] = 191),
        ("sample size field", (h, d, s) => h[12] = 223),
        ("device count above 64", (h, d, s) => BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(48), 65)),
        ("more samples than rows", (h, d, s) => BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(52), 3)),
        ("device row index", (h, d, s) => d[192 + 8] = 0),
        ("device row ABI", (h, d, s) => d[4] = 0),
        ("unknown device flag", (h, d, s) => d[16] |= 4),
        ("upper-case instance", (h, d, s) => Encoding.ASCII.GetBytes(Wheel.ToUpperInvariant()).CopyTo(d, 112)),
        ("unterminated product", (h, d, s) => Array.Fill(d, (byte)'a', 152, 40)),
        ("empty instance", (h, d, s) => Array.Clear(d, 112, 40)),
        ("sample row out of range", (h, d, s) => s[8] = 2),
        ("sample size field", (h, d, s) => s[0] = 0),
        ("valid with a pending poll", (h, d, s) => s[40] = (byte)ShimSnapshotCall.Pending),
        ("unknown call value", (h, d, s) => s[41] = 9),
        ("stale without valid", (h, d, s) => { s[42] = 0; s[43] = 1; }),
        ("flag byte above 1", (h, d, s) => s[44] = 2),
    }.Select(c => new object[] { c.Item1, c.Item2 });

    [Theory]
    [MemberData(nameof(Refusals))]
    public void DecodeRefusesInsteadOfGuessing(string why, Action<byte[], byte[], byte[]> change)
    {
        var (h, d, s) = Frame(change);
        Assert.False(string.IsNullOrEmpty(why));
        Assert.Throws<InvalidDataException>(() => ShimSnapshot.Decode(h, d, s));
    }

    [Fact]
    public void DecodeRefusesRepeatedRowsAndShortBuffers()
    {
        var (h, d, s) = Frame();
        BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(52), 2);
        Assert.Throws<InvalidDataException>(() => ShimSnapshot.Decode(h, d, s.Concat(s).ToArray()));   // two reads of row 0
        (h, d, s) = Frame();
        Assert.Throws<InvalidDataException>(() => ShimSnapshot.Decode(h, d.Take(191).ToArray(), s));
        Assert.Throws<InvalidDataException>(() => ShimSnapshot.Decode(h.Take(55).ToArray(), d, s));
    }

    // A fake dbce_shim_snapshot: the header is filled for the right seq even when a capacity is short (then BAD_ARGUMENT
    // and the arrays untouched), as the native contract says.
    sealed class FakeNative
    {
        readonly (byte[] Header, byte[] Devices, byte[] Samples) frame;
        internal int Calls, Result;
        internal readonly List<(int Devices, int Samples)> Capacities = new();
        internal FakeNative((byte[], byte[], byte[]) frame, int result = 0) { this.frame = frame; Result = result; }
        internal int Call(ulong seq, byte[] header, byte[] devices, int deviceCapacity, byte[] samples, int sampleCapacity)
        {
            Calls++; Capacities.Add((deviceCapacity, sampleCapacity));
            Assert.Equal(ShimSnapshotAbi.HeaderSize, BitConverter.ToInt32(header, 0));
            Assert.Equal(deviceCapacity * ShimSnapshotAbi.DeviceSize, devices.Length); Assert.Equal(sampleCapacity * ShimSnapshotAbi.SampleSize, samples.Length);
            if (Result != 0) return Result;
            if (seq != BitConverter.ToUInt64(frame.Header, 16)) return ShimSnapshotAbi.Stale;
            frame.Header.CopyTo(header, 0);
            int needDevices = frame.Devices.Length / ShimSnapshotAbi.DeviceSize, needSamples = frame.Samples.Length / ShimSnapshotAbi.SampleSize;
            if (deviceCapacity < needDevices || sampleCapacity < needSamples) return ShimSnapshotAbi.BadArgument;
            frame.Devices.CopyTo(devices, 0); frame.Samples.CopyTo(samples, 0);
            return ShimSnapshotAbi.Ok;
        }
    }

    static (byte[], byte[], byte[]) Big(int rows)
    {
        var devices = Enumerable.Range(0, rows).SelectMany(i => Device(i, 1, ShimSnapshotAbi.FlagOpened, 1, 1, 0, new[] { 0 },
            "{d71b8350-61b7-11f1-8001-4445535400" + i.ToString("x2") + "}", WheelProduct)).ToArray();
        var samples = Enumerable.Range(0, rows).SelectMany(i => Sample(i, 1, (ulong)i + 1, ShimSnapshotCall.Ok, ShimSnapshotCall.Ok, true, false, false, Axes, new[] { 0 }, Pov)).ToArray();
        return (Header(9, rows, rows), devices, samples);
    }

    [Fact]
    public void TakeRetriesOnceWithTheReportedCountsThenDecodes()
    {
        var native = new FakeNative(Big(6));
        Assert.Equal(ShimSnapshotAbi.Ok, ShimSnapshot.Take(native.Call, 9, out var snap));
        Assert.Equal(2, native.Calls); Assert.Equal((6, 6), native.Capacities[1]);
        Assert.Equal(6, snap!.Devices.Count); Assert.Equal(6UL, snap.Samples[5].DeviceSeq);
        native = new FakeNative(Frame());
        Assert.Equal(ShimSnapshotAbi.Ok, ShimSnapshot.Take(native.Call, 42, out snap));
        Assert.Equal(1, native.Calls); Assert.Equal(2, snap!.Devices.Count);
    }

    [Theory]
    [InlineData(ShimSnapshotAbi.Stale)]
    [InlineData(ShimSnapshotAbi.Refused)]
    [InlineData(ShimSnapshotAbi.WrongThread)]
    [InlineData(ShimSnapshotAbi.BadArgument)]   // header not filled (a size or handle error): no retry
    public void TakePassesOtherResultsThroughWithNoSnapshot(int result)
    {
        var native = new FakeNative(Frame(), result);
        Assert.Equal(result, ShimSnapshot.Take(native.Call, 42, out var snap));
        Assert.Null(snap); Assert.Equal(1, native.Calls);
        native = new FakeNative(Frame());
        Assert.Equal(ShimSnapshotAbi.Stale, ShimSnapshot.Take(native.Call, 41, out snap));   // an older frame's seq
        Assert.Null(snap); Assert.Equal(1, native.Calls);
    }

    [Fact]
    public void TakeRefusesCountsAboveTheDeviceLimit()
    {
        var (h, d, s) = Big(5);
        BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(48), 65);
        var native = new FakeNative((h, d, s));
        Assert.Equal(ShimSnapshotAbi.BadArgument, ShimSnapshot.Take(native.Call, 9, out var snap));
        Assert.Null(snap); Assert.Equal(1, native.Calls);
    }

    [Fact]
    public void CaptureValuesApplyTheCapsAndCenteredPov()
    {
        var snap = ShimSnapshot.Decode(Frame().Header, Frame().Devices, Frame().Samples);
        ShimSnapshot.CaptureValues(snap.Devices[0], snap.Samples[0], out var axes, out var buttons, out var hats, out var min, out var max);
        Assert.Equal(new[] { 32767, 0, 65535, -100, 40000, 0, 0, 0 }, axes);      // axis 7 has no range bit; 5 and 6 are absent
        Assert.Equal(new[] { 0, 0, 0, -512, 0, 0, 0, 0 }, min); Assert.Equal(new[] { 65535, 65535, 65535, 511, 65535, 0, 0, 0 }, max);
        Assert.True(buttons[5]); Assert.True(buttons[127]);
        Assert.False(buttons[6]);    // pressed but not in button_mask
        Assert.False(buttons[0]);    // present but up
        Assert.False(buttons[120]);  // present, byte set without 0x80
        Assert.Equal(new[] { 9000, -1, 18000, -1 }, hats);   // 1 centered; 3 not in pov_mask
        Assert.Throws<InvalidOperationException>(() => ShimSnapshot.CaptureValues(snap.Devices[1], snap.Samples[0], out _, out _, out _, out _, out _));
        var (h, d, s) = Frame((h2, d2, s2) => { s2[40] = (byte)ShimSnapshotCall.Pending; s2[41] = (byte)ShimSnapshotCall.NotCalled; s2[42] = 0; });
        var pending = ShimSnapshot.Decode(h, d, s);
        Assert.False(pending.Samples[0].Valid);
        Assert.Throws<InvalidOperationException>(() => ShimSnapshot.CaptureValues(pending.Devices[0], pending.Samples[0], out _, out _, out _, out _, out _));
    }

    [Fact]
    public void ValidateChecksTheLoadedDllsAbiAndSizes()
    {
        ShimSnapshotAbi.Validate(1, 56, 192, 224);
        Assert.Throws<InvalidDataException>(() => ShimSnapshotAbi.Validate(2, 56, 192, 224));
        Assert.Throws<InvalidDataException>(() => ShimSnapshotAbi.Validate(1, 56, 192, 232));
    }
}
