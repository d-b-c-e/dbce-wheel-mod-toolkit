# Playback core 0.3.0

`Dbce.Wheel.Playback` is a separate development component targeting netstandard2.0.
It does not change or republish the native FFB/telemetry release. No Unity, loader,
device, process launch, input injection or network dependency is present.

- `SegmentCursor<T>` consumes state-labelled streams in order. Required driving
  segments cannot be skipped, including future segments. Repeated state names
  retain their separate occurrences. A monotonic adapter clock bounds waiting.
- `BufferedTapeWriter` uses an exclusive file and bounded background queue.
  Overflow, oversize rows and writer failure invalidate the capture; closing
  drains the final tail before a completion seal can be created.
- `ArtifactSeal` hashes completed files and validates required streams, sizes,
  relative paths and hashes before playback. It rejects reparse-point payloads.
- `SignalStatistics` reports per-sample RMS, signed range, absolute nearest-rank
  percentiles and optional normalized saturation. Missing channels stay missing.
- `TrajectoryTape` records a bounded single-stage pose stream with explicit game
  build, scene/car identity, timestep and physical-output policy. It rejects
  nonfinite poses, invalid quaternions, missing physics steps and changed files.
- `TrajectoryPlayer` owns the adapter through acquire/apply/release and checks
  position and rotation on the following tick, including the final pose. Any
  competing transform writer fails playback and restores the original flags.
  Additive `StepUntil(exclusiveEnd)` stops before the next interval without releasing
  ownership. Its `false` return at an interior boundary requires the last applied
  pose to have passed the subsequent callback's position/rotation check. `Verified`
  counts those checked poses separately from `Applied`. Repeated calls at the same
  boundary recheck the held pose; resuming with a larger end also checks it before
  applying another pose, so drift during a native pause cannot be concealed.
  Ends must be positive, within the tape and nondecreasing; invalid calls close the
  player, releasing any held target. Only the full tape end sets `Passed` and releases
  normally. Existing `Step()` still runs to the tape end with its final next-tick check.
  Native pause/menu timing, eligibility, output suppression and timeout policy remain
  in each game adapter. An interval return is not proof of a native pause or finish.
- `StageSession` shares arming, capture, playback, completion and timeout behavior.
  It keeps original signals alongside poses through game-specific callbacks.
  `FileSessionControl` provides expiring, identified local commands and status.
  Eligibility and timestep are checked throughout playback. Control storage
  failure aborts the session without escaping the game's update loop; an idle
  controller writes no repeated status files.
  ReplayActive means an accepted replay is armed or playing; it is false for
  idle, armed/active recording and every terminal state. Consumers must not use
  !Recording or status-text parsing to authorize replay-only behavior. This
  state does not itself grant background execution, output mute or menu access;
  those remain explicit consumer policies. Core lifecycle fixtures: 62 checks.
  These new stage APIs have device-free fixtures; each consumer still needs live
  hook/ownership/scene tests. They do not automate menus or load saved game state.

Consumers own scene/menu state, selection snapshots, physics hooks, pose/input
codecs, force signals, units, output suppression and restoration. Keep three
capabilities distinct: trajectory playback recreates recorded motion; signal
reprocessing evaluates models against original samples; input diagnostics expose
divergence and must not claim deterministic Unity physics.

Art of Rally's format-3 adapter first validated core 0.1.0 in Unity 2019.4 Mono:
a fresh synthetic physics capture/replay, and the owner's complete session-5
trajectory through finish/results/menu. Other adapters need their own live tests.
Physical force and SimHub network delivery were muted during those tests. The
recording retained the requested force, effect arguments and telemetry before
muting. Native acceptance is not measured torque.

Run `dotnet run --project dotnet/Dbce.Wheel.Playback.Tests -c Release`.
The same device-free interval/drift fixtures can read an existing sealed original:
`dotnet run --project dotnet/Dbce.Wheel.Playback.Tests -c Release -- --trajectory <directory> --pause-at <exclusive-index>`.
They use a fake target; passing them does not qualify runtime native pause behavior.
October 11 candidate: the original DRIVE native-01 tape (3,064 poses, boundary 2,446)
passes 2,555 assertions including injected position and rotation drift after pause.
No consumer pin or installation changes follow automatically from this source check.
Consumers vendor the DLL with source commit, version and SHA-256; this component
is not part of the existing whole-toolkit package pin.

`tools/playback/Stage-Session.ps1 -Game woden|drive|iracing -Action Record
-Path <new-directory> -Seconds 60` arms one stage. Select the desired stage/car
in the game. `-Action Replay -Path <capture> -Output <new-directory>` requires
the recorded build, scenario and physics rate. `-Launch` optionally opens Steam;
it does not dismiss Steam prompts. `-Action Status` reports the acknowledged
request ID and terminal result; F12 or `-Action Stop` stops the session.
Physical-output suppression stays latched until the game restarts. Stop before
quitting and verify `complete.tsv`; an incomplete directory is evidence of a
failure, never a playable capture. These commands require a compatible game
adapter and do not imply that its game hooks have been runtime-qualified.
