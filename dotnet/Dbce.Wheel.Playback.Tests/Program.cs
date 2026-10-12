using Dbce.Wheel.Playback;

int assertions = 0;
void Check(bool ok, string message) { assertions++; if (!ok) throw new Exception(message); }
void Reject(Action action, string message) { try { action(); } catch (Exception ex) when (ex is IOException || ex is InvalidDataException) { assertions++; return; } throw new Exception(message); }
void RejectCall<T>(Action action, string message) where T : Exception { try { action(); } catch (T) { assertions++; return; } throw new Exception(message); }
void CheckIntervals(TrajectoryTape tape, int boundary)
{
    Check(boundary > 0 && boundary < tape.Frames.Length, "Fixture has an interior interval boundary");
    var target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target))
    {
        for (int i = 0; i < boundary; i++) Check(player.StepUntil(boundary), "Exactly one pose applied per callback before boundary");
        Check(player.Applied == boundary && player.Verified == boundary - 1 && !player.Passed, "Writing interval endpoint alone cannot finish it");
        Check(!player.StepUntil(boundary) && player.Verified == boundary, "Endpoint verified on next callback");
        Check(target.Applies == boundary && target.Acquires == 1 && target.Releases == 0 && !player.Passed, "Boundary neither applies next pose nor releases ownership");
        int reads = target.Reads;
        Check(!player.StepUntil(boundary) && target.Reads == reads + 1 && target.Applies == boundary, "Paused boundary may be reverified without advancing");
        Check(player.Step() && player.Applied == boundary + 1 && target.Acquires == 1, "Legacy Step resumes held interval without reacquiring");
        while (player.Step()) { }
        Check(player.Passed && player.Verified == tape.Frames.Length && target.Applies == tape.Frames.Length && target.Releases == 1,
            "Mixed interval and legacy stepping verifies entire tape and releases once");
        RejectCall<InvalidOperationException>(() => player.StepUntil(tape.Frames.Length), "Completed player cannot step again");
        RejectCall<InvalidOperationException>(() => player.Step(), "Completed legacy Step remains closed");
    }
    foreach (bool rotation in new[] { false, true })
    {
        target = new FixtureTarget();
        using var player = new TrajectoryPlayer(tape, target);
        while (player.StepUntil(boundary)) { }
        if (rotation)
        {
            // Orthogonal unit quaternion relative to the actual held pose, including an original recording.
            var q = (float[])target.Current.Values.Clone();
            target.Current.Values[3] = -q[4]; target.Current.Values[4] = q[3];
            target.Current.Values[5] = -q[6]; target.Current.Values[6] = q[5];
        }
        else target.Current.Values[0] += 1;
        Reject(() => player.StepUntil(tape.Frames.Length), "Pause drift is refused on resume before another pose overwrites it");
        Check(!player.Passed && player.Applied == boundary && target.Applies == boundary && target.Releases == 1,
            "Pause mismatch releases once without consuming next interval");
    }
    target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target))
    {
        for (int i = 0; i < boundary; i++) player.StepUntil(boundary);
        target.Current.Values[0] += 1;
        Reject(() => player.StepUntil(boundary), "Wrong last-applied pose cannot acknowledge the boundary");
        Check(player.Verified == boundary - 1 && target.Releases == 1, "Failed boundary never increments verification");
    }
    target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target))
    {
        while (player.StepUntil(boundary)) { }
        player.Dispose();
        Check(!player.Passed && target.Releases == 1, "Intentional stop while paused releases without claiming full playback");
        RejectCall<InvalidOperationException>(() => player.StepUntil(tape.Frames.Length), "Disposed interval cannot resume");
    }
}
int restored = 0;
try
{
    CleanupActions.Run(() => throw new IOException("disk full"), () => restored++, () => throw new IOException("second failure"));
    throw new Exception("Cleanup failures were hidden");
}
catch (AggregateException ex) { Check(restored == 1 && ex.InnerExceptions.Count == 2, "Independent cleanup continues after a failed writer and reports every failure"); }
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
    var processSeal = File.ReadAllBytes(Path.Combine(root, "complete.tsv"));
    ArtifactSeal.CompleteNamed(root, "recording-complete.tsv", new[] { "rows", "metadata" });
    ArtifactSeal.VerifyNamed(root, "recording-complete.tsv", "rows", "metadata"); assertions++;
    Check(processSeal.SequenceEqual(File.ReadAllBytes(Path.Combine(root, "complete.tsv"))), "Independent recording seal leaves the process seal unchanged");
    Reject(() => ArtifactSeal.CompleteNamed(root, "recording-complete.tsv", new[] { "rows" }), "Cannot replace a named completion seal");
    foreach (var invalidSeal in new[] { "../escape", "nested/seal", "C:/escape", "", "seal:stream", "seal\\escape" })
    {
        Reject(() => ArtifactSeal.CompleteNamed(root, invalidSeal, new[] { "rows" }), "Reject invalid named seal before writes");
        Reject(() => ArtifactSeal.VerifyNamed(root, invalidSeal, "rows"), "Reject invalid named seal before reads");
    }
    Reject(() => ArtifactSeal.VerifyNamed(root, "recording-complete.tsv", "missing"), "Named seal checks required streams");
    Reject(() => ArtifactSeal.Verify(root, "missing"), "Required streams must be sealed");
    File.AppendAllText(Path.Combine(root, "rows"), "extra");
    Reject(() => ArtifactSeal.VerifyNamed(root, "recording-complete.tsv"), "Named seal rejects changed payload");
    Reject(() => ArtifactSeal.Verify(root), "Changed or truncated payload fails verification");
    File.WriteAllText(Path.Combine(root, "complete.tsv"), "dbce-playback-seal-v1\n../escape\t0\thash\n");
    Reject(() => ArtifactSeal.Verify(root), "Reject traversal before opening referenced file");
    using (var writer = new BufferedTapeWriter(Path.Combine(root, "oversized")))
        Reject(() => writer.WriteLine(new string('a', 262145)), "Oversized rows rejected");
}
finally { Directory.Delete(root, true); }
var trajectoryRoot = Path.Combine(Path.GetTempPath(), "dbce-trajectory-" + Guid.NewGuid().ToString("N"));
var metadata = new Dictionary<string, string> { ["contract"] = "dbce.trajectory@1", ["game"] = "fixture", ["gameSha256"] = new string('a', 64), ["scenario"] = "track|car", ["fixedDeltaTime"] = "0.02", ["captureSource"] = "synthetic", ["physicalOutput"] = "false" };
try
{
    using (var writer = new TrajectoryRecorder(trajectoryRoot, metadata))
    {
        for (int i = 0; i < 3; i++) { var f = new TrajectoryFrame { Time = i * .02 }; f.Values[0] = i; f.Values[6] = 1; writer.Write(f); }
        writer.Complete();
    }
    var tape = TrajectoryTape.Read(trajectoryRoot);
    CheckIntervals(tape, 2);
    foreach (int end in new[] { -1, 0, tape.Frames.Length + 1 })
    {
        var invalidTarget = new FixtureTarget();
        using var invalidPlayer = new TrajectoryPlayer(tape, invalidTarget);
        RejectCall<ArgumentOutOfRangeException>(() => invalidPlayer.StepUntil(end), "Invalid bounds refused");
        Check(invalidTarget.Acquires == 0 && invalidTarget.Applies == 0, "Invalid initial bounds cannot acquire target");
        RejectCall<InvalidOperationException>(() => invalidPlayer.Step(), "Invalid call is terminal");
    }
    var backwardsTarget = new FixtureTarget();
    using (var backwards = new TrajectoryPlayer(tape, backwardsTarget))
    {
        backwards.StepUntil(2);
        RejectCall<ArgumentException>(() => backwards.StepUntil(1), "Declared end cannot move backwards even before it is reached");
        Check(backwardsTarget.Applies == 1 && backwardsTarget.Releases == 1 && !backwards.Passed, "Out-of-order interval aborts ownership");
    }
    tape.ValidateTarget("fixture", new string('a', 64), "track|car", .02); assertions++;
    Reject(() => tape.ValidateTarget("fixture", new string('a', 64), "wrong-car", .02), "Scenario mismatch refused before ownership");
    Reject(() => tape.ValidateTarget("fixture", new string('a', 64), "track|car", .01), "Different physics timestep refused");
    Reject(() => tape.ValidateTarget("fixture", new string('a', 64), "track|car", double.NaN), "Nonfinite target timestep refused before ownership");
    Reject(() => new TrajectoryRecorder(trajectoryRoot, metadata), "Existing trajectory immutable");
    var target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target))
    {
        Check(player.Step() && player.Step() && player.Step() && !player.Passed, "Last pose must actually be observed on next tick");
        Check(!player.Step() && player.Passed && target.Releases == 1 && player.Applied == 3 && player.Verified == 3, "Complete trajectory releases ownership once");
    }
    Check(target.Releases == 1, "Release is idempotent");
    target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target))
    {
        player.Step(); target.Current.Values[0] += 1;
        Reject(() => player.Step(), "Competing physics detected");
        Check(!player.Passed && target.Releases == 1, "Failure restores original ownership");
    }
    target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target))
    {
        player.Step(); target.Current.Values[3] = .70710677f; target.Current.Values[6] = .70710677f;
        Reject(() => player.Step(), "Rotation ownership also verified");
    }
    target = new FixtureTarget();
    using (var player = new TrajectoryPlayer(tape, target)) { player.Step(); target.Current.Values[6] = -1; Check(player.Step(), "Equivalent negative quaternion accepted"); }
    var invalid = new TrajectoryFrame(); invalid.Values[6] = 1; invalid.Values[0] = float.NaN;
    Reject(() => invalid.Validate(), "Nonfinite pose rejected");
    using (var writer = new TrajectoryRecorder(trajectoryRoot + "-gap", metadata))
    {
        var f = new TrajectoryFrame(); f.Values[6] = 1; writer.Write(f); f.Time = .04;
        Reject(() => writer.Write(f), "Capture missing physics step invalidates tape");
        Reject(() => writer.Complete(), "Bad capture cannot acquire completion seal");
    }
    Check(!File.Exists(Path.Combine(trajectoryRoot + "-gap", "complete.tsv")), "Incomplete tape has no seal");
}
finally { Directory.Delete(trajectoryRoot, true); if (Directory.Exists(trajectoryRoot + "-gap")) Directory.Delete(trajectoryRoot + "-gap", true); }
var stageRoot = Path.Combine(Path.GetTempPath(), "dbce-stage-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(stageRoot);
try
{
    var target = new FixtureStage();
    using (var session = new StageSession(target))
    {
        Check(!session.ReplayActive, "Idle never grants replay mode");
        session.Record(Path.Combine(stageRoot, "capture"), 10, 0);
        Check(!session.ReplayActive, "Armed owner recording is not replay mode");
        Check(target.Muted && session.Active && !session.Recording, "Output suppression precedes stage acquisition");
        target.Available = true;
        for (int i = 0; i < 3; i++) { target.Current.Time = 100 + i * .02; target.Current.Values[0] = i; session.FixedStep(i * .02); }
        Check(!session.ReplayActive, "Live recording never grants replay mode");
        session.Stop();
        Check(session.Status.StartsWith("recorded:") && !session.Active, "Stage completes capture and signal seal");
    }
    var tape = TrajectoryTape.Read(Path.Combine(stageRoot, "capture"));
    Check(tape.Frames.Length == 3 && Math.Abs(tape.Frames[2].Time - .04) < 1e-9, "Capture uses actual physics clock relative to first frame");
    using (var session = new StageSession(target))
    {
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "replay"), 0);
        Check(session.ReplayActive && !session.Playing, "Validated armed replay exposes its mode before acquisition");
        session.FixedStep(0);
        Check(session.ReplayActive && session.Playing, "Active player retains replay mode");
        for (int i = 1; i < 4; i++) session.FixedStep(i * .02);
        Check(session.Status.StartsWith("passed:") && !session.Active, "Shared stage runner passes full replay");
        Check(!session.ReplayActive, "Completed replay revokes replay mode");
        session.Record(Path.Combine(stageRoot, "record-after-replay"), 10, 1);
        Check(!session.ReplayActive, "A subsequent owner recording cannot inherit replay mode");
        session.Stop();
    }
    using (var session = new StageSession(target))
    {
        session.Record(Path.Combine(stageRoot, "gap"), 10, 0);
        target.Current.Time = 20; session.FixedStep(0); target.Current.Time = 20.04; session.FixedStep(.04);
        Check(session.Status.StartsWith("failed:") && !File.Exists(Path.Combine(stageRoot, "gap", "complete.tsv")), "Runner cannot hide a missing physics callback with a synthetic counter");
    }
    using (var session = new StageSession(target))
    {
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "eligibility"), 0);
        session.FixedStep(0); target.Available = false; int before = target.Releases; session.FixedStep(.02);
        Check(session.Status.Contains("eligibility lost") && !session.Active && target.Releases == before + 1, "Eligibility is checked throughout playback and restores ownership");
        Check(!session.ReplayActive, "Aborted replay revokes mode immediately");
        target.Available = true;
    }
    using (var session = new StageSession(target))
    {
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "dt-change"), 0);
        session.FixedStep(0); target.Delta = .01; session.FixedStep(.02);
        Check(session.Status.Contains("timestep changed") && !session.Active, "Changing physics rate cannot silently change playback speed");
        target.Delta = .02;
    }
    using (var session = new StageSession(target))
    {
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "paused-without-physics"), 0);
        session.FixedStep(0); target.Available = false; int before = target.Releases; session.Heartbeat(.01);
        Check(!session.Active && target.Releases == before + 1 && session.Status.Contains("eligibility lost"), "Render heartbeat releases a paused replay even without another physics callback");
        target.Available = true;
    }
    using (var session = new StageSession(target))
    {
        session.Record(Path.Combine(stageRoot, "record-paused"), 10, 0);
        target.Current.Time = 10; session.FixedStep(0); target.Current.Time = 10.02; session.FixedStep(.02);
        target.Available = false; session.Heartbeat(.03);
        Check(!session.Active && session.Status.StartsWith("recorded:") && TrajectoryTape.Read(Path.Combine(stageRoot, "record-paused")).Frames.Length == 2,
            "Pause finalizes an aligned capture without waiting for physics to resume");
        target.Available = true;
    }
    using (var session = new StageSession(target))
    {
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "nan-timestep"), 0);
        session.FixedStep(0); target.Delta = double.NaN; session.Heartbeat(.01);
        Check(!session.Active && session.Status.Contains("timestep changed"), "Nonfinite physics clock cannot retain trajectory ownership");
        target.Delta = .02;
    }
    using (var session = new StageSession(target))
    {
        var controlRoot = Path.Combine(stageRoot, "control");
        var control = new FileSessionControl(controlRoot, session);
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "io-failure"), 0);
        session.FixedStep(0);
        Directory.CreateDirectory(Path.Combine(controlRoot, "status.tmp"));
        control.Poll(.1);
        Check(!session.Active && session.Status.Contains("Control storage") && control.LastError != null, "Status IO failure aborts ownership without escaping the game loop");
    }
    using (var session = new StageSession(target))
    {
        target.Available = false;
        session.Replay(Path.Combine(stageRoot, "capture"), Path.Combine(stageRoot, "timeout"), 0);
        session.Heartbeat(181);
        Check(!session.Active && session.Status.Contains("180 seconds"), "Bounded scene wait releases the session");
    }
}
finally { Directory.Delete(stageRoot, true); }
if (args.Length > 0)
{
    if (args.Length != 4 || args[0] != "--trajectory" || args[2] != "--pause-at" || !int.TryParse(args[3], out int boundary))
        throw new ArgumentException("Expected --trajectory <sealed-directory> --pause-at <exclusive-index>");
    var original = TrajectoryTape.Read(args[1]);
    CheckIntervals(original, boundary);
    Console.WriteLine($"Original sealed trajectory: {original.Frames.Length} poses, interval boundary {boundary}; fake-target checks only");
}
Console.WriteLine($"Playback core passed {assertions} assertions");

class FixtureTarget : ITrajectoryTarget
{
    public TrajectoryFrame Current = new();
    public int Releases, Acquires, Applies, Reads;
    public void Acquire() => Acquires++;
    public void Apply(TrajectoryFrame frame) { Applies++; Current = new TrajectoryFrame { Time = frame.Time, Values = (float[])frame.Values.Clone() }; }
    public TrajectoryFrame Read() { Reads++; return Current; }
    public void Release() => Releases++;
}
sealed class FixtureStage : FixtureTarget, IStageAdapter
{
    public string Game => "fixture";
    public string GameHash => new string('a', 64);
    public string Scenario => "track|car";
    public double Delta = .02;
    public double FixedDeltaTime => Delta;
    public bool Available, Muted;
    public bool Ready => Available;
    public bool PlaybackAllowed => Available;
    public FixtureStage() { Current.Values[6] = 1; }
    public void MuteOutputs() => Muted = true;
    public void StartSignals(string directory) { }
    public string[] FinishSignals() => Array.Empty<string>();
}
