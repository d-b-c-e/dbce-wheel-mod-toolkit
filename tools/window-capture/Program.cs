// dbce-window-capture: saves one frame of a single game window to PNG through Windows
// Graphics Capture. It captures only that window's own content (even when other windows
// cover it), never the desktop, and needs no focus. Works for windowed/borderless games;
// exclusive fullscreen may capture black.
//
//   dbce-window-capture <process-name|pid> <out.png> [--width 1920]
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

if (args.Length < 2) { Console.Error.WriteLine("usage: dbce-window-capture <process-name|pid> <out.png> [--width N] [--hwnd 0xHEX]\n       dbce-window-capture <process-name|pid> --list"); return 2; }
int width=0; long? requested=null; bool listing=args[1]=="--list";
for(int i=2;i<args.Length;i++)
{
    if(listing || i+1>=args.Length){Console.Error.WriteLine("invalid options");return 2;}
    string option=args[i],value=args[++i];
    if(option=="--width" && int.TryParse(value,out var size) && size>0) width=size;
    else if(option=="--hwnd" && long.TryParse(value.StartsWith("0x",StringComparison.OrdinalIgnoreCase)?value[2..]:value,
        value.StartsWith("0x",StringComparison.OrdinalIgnoreCase)?System.Globalization.NumberStyles.HexNumber:System.Globalization.NumberStyles.None,
        System.Globalization.CultureInfo.InvariantCulture,out var handle) && handle>0) requested=handle;
    else {Console.Error.WriteLine("invalid option: "+option);return 2;}
}
var process = int.TryParse(args[0], out var pid) ? Process.GetProcessById(pid)
    : Process.GetProcessesByName(args[0]).OrderByDescending(p => p.StartTime).FirstOrDefault();
if (process == null) { Console.Error.WriteLine($"no process {args[0]}"); return 3; }
// List the owned windows when separate-display rendering creates equal-sized
// outputs. Largest-window selection alone may capture a side view without HUD.
var windows=Windows32.List((uint)process.Id);
if(listing){Console.WriteLine(JsonSerializer.Serialize(new{process=process.Id,startedUtc=process.StartTime.ToUniversalTime(),windows}));return 0;}
var chosen=requested.HasValue?windows.SingleOrDefault(w=>w.Handle==requested.Value):windows.OrderByDescending(w=>w.Area).FirstOrDefault();
var hwnd=chosen is null?IntPtr.Zero:new IntPtr(chosen.Handle);
if (hwnd == IntPtr.Zero) { Console.Error.WriteLine($"no window for {args[0]}"); return 3; }
if(process.HasExited || !Windows32.OwnedVisible(hwnd,(uint)process.Id)){Console.Error.WriteLine("window ownership changed");return 3;}

var item = Capture.ItemForWindow(hwnd);
var device = Capture.CreateDevice();
using var pool = Direct3D11CaptureFramePool.CreateFreeThreaded(device, DirectXPixelFormat.B8G8R8A8UIntNormalized, 1, item.Size);
using var session = pool.CreateCaptureSession(item);
try { session.IsBorderRequired = false; } catch { }
try { session.IsCursorCaptureEnabled = false; } catch { }
var arrived = new TaskCompletionSource<Direct3D11CaptureFrame>();
pool.FrameArrived += (p, _) => { var f = p.TryGetNextFrame(); if (f != null) arrived.TrySetResult(f); };
session.StartCapture();
var done = await Task.WhenAny(arrived.Task, Task.Delay(5000));
if (done != arrived.Task) { Console.Error.WriteLine("no frame within 5 s"); return 4; }
using var frame = arrived.Task.Result;
var bitmap = await SoftwareBitmap.CreateCopyFromSurfaceAsync(frame.Surface, BitmapAlphaMode.Ignore);
using var stream = new InMemoryRandomAccessStream();
var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, stream);
encoder.SetSoftwareBitmap(bitmap);
if (width > 0 && width < bitmap.PixelWidth)
{
    encoder.BitmapTransform.ScaledWidth = (uint)width;
    encoder.BitmapTransform.ScaledHeight = (uint)(bitmap.PixelHeight * (long)width / bitmap.PixelWidth);
    encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Fant;
}
await encoder.FlushAsync();
var bytes = new byte[stream.Size];
using (var reader = new DataReader(stream.GetInputStreamAt(0))) { await reader.LoadAsync((uint)stream.Size); reader.ReadBytes(bytes); }
if(process.HasExited || !Windows32.OwnedVisible(hwnd,(uint)process.Id)){Console.Error.WriteLine("window ownership changed during capture");return 3;}
File.WriteAllBytes(args[1], bytes);
Console.WriteLine($"{args[1]} ({bitmap.PixelWidth}x{bitmap.PixelHeight} window 0x{chosen!.Handle:X} at {chosen.Left},{chosen.Top} of {process.ProcessName} {process.Id})");
return 0;

static class Windows32
{
    delegate bool EnumProc(IntPtr hwnd, IntPtr l);
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h,StringBuilder text,int size);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder text,int size);

    public sealed record Window(long Handle,string Title,string Class,int Left,int Top,int Width,int Height) { public long Area=>(long)Width*Height; }
    public static bool OwnedVisible(IntPtr hwnd,uint pid) {GetWindowThreadProcessId(hwnd,out var owner);return owner==pid && IsWindowVisible(hwnd);}

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
