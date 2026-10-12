using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using Windows.Graphics.Imaging;
using Windows.Storage.Streams;
using WinRT;

internal sealed class CaptureMetrics(CaptureOptions options) : IDisposable
{
    internal const int MaxRows = 12000;
    internal const long RepresentativeBudget = 256L * 1024 * 1024;
    internal const double GapMilliseconds = 50;
    private readonly List<MeasuredFrame> _rows = new();
    private Representatives<MeasuredBitmap>? _images;
    private long _frameBytes, _lastTimestamp;
    private bool _leftBright, _rightBright;
    internal int Count => _rows.Count;
    internal int TimestampGaps { get; private set; }
    internal int NonIncreasingTimestamps { get; private set; }
    internal int SideThresholdCrossings { get; private set; }
    internal double MaximumGapMilliseconds { get; private set; }
    internal object Retention => new { budgetBytes = RepresentativeBudget, capacity = _images?.Capacity ?? 0,
        thresholdCandidates = _images?.ThresholdCandidates ?? 0, omittedThresholds = _images?.OmittedThresholds ?? 0 };

    [ComImport, Guid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private unsafe interface IMemoryBufferByteAccess { void GetBuffer(out byte* buffer, out uint capacity); }

    // Takes bitmap ownership on every path. A kept last bitmap replaces the old
    // one without copying it; no PNG work or file I/O occurs during observation.
    internal unsafe void Observe(SoftwareBitmap bitmap, long timestamp, DateTime arrivedUtc, double arrivedSeconds)
    {
        try
        {
            if (Count >= MaxRows) throw new IOException("Metric row limit exceeded.");
            if (bitmap.BitmapPixelFormat != BitmapPixelFormat.Bgra8) throw new IOException("Metrics require BGRA8 pixels.");
            var timer = Stopwatch.StartNew();
            ThirdMetrics[] thirds; uint capacity;
            using (var buffer = bitmap.LockBuffer(BitmapBufferAccessMode.Read))
            using (var reference = buffer.CreateReference())
            {
                reference.As<IMemoryBufferByteAccess>().GetBuffer(out var data, out capacity);
                if (data == null || capacity > int.MaxValue) throw new IOException("Pixel buffer unavailable or too large.");
                var plane = buffer.GetPlaneDescription(0);
                thirds = PixelMetrics.Measure(new ReadOnlySpan<byte>(data, (int)capacity), bitmap.PixelWidth, bitmap.PixelHeight, plane.Stride, plane.StartIndex);
            }
            double analysisMs = timer.Elapsed.TotalMilliseconds;
            double? delta = Count == 0 ? null : (timestamp - _lastTimestamp) / (double)TimeSpan.TicksPerMillisecond;
            if (delta > GapMilliseconds) TimestampGaps++;
            if (delta <= 0) NonIncreasingTimestamps++;
            if (delta.HasValue) MaximumGapMilliseconds = Math.Max(MaximumGapMilliseconds, delta.Value);
            bool left = thirds[0].WhiteFraction >= options.WhiteFraction, right = thirds[2].WhiteFraction >= options.WhiteFraction;
            bool crossing = Count > 0 && (left != _leftBright || right != _rightBright);
            if (crossing) SideThresholdCrossings++;
            var row = new MeasuredFrame(Count + 1, timestamp, arrivedUtc, arrivedSeconds, DateTime.UtcNow,
                bitmap.PixelWidth, bitmap.PixelHeight, delta, analysisMs, thirds[0], thirds[1], thirds[2], left, right, crossing);
            _rows.Add(row); _lastTimestamp = timestamp; _leftBright = left; _rightBright = right;
            if (options.NoPng) { bitmap.Dispose(); return; }
            if (_images == null) { _frameBytes = capacity; _images = new(options.MaxFrames, _frameBytes, RepresentativeBudget); }
            if (capacity != _frameBytes) throw new IOException("Pixel buffer size changed during capture.");
            _images.Observe(new MeasuredBitmap(bitmap, row), crossing);
        }
        catch { bitmap.Dispose(); throw; }
    }

    internal async Task SaveImages(string output, List<SavedFrame> saved, Action verifyTarget)
    {
        if (_images == null) return;
        foreach (var image in _images.Frames())
        {
            verifyTarget();
            using var stream = new InMemoryRandomAccessStream();
            var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, stream);
            var bitmap = image.Bitmap; encoder.SetSoftwareBitmap(bitmap);
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
            verifyTarget();
            string name = $"frame-{image.Row.Index:D5}.png";
            using (var file = new FileStream(Path.Combine(output, name), FileMode.CreateNew, FileAccess.Write, FileShare.None)) file.Write(bytes);
            image.Row.Image = name;
            saved.Add(new(name, image.Row.SystemRelativeTimeTicks, image.Row.ArrivedUtc, image.Row.ArrivalElapsedSeconds, DateTime.UtcNow,
                bitmap.PixelWidth, bitmap.PixelHeight, width, height, Convert.ToHexString(SHA256.HashData(bytes))));
        }
    }

    internal void WriteRows(string output)
    {
        using var file = new StreamWriter(new FileStream(Path.Combine(output, "metrics.jsonl"), FileMode.CreateNew, FileAccess.Write, FileShare.None));
        foreach (var row in _rows) file.WriteLine(JsonSerializer.Serialize(row));
    }
    public void Dispose() => _images?.Dispose();
}
internal sealed record MeasuredFrame(int Index, long SystemRelativeTimeTicks, DateTime ArrivedUtc, double ArrivalElapsedSeconds,
    DateTime ProcessedUtc, int Width, int Height, double? SystemDeltaMilliseconds, double AnalysisMilliseconds,
    ThirdMetrics Left, ThirdMetrics Centre, ThirdMetrics Right, bool LeftAboveWhiteThreshold, bool RightAboveWhiteThreshold, bool SideThresholdCrossing)
{
    public string? Image { get; set; }
}
internal sealed class MeasuredBitmap(SoftwareBitmap bitmap, MeasuredFrame row) : IDisposable
{
    internal SoftwareBitmap Bitmap { get; } = bitmap;
    internal MeasuredFrame Row { get; } = row;
    public void Dispose() => Bitmap.Dispose();
}
