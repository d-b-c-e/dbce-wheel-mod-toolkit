// Pure BGRA8 code-value diagnostics. Sampling is spatially stratified, not an
// exhaustive pixel maximum or a physical/linear-light luminance measurement.
internal static class PixelMetrics
{
    internal const int Columns = 256, Rows = 144, WhiteCode = 235;
    internal static ThirdMetrics[] Measure(ReadOnlySpan<byte> bytes, int width, int height, int stride, int start = 0)
    {
        if (width <= 0 || height <= 0 || start < 0 || stride < (long)width * 4 ||
            start + (long)(height - 1) * stride + (long)width * 4 > bytes.Length)
            throw new ArgumentException("Invalid BGRA8 pixel buffer dimensions, stride or bounds.");
        var result = new ThirdMetrics[3];
        for (int third = 0; third < 3; third++)
        {
            int left = (int)((long)width * third / 3), right = (int)((long)width * (third + 1) / 3);
            int columns = Math.Min(Columns, right - left), rows = Math.Min(Rows, height);
            long sum = 0; int maximum = 0, white = 0;
            for (int row = 0; row < rows; row++)
            {
                int y = (int)((2L * row + 1) * height / (2L * rows));
                for (int column = 0; column < columns; column++)
                {
                    int x = left + (int)((2L * column + 1) * (right - left) / (2L * columns));
                    int at = checked(start + y * stride + x * 4);
                    int b = bytes[at], g = bytes[at + 1], r = bytes[at + 2];
                    int luma = (54 * r + 183 * g + 19 * b + 128) >> 8;
                    sum += luma; maximum = Math.Max(maximum, luma);
                    if (r >= WhiteCode && g >= WhiteCode && b >= WhiteCode) white++;
                }
            }
            int samples = columns * rows;
            result[third] = new(left, right, columns, rows, samples, samples == 0 ? null : (double)sum / samples,
                samples == 0 ? null : maximum, samples == 0 ? null : (double)white / samples);
        }
        return result;
    }
}
internal sealed record ThirdMetrics(int Left, int RightExclusive, int Columns, int Rows, int Samples,
    double? Mean, int? Max, double? WhiteFraction);

// Owns only first, current last, and threshold-transition representatives. The
// reserve for a future last frame prevents transitions from consuming its slot.
internal sealed class Representatives<T>(int maxFrames, long bytesPerFrame, long budget) : IDisposable where T : class, IDisposable
{
    private readonly int _slots = maxFrames >= 2 && bytesPerFrame > 0 && budget / bytesPerFrame >= 2
        ? (int)Math.Min(maxFrames, budget / bytesPerFrame) : throw new ArgumentException("Source frames exceed representative memory budget; use --no-png.");
    private readonly List<T> _crossings = new();
    private T? _first, _last;
    internal int ThresholdCandidates { get; private set; }
    internal int OmittedThresholds { get; private set; }
    internal int Capacity => _slots;
    internal void Observe(T frame, bool crossing)
    {
        if (_first == null) { _first = _last = frame; return; }
        if (_last != null && !ReferenceEquals(_last, _first) && !_crossings.Any(f => ReferenceEquals(f, _last))) _last.Dispose();
        _last = frame;
        if (!crossing) return;
        ThresholdCandidates++;
        if (_crossings.Count < _slots - 2) _crossings.Add(frame); else OmittedThresholds++;
    }
    internal IReadOnlyList<T> Frames()
    {
        var result = new List<T>();
        if (_first != null) result.Add(_first);
        result.AddRange(_crossings);
        if (_last != null && !result.Any(f => ReferenceEquals(f, _last))) result.Add(_last);
        return result;
    }
    public void Dispose() { foreach (var frame in Frames()) frame.Dispose(); _first = _last = null; _crossings.Clear(); }
}
