using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Dbce.Wheel.Shim;
using Xunit;

namespace Dbce.Wheel.Shim.Tests;

// ShimCandidateNative over fake exports: static UnmanagedCallersOnly functions behind real native function pointers, so
// every call crosses the same cdecl marshalling as the DLL's. (A pointer from GetFunctionPointerForDelegate would come
// back as its original managed delegate, which no DLL export does.) No shim DLL, device or clock. xUnit runs one class's
// tests serially, so the fake state is static and reset per test.
public unsafe class ShimCandidateNativeTests
{
    private static int s_version, s_lists, s_releases, s_selectResult, s_snapshotResult, s_snapshotDevices;
    private static IntPtr s_lastHandle;
    private static string? s_instance, s_product;
    private static string s_report = "";
    private static bool s_unterminated;
    private static ulong s_snapshotSeq;

    private static void Reset()
    {
        s_version = 1; s_lists = s_releases = s_selectResult = 0; s_snapshotResult = ShimSnapshotAbi.Stale; s_snapshotDevices = -1;
        s_lastHandle = IntPtr.Zero; s_instance = s_product = null; s_report = "{\"abi\":1}"; s_unterminated = false; s_snapshotSeq = 0;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })] private static int Version() => s_version;
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })] private static int List(IntPtr handle) { s_lastHandle = handle; s_lists++; return 0; }
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })] private static int Release(IntPtr handle) { s_lastHandle = handle; s_releases++; return 0; }
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Select(IntPtr handle, IntPtr instance, IntPtr product, IntPtr why, int length)
    {
        s_lastHandle = handle; s_instance = Marshal.PtrToStringAnsi(instance); s_product = Marshal.PtrToStringAnsi(product);
        var text = Encoding.UTF8.GetBytes("the profile already binds this device\0");
        Marshal.Copy(text, 0, why, Math.Min(text.Length, length));
        return s_selectResult;
    }
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Report(IntPtr handle, IntPtr text, int length, IntPtr needed)
    {
        var bytes = Encoding.UTF8.GetBytes(s_report + (s_unterminated ? "" : "\0"));
        Marshal.WriteInt32(needed, s_unterminated ? bytes.Length + 1 : bytes.Length);
        if (bytes.Length > length) return 0;   // as the DLL: needed says how much, nothing written
        Marshal.Copy(bytes, 0, text, bytes.Length);
        if (s_unterminated && bytes.Length < length) Marshal.WriteByte(text, bytes.Length, 1);
        return 0;
    }
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Snapshot(IntPtr handle, ulong frameSeq, IntPtr header, IntPtr devices, int deviceCapacity, IntPtr samples, int sampleCapacity)
    { s_lastHandle = handle; s_snapshotSeq = frameSeq; s_snapshotDevices = deviceCapacity; return s_snapshotResult; }

    private static Dictionary<string, IntPtr> Exports() => new()
    {
        ["dbce_shim_candidate_abi_version"] = (IntPtr)(delegate* unmanaged[Cdecl]<int>)&Version,
        ["dbce_shim_candidate_list"] = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, int>)&List,
        ["dbce_shim_candidate_release"] = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, int>)&Release,
        ["dbce_shim_candidate_select"] = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, IntPtr, IntPtr, IntPtr, int, int>)&Select,
        ["dbce_shim_candidate_report"] = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, IntPtr, int, IntPtr, int>)&Report,
        ["dbce_shim_candidate_snapshot"] = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, ulong, IntPtr, IntPtr, int, IntPtr, int, int>)&Snapshot,
    };
    private static Func<string, IntPtr> Resolver(Dictionary<string, IntPtr> exports) => name => exports.TryGetValue(name, out var address) ? address : IntPtr.Zero;

    [Fact]
    public void An_older_dll_or_another_version_offers_no_candidate()
    {
        Reset();
        var exports = Exports(); exports.Remove("dbce_shim_candidate_snapshot");
        Assert.Null(ShimCandidateNative.TryBind(Resolver(exports), out var missing));
        Assert.Contains("dbce_shim_candidate_snapshot", missing);
        s_version = 2;
        Assert.Null(ShimCandidateNative.TryBind(Resolver(Exports()), out var version));
        Assert.Contains("ABI 2", version);
        Assert.Throws<ArgumentNullException>(() => ShimCandidateNative.TryBind(null!, out _));
    }

    [Fact]
    public void Calls_reach_the_exports_with_the_handle_and_exact_identity_text()
    {
        Reset(); s_selectResult = ShimSnapshotAbi.Refused;
        var native = ShimCandidateNative.TryBind(Resolver(Exports()), out var unavailable);
        Assert.NotNull(native); Assert.Null(unavailable);
        var handle = new IntPtr(0x1234);
        Assert.Equal(0, native!.RequestList(handle));
        Assert.Equal(1, s_lists); Assert.Equal(handle, s_lastHandle);
        var instance = Guid.Parse("D71B8351-61B7-11F1-8001-444553540000"); var product = Guid.Parse("0006346F-0000-0000-0000-504944564944");
        Assert.Equal(ShimSnapshotAbi.Refused, native.Select(handle, instance, product, out var why));
        Assert.Equal("{d71b8351-61b7-11f1-8001-444553540000}", s_instance);   // lower-case braced, as the shim requires
        Assert.Equal("{0006346f-0000-0000-0000-504944564944}", s_product);
        Assert.Equal("the profile already binds this device", why);
        Assert.Equal(0, native.Release(handle));
        Assert.Equal(1, s_releases);
    }

    [Fact]
    public void The_report_is_one_complete_text_or_none()
    {
        Reset();
        var native = ShimCandidateNative.TryBind(Resolver(Exports()), out _)!;
        Assert.Equal(0, native.Report(IntPtr.Zero, out var json));
        Assert.Equal("{\"abi\":1}", json);
        s_report = "{\"abi\":1,\"pad\":\"" + new string('x', 40000) + "\"}";
        Assert.Equal(0, native.Report(IntPtr.Zero, out json));
        Assert.Equal(s_report, json);   // a larger report is read again at its stated size
        s_report = "{\"abi\":1}"; s_unterminated = true;
        Assert.Equal(ShimSnapshotAbi.BadArgument, native.Report(IntPtr.Zero, out json));
        Assert.Null(json);
    }

    [Fact]
    public void The_candidate_snapshot_goes_through_the_extension_3_decoder()
    {
        Reset();
        var native = ShimCandidateNative.TryBind(Resolver(Exports()), out _)!;
        var handle = new IntPtr(77);
        Assert.Equal(ShimSnapshotAbi.Stale, ShimSnapshot.Take(native.SnapshotCallFor(handle), 41, out var snapshot));
        Assert.Null(snapshot);
        Assert.Equal(handle, s_lastHandle); Assert.Equal(41UL, s_snapshotSeq); Assert.True(s_snapshotDevices > 0);
    }
}
