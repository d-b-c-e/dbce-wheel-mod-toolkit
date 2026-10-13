using System.IO;
using Dbce.Wheel.Sessions;

// Data-only full-session timeline checks: no engine, device, process or package file.
const string Schema = "test.full-session@1";
string build = new('a', 64);
int checks = 0;
void Check(bool good, string reason) { checks++; if (!good) throw new Exception(reason); }
void Refuses(Action action, string reason)
{
    checks++;
    try { action(); }
    catch (InvalidDataException) { return; }
    throw new Exception("accepted: " + reason);
}
FullSessionSelection Stage(string car = "car=8") => new() { Scene = "Kenya SS1", Biome = "route-0", Track = "kenya-ss1", Car = car,
    Mode = "Arcade/Arcade", Start = "round=0", FixedDeltaTime = .02 };
FullSessionState State(FullSessionPhase phase, string marker, FullSessionSelection? selection = null) => new() { Phase = phase, Marker = marker, Selection = selection };
FullSessionArtifact Artifact(int poses) => new() { Directory = "source.stage", SealSha256 = new string('b', 64), PoseCount = poses };

// One launch: boot, menus, loading, a driven stage with a pause, finish, then a menu.
var capture = new FullSessionCapture(Schema, build, "test.observed-stage@1");
capture.Observe(State(FullSessionPhase.Boot, "recording-start"), 0, 0);
capture.Observe(State(FullSessionPhase.Menu, "scene:Title Screen"), 0, 1);
capture.Observe(State(FullSessionPhase.Menu, "scene:Title Screen"), 0, 1.5); // the same state is not repeated
capture.Observe(State(FullSessionPhase.Loading, "load:StagePresentation"), 0, 2);
capture.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage()), 0, 3);
capture.Observe(State(FullSessionPhase.Paused, "stage.paused", Stage()), 50, 4);
capture.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage()), 50, 9);
capture.Observe(State(FullSessionPhase.Finish, "stage.finish", Stage()), 120, 10);
capture.Observe(State(FullSessionPhase.Menu, "scene:MapScreen"), 120, 12);
var tape = capture.Stop(120, 13, Artifact(120));
tape.Validate(Schema, forPlayback: true);
Check(tape.Outcome == FullSessionOutcome.Stopped, "finish without results is a stopped session");
Check(tape.Steps.Count(s => s.State?.Marker == "scene:Title Screen") == 1, "an unchanged state is recorded once");
var intervals = tape.Steps.Where(s => s.Kind == FullSessionStepKind.Poses).ToArray();
Check(intervals.Length == 2 && intervals[0].FirstPose == 0 && intervals[0].PoseCount == 50 && intervals[1].FirstPose == 50 && intervals[1].PoseCount == 70,
    "a pause closes one pose interval and resumes the next");
int finish = Array.FindIndex(tape.Steps, s => s.Action?.Kind == FullSessionActionKind.FinishStage);
Check(finish > 0 && tape.Steps[finish + 1].State?.Phase == FullSessionPhase.Finish, "the observed finish records its native finish action first");
Refuses(() => tape.Validate("other.full-session@1"), "another game's schema");

// Poses only advance while driving, and the pose counter never rewinds.
var paused = new FullSessionCapture(Schema, build, "a@1");
paused.Observe(State(FullSessionPhase.Boot, "recording-start"), 0, 0);
paused.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage()), 0, 1);
paused.Observe(State(FullSessionPhase.Paused, "stage.paused", Stage()), 10, 2);
Refuses(() => paused.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage()), 11, 3), "poses written while paused");
var rewind = new FullSessionCapture(Schema, build, "a@1");
rewind.Observe(State(FullSessionPhase.Boot, "recording-start"), 0, 0);
rewind.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage()), 0, 1);
rewind.Observe(State(FullSessionPhase.Menu, "scene:MapScreen"), 10, 2);
Refuses(() => rewind.Observe(State(FullSessionPhase.Menu, "scene:Title Screen"), 9, 3), "a rewound pose counter");
Refuses(() => rewind.Observe(State(FullSessionPhase.Menu, "scene:Title Screen"), 10, 1), "a rewound clock");

// A second, different stage in one session is outside the format.
var second = new FullSessionCapture(Schema, build, "a@1");
second.Observe(State(FullSessionPhase.Boot, "recording-start"), 0, 0);
second.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage()), 0, 1);
second.Observe(State(FullSessionPhase.Menu, "scene:MapScreen"), 10, 2);
second.Observe(State(FullSessionPhase.Driving, "stage.driving", Stage("car=9")), 10, 3);
Refuses(() => second.Stop(20, 4, null), "a changed stage identity");

// Menu-only evidence is valid but can never be played.
var menus = new FullSessionCapture(Schema, build, "a@1");
menus.Observe(State(FullSessionPhase.Boot, "recording-start"), 0, 0);
menus.Observe(State(FullSessionPhase.Menu, "scene:Title Screen"), 0, 1);
var menuTape = menus.Stop(0, 2, null);
menuTape.Validate(Schema);
Refuses(() => menuTape.Validate(Schema, forPlayback: true), "playback without a sealed stage");

// The stage reference must match the poses and stay inside the package.
Refuses(() => capture.Stop(120, 14, Artifact(120)), "a second Stop");
foreach (string bad in new[] { "../source.stage", "/source.stage", "a\\b", "c:stage", "stage/", "stage/./x", "stage." })
{
    var copy = tape.Copy(); copy.Stage!.Directory = bad;
    Refuses(() => copy.Validate(Schema), "unsafe stage path " + bad);
}
var short_ = tape.Copy(); short_.Stage!.PoseCount = 119; Refuses(() => short_.Validate(Schema), "stage pose count differs from the intervals");
var unsealed = tape.Copy(); unsealed.Stage!.SealSha256 = "not-a-hash"; Refuses(() => unsealed.Validate(Schema), "malformed stage seal");
var completed = tape.Copy(); completed.Outcome = FullSessionOutcome.CompletedStage;
Refuses(() => completed.Validate(Schema), "completed-stage without observed results");
var failed = tape.Copy(); failed.Outcome = FullSessionOutcome.Failed;
failed.Validate(Schema); Refuses(() => failed.Validate(Schema, forPlayback: true), "playback of a failed session");
var nonBoot = tape.Copy(); nonBoot.Steps = nonBoot.Steps.Skip(1).ToArray(); Refuses(() => nonBoot.Validate(Schema), "a session that does not start at boot");
Refuses(() => new FullSessionCapture(Schema, build, "a@1").Observe(State(FullSessionPhase.Driving, "stage.driving"), 0, 0), "driving without a selection");
var badStep = Stage(); badStep.FixedDeltaTime = double.NaN;
Refuses(() => badStep.Validate(), "a NaN timestep");
Refuses(() => FullSessionTimeline.Text("line\nbreak"), "control characters in session text");

// Payload names follow the original signal file; the outer seal covers the descriptor and every payload.
var names = FullSessionPackageNames.FromSignal("source.jsonl");
Check(names.Observation == "source.session.jsonl" && names.Timeline == "source.stage-session.json" && names.Stage == "source.stage", "payload names");
Check(names.OuterSealFiles().SequenceEqual(new[] { "session.json", "source.jsonl", "source.session.jsonl", "source.stage-session.json", "source.stage/complete.tsv" }),
    "outer seal inventory");
foreach (string bad in new[] { "source.json", "dir/source.jsonl", ".jsonl", "..\\source.jsonl", "" })
    Refuses(() => FullSessionPackageNames.FromSignal(bad), "signal name " + bad);
Check(FullSessionPackageNames.UnavailableReason(" a\nb ") == "a b", "reason control characters");
Check(FullSessionPackageNames.UnavailableReason("") == "The native route compiler refused this recording.", "empty reason");
string pair = new string('x', FullSessionPackageNames.MaxReasonLength - 1) + "😀";
Check(FullSessionPackageNames.UnavailableReason(pair).Length == FullSessionPackageNames.MaxReasonLength - 1, "the limit never splits a surrogate pair");

Console.WriteLine($"Full-session timeline checks passed: {checks}");
