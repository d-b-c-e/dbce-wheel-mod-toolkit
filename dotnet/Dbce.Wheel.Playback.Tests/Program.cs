using Dbce.Wheel.Playback;

int assertions = 0;
void Check(bool ok, string message) { assertions++; if (!ok) throw new Exception(message); }
void Reject(Action action, string message) { try { action(); } catch (Exception ex) when (ex is IOException || ex is InvalidDataException) { assertions++; return; } throw new Exception(message); }
var stats = SignalStatistics.Summarize(new double[] { -1, 0, 1, 0 }, "normalized-request", true);
Check(stats.Count == 4 && stats.Rms == Math.Sqrt(.5) && stats.FractionAtUnitLimit == .5, "Symmetric force RMS and clipping");
Check(stats.Minimum == -1 && stats.Maximum == 1 && stats.AbsoluteP50 == 0 && stats.AbsoluteP95 == 1, "Signed range and absolute nearest-rank percentiles");
stats = SignalStatistics.Summarize(Array.Empty<double>(), "m/s");
Check(!stats.Available && stats.Rms == null && stats.Maximum == null, "Missing channels are not zero");
try { SignalStatistics.Summarize(new[] { double.NaN }, "m/s"); throw new Exception("Nonfinite accepted"); } catch (ArgumentException) { assertions++; }
var names = new Dictionary<int, string> { [0] = "menu", [1] = "drive", [2] = "finish" };
float now = 0;
SegmentCursor<int> Cursor(params int[] ids) => new SegmentCursor<int>(ids.Select((m, i) => new KeyValuePair<int, int>(m, i)).ToList(), names, "test", () => now, 0, m => m == "drive");
var cursor = Cursor(0, 1, 1, 2);
cursor.Next(0, out _);
Check(cursor.SkippedWhere(m => m == "drive") == 2, "Future driving rows must count as unplayed");
Check(!cursor.Next(2, out _) && cursor.Failed != null, "Cannot jump past unstarted driving segment");
cursor = Cursor(1, 1, 2); cursor.Next(1, out _);
Check(!cursor.Next(2, out _) && cursor.Failed != null, "Cannot skip remainder of drive");
cursor = Cursor(0, 0, 1, 1, 2);
Check(cursor.Next(1, out var row) && row == 2 && cursor.Skipped == 2, "May skip menu wait");
cursor.Next(1, out _);
Check(!cursor.Next(1, out _) && !cursor.Finished, "Wait at transition without consuming next state's row");
Check(cursor.Next(2, out _) && cursor.Finished, "Explicit final-state observation finishes");
cursor = Cursor(0, 1, 2, 0, 1, 2);
foreach (int id in new[] { 0, 1, 2, 0, 1, 2 }) Check(cursor.Next(id, out _), "Repeated stage order");
Check(cursor.Finished && cursor.Consumed == 6, "Repeated markers do not collapse occurrences");
cursor = Cursor(0, 1); cursor.Next(0, out _); cursor.Next(0, out _); now = 121; cursor.Next(0, out _);
Check(cursor.Failed != null, "Transition timeout");
var root = Path.Combine(Path.GetTempPath(), "dbce-playback-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    using (var writer = new BufferedTapeWriter(Path.Combine(root, "rows"), 20000))
    {
        for (int i = 0; i < 10000; i++) writer.WriteLine(i.ToString());
        writer.Flush();
    }
    Check(File.ReadAllLines(Path.Combine(root, "rows")).SequenceEqual(Enumerable.Range(0, 10000).Select(i => i.ToString())), "Queue preserves every row in order including final tail");
    Reject(() => new BufferedTapeWriter(Path.Combine(root, "rows")), "Cannot overwrite a capture");
    File.WriteAllText(Path.Combine(root, "metadata"), "fixture");
    ArtifactSeal.Complete(root, new[] { "rows", "metadata" });
    ArtifactSeal.Verify(root, "rows", "metadata"); assertions++;
    Reject(() => ArtifactSeal.Verify(root, "missing"), "Required streams must be sealed");
    File.AppendAllText(Path.Combine(root, "rows"), "extra");
    Reject(() => ArtifactSeal.Verify(root), "Changed or truncated payload fails verification");
    File.WriteAllText(Path.Combine(root, "complete.tsv"), "dbce-playback-seal-v1\n../escape\t0\thash\n");
    Reject(() => ArtifactSeal.Verify(root), "Reject traversal before opening referenced file");
    using (var writer = new BufferedTapeWriter(Path.Combine(root, "oversized")))
        Reject(() => writer.WriteLine(new string('a', 262145)), "Oversized rows rejected");
}
finally { Directory.Delete(root, true); }
Console.WriteLine($"Playback core passed {assertions} assertions");
