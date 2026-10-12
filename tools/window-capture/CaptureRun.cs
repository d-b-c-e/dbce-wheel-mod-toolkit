using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX;
using Windows.Graphics.Imaging;
using Windows.Storage.Streams;

internal static class CaptureRun
{
    internal static async Task<int> Run(CaptureOptions options)
    {
        using var process = FindProcess(options.Process);
        if (process == null) { Console.Error.WriteLine("no process " + options.Process); return 3; }
        int pid = process.Id; var processStart = process.StartTime.ToUniversalTime();
        var windows = Windows32.List((uint)pid);
        if (options.List) { Console.WriteLine(JsonSerializer.Serialize(new { process = pid, startedUtc = processStart, windows })); return 0; }
        var chosen = options.Window.HasValue ? windows.SingleOrDefault(w => w.Handle == options.Window.Value) : windows.OrderByDescending(w => w.Area).FirstOrDefault();
        if (chosen == null) { Console.Error.WriteLine("no window for " + options.Process); return 3; }
        var hwnd = new IntPtr(chosen.Handle);
        void VerifyTarget()
        {
            process.Refresh();
            if (process.HasExited || process.StartTime.ToUniversalTime() != processStart || !Windows32.SameWindow(hwnd, (uint)pid, chosen.Class))
                throw new IOException("Process start/window identity or visibility changed; capture refused.");
        }
        VerifyTarget();
        string output = Path.GetFullPath(options.Output);
        if (options.Burst) Windows32.FreshFolder(output);
        var frames = new List<SavedFrame>();
        var clock = Stopwatch.StartNew(); var startedUtc = DateTime.UtcNow;
        string endReason = "failed", error = "";
        using var inbox = new FrameInbox<CapturedFrame>(options.Burst ? 2 : 1);
        try
        {
            var item = Capture.ItemForWindow(hwnd);
            var captureSize = item.Size;
            using var device = Capture.CreateDevice();
            using var pool = Direct3D11CaptureFramePool.CreateFreeThreaded(device, DirectXPixelFormat.B8G8R8A8UIntNormalized, options.Burst ? 4 : 2, captureSize);
            var session = pool.CreateCaptureSession(item);
            int sessionClosed = 0;
            var callbacks = new object(); bool accepting = true;
            void StopCapture()
            {
                lock (callbacks) { accepting = false; inbox.Stop(); }
                if (Interlocked.Exchange(ref sessionClosed, 1) != 0) return;
                try { session.Dispose(); }
                catch (Exception e) { inbox.Fail("Capture shutdown failed: " + e.Message); }
            }
            try
            {
            try { session.IsBorderRequired = false; } catch { }
            try { session.IsCursorCaptureEnabled = false; } catch { }
            void Arrived(Direct3D11CaptureFramePool source, object _)
            {
                lock (callbacks)
                {
                    if (!accepting) return;
                    Direct3D11CaptureFrame? frame = null;
                    try
                    {
                        frame = source.TryGetNextFrame();
                        if (frame == null) { inbox.Empty(); return; }
                        if (clock.Elapsed.TotalSeconds >= options.Duration) inbox.Stop();
                        var queued = new CapturedFrame(frame, DateTime.UtcNow, clock.Elapsed.TotalSeconds);
                        frame = null; inbox.Offer(queued);
                    }
                    catch (Exception e) { frame?.Dispose(); inbox.Fail("Frame delivery failed: " + e.Message); }
                }
            }
            void Closed(GraphicsCaptureItem _, object __) { lock (callbacks) if (accepting) inbox.Fail("Capture target closed or was replaced."); }
            pool.FrameArrived += Arrived; item.Closed += Closed;
            // Deadline closes the session and disposes queued frames even when
            // PNG encoding is slow. Pool/device remain alive for the one current
            // surface copy; at most its encoding/write can finish later.
            clock.Restart(); startedUtc = DateTime.UtcNow;
            using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(options.Duration));
            using var deadlineRegistration = deadline.Token.Register(StopCapture);
            try
            {
                session.StartCapture();
                double lastFrameAt = 0;
                while (frames.Count < options.MaxFrames && clock.Elapsed.TotalSeconds < options.Duration)
                {
                    VerifyTarget();
                    if (inbox.Failure is { } failed) throw new IOException(failed);
                    using var captured = await inbox.Take(100);
                    if (captured == null)
                    {
                        if (clock.Elapsed.TotalSeconds - lastFrameAt >= 5) throw new IOException("No capture frame delivered for 5 seconds.");
                        continue;
                    }
                    lastFrameAt = captured.ArrivalElapsedSeconds;
                    if (captured.Frame.ContentSize.Width != captureSize.Width || captured.Frame.ContentSize.Height != captureSize.Height)
                        throw new IOException("Capture size changed; start a new burst for the new window size.");
                    long timestamp = captured.Frame.SystemRelativeTime.Ticks;
                    using var bitmap = await SoftwareBitmap.CreateCopyFromSurfaceAsync(captured.Frame.Surface, BitmapAlphaMode.Ignore);
                    captured.Dispose(); // Release the WGC surface before slower PNG encoding.
                    using var stream = new InMemoryRandomAccessStream();
                    var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, stream);
                    encoder.SetSoftwareBitmap(bitmap);
                    int width = bitmap.PixelWidth, height = bitmap.PixelHeight;
                    if (options.Width > 0 && options.Width < width)
                    {
                        height = Math.Max(1, (int)(height * (long)options.Width / width)); width = options.Width;
                        encoder.BitmapTransform.ScaledWidth = (uint)width; encoder.BitmapTransform.ScaledHeight = (uint)height;
                        encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Fant;
                    }
                    await encoder.FlushAsync();
                    var bytes = new byte[stream.Size];
                    using (var reader = new DataReader(stream.GetInputStreamAt(0))) { await reader.LoadAsync((uint)stream.Size); reader.ReadBytes(bytes); }
                    VerifyTarget(); // Required immediately before every image save.
                    if (inbox.Failure is { } ended) throw new IOException(ended);
                    string file = options.Burst ? $"frame-{frames.Count + 1:D4}.png" : Path.GetFileName(output);
                    using (var destination = new FileStream(options.Burst ? Path.Combine(output, file) : output,
                        options.Burst ? FileMode.CreateNew : FileMode.Create, FileAccess.Write, FileShare.None)) destination.Write(bytes);
                    frames.Add(new(file, timestamp, captured.ArrivedUtc, captured.ArrivalElapsedSeconds, DateTime.UtcNow,
                        bitmap.PixelWidth, bitmap.PixelHeight, width, height, Convert.ToHexString(SHA256.HashData(bytes))));
                }
                VerifyTarget();
                if (inbox.Failure is { } finalFailure) throw new IOException(finalFailure);
                if (frames.Count == 0) throw new IOException("No frame saved within the capture duration.");
                endReason = frames.Count == options.MaxFrames ? "frame-limit" : "duration";
            }
            finally
            {
                lock (callbacks) { accepting = false; inbox.Stop(); }
                pool.FrameArrived -= Arrived; item.Closed -= Closed;
                StopCapture();
                // Registration disposal waits for a deadline callback before
                // using declarations dispose the pool and device.
            }
            if (inbox.Failure is { } shutdownFailure) throw new IOException(shutdownFailure);
            }
            finally { StopCapture(); }
        }
        catch (Exception e) { endReason = "failed"; error = e.Message; Console.Error.WriteLine(error); }
        finally { inbox.Stop(); }
        var counts = inbox.Counts;
        if (options.Burst)
        {
            var manifest = new { schema = "dbce.window-burst@1", process = pid, processStartedUtc = processStart, window = chosen,
                startedUtc, endedUtc = DateTime.UtcNow, requestedDurationSeconds = options.Duration, maxFrames = options.MaxFrames,
                status = error.Length == 0 ? "complete" : "failed", endReason, error, saved = frames.Count, counts,
                skippedObservedFrames = counts.Received - frames.Count, captureServiceDroppedFrames = (long?)null,
                coverage = "Sampled frames only. Windows may omit frames before delivery; no claim about blanks between saved frames.", frames };
            using var file = new FileStream(Path.Combine(output, "capture.json"), FileMode.CreateNew, FileAccess.Write, FileShare.None);
            JsonSerializer.Serialize(file, manifest, new JsonSerializerOptions { WriteIndented = true });
        }
        if (!options.Burst && frames.Count == 1)
            Console.WriteLine($"{options.Output} ({frames[0].SourceWidth}x{frames[0].SourceHeight} window 0x{chosen.Handle:X} at {chosen.Left},{chosen.Top} of {process.ProcessName} {pid})");
        else Console.WriteLine($"{output}: {frames.Count} saved; {counts.Received - frames.Count} observed frames skipped; {endReason}");
        return error.Length == 0 ? 0 : 4;
    }
    private static Process? FindProcess(string name)
    {
        if (int.TryParse(name, out int pid)) return Process.GetProcessById(pid);
        var all = Process.GetProcessesByName(name);
        var newest = all.OrderByDescending(p => p.StartTime).FirstOrDefault();
        foreach (var process in all) if (!ReferenceEquals(process, newest)) process.Dispose();
        return newest;
    }
}
internal sealed class CapturedFrame(Direct3D11CaptureFrame frame, DateTime arrivedUtc, double elapsed) : IDisposable
{
    private Direct3D11CaptureFrame? _frame = frame;
    internal Direct3D11CaptureFrame Frame => _frame ?? throw new ObjectDisposedException(nameof(CapturedFrame));
    internal DateTime ArrivedUtc { get; } = arrivedUtc;
    internal double ArrivalElapsedSeconds { get; } = elapsed;
    public void Dispose() => Interlocked.Exchange(ref _frame, null)?.Dispose();
}
internal sealed record SavedFrame(string File, long SystemRelativeTimeTicks, DateTime ArrivedUtc, double ArrivalElapsedSeconds,
    DateTime SavedUtc, int SourceWidth, int SourceHeight, int Width, int Height, string Sha256);
