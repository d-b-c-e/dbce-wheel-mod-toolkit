# Recording and playback (developer tools)

STD-050 keeps recording, playback and course capture as developer tooling. None of it ships in a player build, and no
F6 page, HUD state or hotkey for it belongs in one. For checking a mod in the game, use the in-mod self-test first
(`docs/SELFTEST.md`).

## What is here

| Part | What it does |
|---|---|
| `dotnet/Dbce.Wheel.Recording` | Numeric session capture: metadata, named channels and units, a streaming reader. No devices, no forces. |
| `dotnet/Dbce.Wheel.Playback` | `TrajectoryTape` (a pose stream that ends with `complete.txt`), `TrajectoryRecorder`, `TrajectoryPlayer` (applies poses and checks each one on the next tick), `SegmentCursor`, `BufferedTapeWriter`, `SignalStatistics`. |
| `dotnet/Dbce.Wheel.Course` | Course route files (`route.json`, `progress.tsv`, `complete.tsv`) and their reader. |
| `native/session` | `session_writer.h`, the native session writer; `Test-NativeSession.ps1`. |
| `native/shim/host/recorder` | The input shim's frame recorder. |
| `shared/session/FullSessionTimeline.cs` | Lines up a recorded session's streams on one timeline. |
| `tools/playback/Stage-Registry.ps1` | Snapshots and restores a game's registry settings byte for byte around a run (`docs/STAGE-REGISTRY.md`). |

All libraries target netstandard2.0 with tests on .NET 8 (`dotnet test Dbce.Wheel.sln`). They have no Unity, loader or
network dependency.

## Rules

- A game run with recording or playback still follows HOW-WE-WORK.md: hold the rig lease, back up and restore saves
  and settings, mute force feedback and motion.
- Back up registry settings with `Stage-Registry.ps1`, not `reg.exe export`, which can truncate long Rewired value
  names and still return success.
- A recorded API success shows the call was accepted, not how the force felt. Force feel stays the owner's check.
