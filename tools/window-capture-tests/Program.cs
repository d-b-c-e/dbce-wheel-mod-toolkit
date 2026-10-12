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
Console.WriteLine($"PASS {checks} capture parser/queue lifecycle checks; no graphics, window, device or game opened.");
sealed class TestFrame : IDisposable { private int _disposals; internal int Disposals => Volatile.Read(ref _disposals); public void Dispose() => Interlocked.Increment(ref _disposals); }
