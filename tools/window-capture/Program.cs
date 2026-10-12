// dbce-window-capture: saves one frame or a bounded burst of one game window through Windows
// Graphics Capture. It captures only that window's own content (even when other windows
// cover it), never the desktop, and needs no focus. Works for windowed/borderless games;
// exclusive fullscreen may capture black.
//
//   dbce-window-capture <process-name|pid> <out.png> [--width 1920]
//   dbce-window-capture <pid> <fresh-folder> --duration 10 --max-frames 300
using System.Diagnostics;
using System.Runtime.InteropServices;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX;
using Windows.Graphics.DirectX.Direct3D11;
using Windows.Graphics.Imaging;
using Windows.Storage.Streams;
using WinRT;
using System.Text;
using System.Text.Json;

CaptureOptions options;
try { options = CaptureOptions.Parse(args); }
catch (ArgumentException e) { Console.Error.WriteLine(e.Message); return 2; }
try { return await CaptureRun.Run(options); }
catch (Exception e) { Console.Error.WriteLine(e.Message); return 4; }

static class Windows32
{
    delegate bool EnumProc(IntPtr hwnd, IntPtr l);
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateDirectory(string path,IntPtr security);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h,StringBuilder text,int size);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder text,int size);

    public sealed record Window(long Handle,string Title,string Class,int Left,int Top,int Width,int Height) { public long Area=>(long)Width*Height; }
    public static bool OwnedVisible(IntPtr hwnd,uint pid) {GetWindowThreadProcessId(hwnd,out var owner);return IsWindow(hwnd) && owner==pid && IsWindowVisible(hwnd);}
    public static bool SameWindow(IntPtr hwnd,uint pid,string kind)
    { var name=new StringBuilder(256);GetClassName(hwnd,name,name.Capacity);return OwnedVisible(hwnd,pid)&&name.ToString()==kind; }
    public static void FreshFolder(string path)
    {
        // Win32 creates atomically or refuses an existing directory. Never merge
        // a partial burst with older images, even in a simultaneous invocation.
        if (!CreateDirectory(path,IntPtr.Zero)) throw new IOException("Burst folder must be fresh with an existing parent: "+path,
            new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()));
    }

    public static List<Window> List(uint pid)
    {
        var result=new List<Window>();
        EnumWindows((h, _) =>
        {
            GetWindowThreadProcessId(h, out var owner);
            if (owner == pid && IsWindowVisible(h) && GetWindowRect(h, out var r))
            {
                if(r.R>r.L && r.B>r.T)
                {
                    var title=new StringBuilder(1024);var name=new StringBuilder(256);
                    GetWindowText(h,title,title.Capacity);GetClassName(h,name,name.Capacity);
                    result.Add(new(h.ToInt64(),title.ToString(),name.ToString(),r.L,r.T,r.R-r.L,r.B-r.T));
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}

static class Capture
{
    [ComImport, Guid("3628E81B-3CAC-4C60-B7F4-23CE0E0C3356"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IGraphicsCaptureItemInterop
    {
        IntPtr CreateForWindow([In] IntPtr window, [In] ref Guid iid);
        IntPtr CreateForMonitor([In] IntPtr monitor, [In] ref Guid iid);
    }

    static readonly Guid GraphicsCaptureItemIid = new("79C3F95B-31F7-4EC2-A464-632EF5D30760");
    static readonly Guid DxgiDeviceIid = new("54ec77fa-1377-44e6-8c32-88fd5f44c84c");

    [DllImport("d3d11.dll", ExactSpelling = true)]
    static extern int D3D11CreateDevice(IntPtr adapter, int driverType, IntPtr software, uint flags, IntPtr levels, uint numLevels, uint sdkVersion, out IntPtr device, out int level, out IntPtr context);

    [DllImport("d3d11.dll", ExactSpelling = true)]
    static extern int CreateDirect3D11DeviceFromDXGIDevice(IntPtr dxgiDevice, out IntPtr graphicsDevice);

    public static GraphicsCaptureItem ItemForWindow(IntPtr hwnd)
    {
        var factory = ActivationFactory.Get("Windows.Graphics.Capture.GraphicsCaptureItem");
        var interop = factory.AsInterface<IGraphicsCaptureItemInterop>();
        var iid = GraphicsCaptureItemIid;
        IntPtr abi = interop.CreateForWindow(hwnd, ref iid);
        try { return GraphicsCaptureItem.FromAbi(abi); } finally { Marshal.Release(abi); }
    }

    public static IDirect3DDevice CreateDevice()
    {
        // D3D_DRIVER_TYPE_HARDWARE, D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_SDK_VERSION.
        Marshal.ThrowExceptionForHR(D3D11CreateDevice(IntPtr.Zero, 1, IntPtr.Zero, 0x20, IntPtr.Zero, 0, 7, out var d3d, out _, out var ctx));
        try
        {
            var iid = DxgiDeviceIid;
            Marshal.ThrowExceptionForHR(Marshal.QueryInterface(d3d, ref iid, out var dxgi));
            try
            {
                Marshal.ThrowExceptionForHR(CreateDirect3D11DeviceFromDXGIDevice(dxgi, out var inspectable));
                try { return MarshalInterface<IDirect3DDevice>.FromAbi(inspectable); } finally { Marshal.Release(inspectable); }
            }
            finally { Marshal.Release(dxgi); }
        }
        finally { Marshal.Release(ctx); Marshal.Release(d3d); }
    }
}
