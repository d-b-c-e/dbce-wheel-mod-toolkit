int checks = 0;
void Check(bool passed, string message) { checks++; if (!passed) throw new Exception(message); }
void Refuses(string[] args) { bool failed = false; try { CaptureOptions.Parse(args); } catch (ArgumentException) { failed = true; } Check(failed, "invalid options accepted: " + string.Join(" ", args)); }
var single = CaptureOptions.Parse(["42", "one.png", "--width", "1280", "--hwnd", "0x1234"]);
Check(!single.Burst && single.MaxFrames == 1 && single.Width == 1280 && single.Window == 0x1234, "single-frame selection changed");
Check(CaptureOptions.Parse(["game", "--list"]).List, "metadata listing preserved");
var burst = CaptureOptions.Parse(["42", "fresh", "--duration", "30", "--max-frames", "300"]);
Check(burst.Burst && burst.Duration == 30 && burst.MaxFrames == 300, "upper bounds refused");
var countOnly = CaptureOptions.Parse(["42", "fresh", "--max-frames", "5"]);
Check(countOnly.Burst && countOnly.Duration == 5 && countOnly.MaxFrames == 5, "count-only burst lacks duration bound");
Check(CaptureOptions.Parse(["42", "fresh", "--duration", ".1"]).MaxFrames == 300, "duration-only burst lacks frame bound");
foreach (string bad in new[] { "0", "-1", "30.01", "NaN", "Infinity", "1,5" }) Refuses(["42", "fresh", "--duration", bad]);
foreach (string bad in new[] { "0", "-1", "301", "1.2" }) Refuses(["42", "fresh", "--max-frames", bad]);
Refuses(["42", "--list", "--duration", "1"]); Refuses(["42", "fresh", "--duration"]);
Refuses(["42", "fresh", "--width", "0"]); Refuses(["42", "fresh", "--hwnd", "0"]);
Refuses(["42", "fresh", "--duration", "1", "--duration", "2"]);
var metrics = CaptureOptions.Parse(["42", "fresh", "--metrics", "--duration", "30", "--max-frames", "2"]);
Check(metrics.Metrics && metrics.Burst && !metrics.NoPng && metrics.WhiteFraction == .1, "metrics defaults or first/last cap incorrect");
Check(CaptureOptions.Parse(["42", "fresh", "--no-png", "--metrics", "--max-frames", "1", "--white-fraction", ".25"]).NoPng, "metrics-only capture refused");
Refuses(["42", "fresh", "--no-png"]); Refuses(["42", "fresh", "--white-fraction", ".1"]);
Refuses(["42", "fresh", "--metrics", "--metrics"]); Refuses(["42", "fresh", "--metrics", "--max-frames", "1"]);
foreach (string bad in new[] { "0", "-1", "1.01", "NaN", "Infinity" }) Refuses(["42", "fresh", "--metrics", "--white-fraction", bad]);

foreach (int width in new[] { 1, 2, 3, 4, 5, 7, 255, 256, 257, 7681 })
{
    int height = 7, stride = width * 4 + 12, start = 9;
    var pixels = new byte[start + stride * height];
    // Padding is white so a stride/plane-offset error cannot masquerade as black.
    Array.Fill(pixels, (byte)255);
    void Fill(Func<int, byte> shade)
    {
        for (int y = 0; y < height; y++) for (int x = 0; x < width; x++)
        {
            int at = start + y * stride + x * 4;
            pixels[at] = pixels[at + 1] = pixels[at + 2] = shade(x); pixels[at + 3] = 0;
        }
    }
    Fill(_ => 0);
    var thirds = PixelMetrics.Measure(pixels, width, height, stride, start);
    Check(thirds[0].Left == 0 && thirds[2].RightExclusive == width && thirds[0].RightExclusive == thirds[1].Left && thirds[1].RightExclusive == thirds[2].Left, "third boundaries have gap/overlap");
    Check(thirds.Where(t => t.Samples > 0).All(t => t.Mean == 0 && t.Max == 0 && t.WhiteFraction == 0), "black pixels included padding or alpha");
    Check(thirds.Where(t => t.Samples == 0).All(t => t.Mean == null && t.Max == null && t.WhiteFraction == null), "empty third invented data");
    Fill(_ => 255);
    thirds = PixelMetrics.Measure(pixels, width, height, stride, start);
    Check(thirds.Where(t => t.Samples > 0).All(t => t.Mean == 255 && t.Max == 255 && t.WhiteFraction == 1), "all-white sample incorrect");
    Fill(x => x >= width / 3 && x < width * 2L / 3 ? (byte)255 : (byte)0);
    thirds = PixelMetrics.Measure(pixels, width, height, stride, start);
    Check(thirds[0].WhiteFraction is null or 0 && thirds[2].WhiteFraction is null or 0, "centre-white pixels leaked into side metrics");
    Check(thirds[1].WhiteFraction is null or 1, "centre-white boundary omitted");
    Check(thirds.All(t => t.Columns <= 256 && t.Rows == height && t.Samples == t.Columns * t.Rows), "sampling grid bound incorrect");
}
var tall = PixelMetrics.Measure(new byte[12 * 1000], 3, 1000, 12);
Check(tall.All(t => t.Samples == 144), "tall window exceeded row sample bound");
var red = PixelMetrics.Measure([0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255], 3, 1, 12);
Check(red[0].Mean == 54 && red[1].Mean == 182 && red[2].Mean == 19 && red.All(t => t.WhiteFraction == 0), "luminance/channel order or white definition wrong");
var nearWhite = PixelMetrics.Measure([235, 235, 235, 0, 234, 255, 255, 0, 255, 255, 255, 0], 3, 1, 12);
Check(nearWhite[0].WhiteFraction == 1 && nearWhite[1].WhiteFraction == 0 && nearWhite[2].WhiteFraction == 1, "white-code boundary incorrect");
foreach (var dimensions in new[] { (0, 1, 4, 0), (1, 0, 4, 0), (1, 1, 3, 0), (1, 1, 4, -1), (2, 2, 8, 0) })
{
    bool refused = false;
    try { PixelMetrics.Measure(new byte[4], dimensions.Item1, dimensions.Item2, dimensions.Item3, dimensions.Item4); }
    catch (ArgumentException) { refused = true; }
    Check(refused, "invalid pixel bounds accepted");
}
using (var retained = new Representatives<TestFrame>(300, 10, 40))
{
    var first = new TestFrame(); var middle = new TestFrame(); var crossing = new TestFrame(); var next = new TestFrame(); var capped = new TestFrame(); var last = new TestFrame();
    retained.Observe(first, false); retained.Observe(middle, false); retained.Observe(crossing, true); retained.Observe(next, true);
    retained.Observe(capped, true); retained.Observe(last, false);
    Check(retained.Capacity == 4 && retained.ThresholdCandidates == 3 && retained.OmittedThresholds == 1, "representative memory/crossing caps wrong");
    Check(retained.Frames().SequenceEqual(new[] { first, crossing, next, last }), "first/last/threshold ownership wrong");
    Check(middle.Disposals == 1 && capped.Disposals == 1 && first.Disposals == 0 && last.Disposals == 0, "superseded bitmap retention leaked or released evidence");
    retained.Dispose(); retained.Dispose();
    Check(new[] { first, middle, crossing, next, capped, last }.All(f => f.Disposals == 1), "retained bitmaps double disposed or leaked");
}
using (var retained = new Representatives<TestFrame>(2, 10, 100))
{
    var only = new TestFrame(); retained.Observe(only, true);
    Check(retained.Frames().Count == 1, "single processed frame duplicated as first/last");
    retained.Dispose(); Check(only.Disposals == 1, "single representative disposed twice");
}
bool budgetRefused = false;
try { using var tooLarge = new Representatives<TestFrame>(300, 30, 40); }
catch (ArgumentException) { budgetRefused = true; }
Check(budgetRefused, "first/last memory reserve not enforced");

using (var inbox = new FrameInbox<TestFrame>(2))
{
    var a = new TestFrame(); var b = new TestFrame(); var dropped = new TestFrame();
    inbox.Offer(a); inbox.Offer(b); inbox.Offer(dropped);
    Check(dropped.Disposals == 1 && a.Disposals == 0 && b.Disposals == 0, "overflow must dispose only rejected frame");
    var taken = await inbox.Take(1); Check(ReferenceEquals(taken, a), "accepted frames reordered");
    inbox.Stop(); Check(a.Disposals == 0 && b.Disposals == 1, "stop must release queued frame without disposing consumer ownership");
    taken!.Dispose(); inbox.Offer(new TestFrame());
    Check(inbox.Counts == new InboxCounts(4, 1, 1, 1, 0), "skip counts inconsistent");
    Check(await inbox.Take(1) == null, "stopped queue exposes discarded frame");
    inbox.Fail("window replaced"); inbox.Fail("later error"); Check(inbox.Failure == "window replaced", "first failure lost");
    inbox.Stop(); Check(a.Disposals == 1 && b.Disposals == 1 && dropped.Disposals == 1, "repeated terminal cleanup double-disposed frame");
}
using (var singleInbox = new FrameInbox<TestFrame>(1))
{
    var first = new TestFrame(); var late = new TestFrame();
    singleInbox.Offer(first); singleInbox.Offer(late); var frame = await singleInbox.Take(1); frame!.Dispose(); singleInbox.Stop();
    Check(first.Disposals == 1 && late.Disposals == 1, "single-frame loser leak regressed");
}
using (var concurrent = new FrameInbox<TestFrame>(2))
{
    var frames = Enumerable.Range(0, 1000).Select(_ => new TestFrame()).ToArray();
    await Task.WhenAll(Task.Run(() => Parallel.ForEach(frames, concurrent.Offer)), Task.Run(concurrent.Stop));
    concurrent.Stop();
    Check(frames.All(f => f.Disposals == 1), "concurrent terminal callback leaked or double-disposed a frame");
    var c = concurrent.Counts;
    Check(c.Received == 1000 && c.DroppedQueueFull + c.RejectedAfterStop + c.DiscardedOnStop == 1000, "terminal counts do not account for every observed frame");
}
Console.WriteLine($"PASS {checks} capture parser/pixel/retention/queue checks; no graphics, window, device or game opened.");
sealed class TestFrame : IDisposable { private int _disposals; internal int Disposals => Volatile.Read(ref _disposals); public void Dispose() => Interlocked.Increment(ref _disposals); }
