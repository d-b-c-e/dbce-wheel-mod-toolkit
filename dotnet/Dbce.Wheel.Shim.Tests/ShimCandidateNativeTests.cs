using System.Runtime.InteropServices;
using System.Text;
using Dbce.Wheel.Shim;
using Xunit;

namespace Dbce.Wheel.Shim.Tests;

// ShimCandidateNative over fake exports: managed functions behind real native function pointers, so every call crosses
// the same marshalling as the DLL's. No shim DLL, device or clock.
public class ShimCandidateNativeTests
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int FakeInt();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int FakeHandle(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int FakeSelect(IntPtr handle, IntPtr instance, IntPtr product, IntPtr why, int whyLength);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int FakeText(IntPtr handle, IntPtr text, int length, IntPtr needed);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int FakeSnapshot(IntPtr handle, ulong frameSeq, IntPtr header, IntPtr devices, int deviceCapacity, IntPtr samples, int sampleCapacity);

    private sealed class Fake
    {
        public int Version = 1, Lists, Releases, SelectResult, SnapshotResult = ShimSnapshotAbi.Stale;
        public IntPtr LastHandle;
        public string? Instance, Product;
        public string Report = "{\"abi\":1}";
        public bool Unterminated;
        public ulong SnapshotSeq;
        public int SnapshotDevices = -1;
        private readonly List<Delegate> _alive = new();
        public readonly Dictionary<string, IntPtr> Exports = new();
        private void Add(string name, Delegate d) { _alive.Add(d); Exports[name] = Marshal.GetFunctionPointerForDelegate(d); }
        public Fake()
        {
            Add("dbce_shim_candidate_abi_version", new FakeInt(() => Version));
            Add("dbce_shim_candidate_list", new FakeHandle(h => { LastHandle = h; Lists++; return 0; }));
            Add("dbce_shim_candidate_release", new FakeHandle(h => { LastHandle = h; Releases++; return 0; }));
            Add("dbce_shim_candidate_select", new FakeSelect((h, i, p, why, length) =>
            {
                LastHandle = h; Instance = Marshal.PtrToStringAnsi(i); Product = Marshal.PtrToStringAnsi(p);
                var text = Encoding.UTF8.GetBytes("the profile already binds this device\0");
                Marshal.Copy(text, 0, why, Math.Min(text.Length, length));
                return SelectResult;
            }));
            Add("dbce_shim_candidate_report", new FakeText((h, text, length, needed) =>
            {
                var bytes = Encoding.UTF8.GetBytes(Report + (Unterminated ? "" : "\0"));
                Marshal.WriteInt32(needed, Unterminated ? bytes.Length + 1 : bytes.Length);
                if (bytes.Length > length) return 0;   // as the DLL: needed says how much, nothing written
                Marshal.Copy(bytes, 0, text, bytes.Length);
                if (Unterminated && bytes.Length < length) Marshal.WriteByte(text, bytes.Length, 1);
                return 0;
            }));
            Add("dbce_shim_candidate_snapshot", new FakeSnapshot((h, seq, header, devices, deviceCapacity, samples, sampleCapacity) =>
            { LastHandle = h; SnapshotSeq = seq; SnapshotDevices = deviceCapacity; return SnapshotResult; }));
        }
        public IntPtr Resolve(string name) => Exports.TryGetValue(name, out var address) ? address : IntPtr.Zero;
    }

    [Fact]
    public void An_older_dll_or_another_version_offers_no_candidate()
    {
        var fake = new Fake();
        fake.Exports.Remove("dbce_shim_candidate_snapshot");
        Assert.Null(ShimCandidateNative.TryBind(fake.Resolve, out var missing));
        Assert.Contains("dbce_shim_candidate_snapshot", missing);
        fake = new Fake { Version = 2 };
        Assert.Null(ShimCandidateNative.TryBind(fake.Resolve, out var version));
        Assert.Contains("ABI 2", version);
        Assert.Throws<ArgumentNullException>(() => ShimCandidateNative.TryBind(null!, out _));
    }

    [Fact]
    public void Calls_reach_the_exports_with_the_handle_and_exact_identity_text()
    {
        var fake = new Fake { SelectResult = ShimSnapshotAbi.Refused };
        var native = ShimCandidateNative.TryBind(fake.Resolve, out var unavailable);
        Assert.NotNull(native); Assert.Null(unavailable);
        var handle = new IntPtr(0x1234);
        Assert.Equal(0, native!.RequestList(handle));
        Assert.Equal(1, fake.Lists); Assert.Equal(handle, fake.LastHandle);
        var instance = Guid.Parse("D71B8351-61B7-11F1-8001-444553540000"); var product = Guid.Parse("0006346F-0000-0000-0000-504944564944");
        Assert.Equal(ShimSnapshotAbi.Refused, native.Select(handle, instance, product, out var why));
        Assert.Equal("{d71b8351-61b7-11f1-8001-444553540000}", fake.Instance);   // lower-case braced, as the shim requires
        Assert.Equal("{0006346f-0000-0000-0000-504944564944}", fake.Product);
        Assert.Equal("the profile already binds this device", why);
        Assert.Equal(0, native.Release(handle));
        Assert.Equal(1, fake.Releases);
    }

    [Fact]
    public void The_report_is_one_complete_text_or_none()
    {
        var fake = new Fake();
        var native = ShimCandidateNative.TryBind(fake.Resolve, out _)!;
        Assert.Equal(0, native.Report(IntPtr.Zero, out var json));
        Assert.Equal("{\"abi\":1}", json);
        fake.Report = "{\"abi\":1,\"pad\":\"" + new string('x', 40000) + "\"}";
        Assert.Equal(0, native.Report(IntPtr.Zero, out json));
        Assert.Equal(fake.Report, json);   // a larger report is read again at its stated size
        fake.Report = "{\"abi\":1}"; fake.Unterminated = true;
        Assert.Equal(ShimSnapshotAbi.BadArgument, native.Report(IntPtr.Zero, out json));
        Assert.Null(json);
    }

    [Fact]
    public void The_candidate_snapshot_goes_through_the_extension_3_decoder()
    {
        var fake = new Fake();
        var native = ShimCandidateNative.TryBind(fake.Resolve, out _)!;
        var handle = new IntPtr(77);
        Assert.Equal(ShimSnapshotAbi.Stale, ShimSnapshot.Take(native.SnapshotCallFor(handle), 41, out var snapshot));
        Assert.Null(snapshot);
        Assert.Equal(handle, fake.LastHandle); Assert.Equal(41UL, fake.SnapshotSeq); Assert.True(fake.SnapshotDevices > 0);
    }
}
