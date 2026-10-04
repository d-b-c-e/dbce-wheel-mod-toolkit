# Playback core 0.1.0

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

Consumers own scene/menu state, selection snapshots, physics hooks, pose/input
codecs, force signals, units, output suppression and restoration. Keep three
capabilities distinct: trajectory playback recreates recorded motion; signal
reprocessing evaluates models against original samples; input diagnostics expose
divergence and must not claim deterministic Unity physics.

Art of Rally's format-3 adapter first validated this core in Unity 2019.4 Mono:
a fresh synthetic physics capture/replay, and the owner's complete session-5
trajectory through finish/results/menu. Other adapters need their own live tests.
Physical force and SimHub network delivery were muted during those tests. The
recording retained the requested force, effect arguments and telemetry before
muting. Native acceptance is not measured torque.

Run `dotnet run --project dotnet/Dbce.Wheel.Playback.Tests -c Release`.
Consumers vendor the DLL with source commit, version and SHA-256; this component
is not part of the existing whole-toolkit package pin.
