# dbce-wheel-mod-toolkit

The shared core for a family of mods that add **force feedback, telemetry and
wheel support** to racing games that lack them. One native FFB and device
layer with a C ABI, thin .NET bindings that load in Unity Mono and CoreCLR
alike, shared tooling, a knowledge base of everything that has already been
paid for once, and templates for the next game.

The native device layer, managed telemetry, force models and versioned profiles
have consumers. SDL backend consolidation and the common input library remain
in progress. The [independent review](docs/REVIEW-2026-09-05.md) distinguishes
implemented behavior from plans and identifies the remaining contract gaps.

Local experiment **0.16.0-external.1** adds a dedicated-owner finite constant
API (native **0.10.0**, 55 exports), optional managed binding, bounded original-
capture leases and explicit lifecycle status. It is **offline verified only**;
hardware and game integration remain untested. See the
[external-session contract](docs/EXTERNAL-FFB.md) and exact clean-source receipt
in `dist/EXTERNAL-HANDOFF.json`. Existing consumers retain the old behavior
unless they explicitly opt in. No official release or adoption is implied.

Preserved local candidate **0.16.0-pov.1**, based on the `v0.15.0` runtime tag, adds a
coherent axes/buttons/four-POV snapshot (native **0.9.0**, 47 exports) and an
optional managed reader. It is not published or hardware-validated. See the
[POV contract and integration order](docs/POV-INPUT.md).

Source version **0.12.0** adds transactional updates, exact-module managed
loading, strict saved-wheel selection, force-state recovery and actionable
setup diagnostics. See [consumer experience](docs/CONSUMER-EXPERIENCE.md) for
integration examples and [art of rally adoption](docs/ART-OF-RALLY-ADOPTION.md)
for the compatibility curve and validation limits.

## Why

| Before this repo | Copies |
|---|---|
| DirectInput constant-force FFB implementations | 6 |
| Forza "Data Out" telemetry encoders (two incompatible layouts) | 5 |
| dinput8 proxy scaffolds | 3 |
| DirectInput device enumerators | 8+ |
| The `DIERR_NOTEXCLUSIVEACQUIRED` fix, the Fanatec twin-device fallback, the vJoy blocklist, the exit-time zeroing, the 323-vs-324-byte Forza trap… | 1 each, in five different repos |

Full survey and rationale: [docs/PROPOSAL.md](docs/PROPOSAL.md).

## What lives here, and what does not

**Here:** device → forces (`native/wheelffb`), frame → telemetry
(`dotnet/Dbce.Wheel.Telemetry`), force shaping and device selection
(`dotnet/Dbce.Wheel.Ffb`), planned shared input kit
(`dotnet/Dbce.Wheel.Input`, currently empty), the dinput8 proxy scaffold (`native/dinput-proxy`),
tooling (`tools/`), knowledge (`knowledge/`), documentation starting points
(`templates/`, not compilable projects or a shared UI).

**Not here:** anything that knows a particular game — hooks, samplers, force
signals and their tuning, physics changes and their online gates, cameras, UI
content. Those stay in each game's repo and consume this one.

## Consumers

- [art-of-sim-rally](https://github.com/d-b-c-e/art-of-sim-rally) — art of rally, Unity 2019 Mono, Unity Mod Manager
- iracing-arcade-wheel — iRacing Arcade, Unity 6 IL2CPP, BepInEx 6
- cruisn-collection — Cruis'n USA / World / Off Road / Exotica, MAME fork
- OutRun2006Tweaks-FFB — OutRun 2006, native dinput8 proxy
- milestone-wheel-tools — Gravel and other Milestone UE4 titles, native dinput8 proxy

How a game feels is data: `profiles/force-profiles.ini` holds named, versioned
tunes that ship beside the game and are read at runtime, so swapping one is a
text edit rather than a build. Both the managed and the native model read the
same file and are pinned to the same golden output.

Who is on which version, and which release obliges whom to move:
[docs/CONSUMERS.md](docs/CONSUMERS.md), with `tools/Get-ConsumerStatus.ps1`
to check it against reality.

A managed mod references `Dbce.Wheel.*.dll`; a native mod includes
`native/wheelffb/include/wheelffb.h` and loads `WheelFfb.dll` at runtime, so a
missing file costs force feedback rather than stopping the game from starting.
Either way the consumer vendors a **built, pinned** version under `lib/toolkit`
(`tools/Sync-Toolkit.ps1`) — see [docs/CONSUMING.md](docs/CONSUMING.md).

## Migration order

1. Knowledge base and this skeleton.
2. Forza telemetry library with its existing test suite; both Unity mods consume it.
3. Native FFB core unified on the most complete implementation, plus the lifecycle rules the others learned; both Unity mods ship one binary.
4. Device identification and the per-brand quirk table, feeding `wheel-db`.
5. Input kit, IL2CPP-safe IMGUI widgets, templates and the new-game checklist.

## Decisions

- [ADR-0001 — FFB output backends: DirectInput primary, SDL2 secondary, one interface](docs/adr/0001-ffb-output-backends.md)

## Building

```powershell
dotnet test Dbce.Wheel.sln
.\native\wheelffb\build.ps1                     # x64, verifies every export
.\native\wheelffb\build.bat x86                 # 32-bit games
.\native\wheelffb\test\build_pov_read_test.bat  # production read path, fake devices only
.\native\wheelffb\test\build_smoke.bat x86     # then build\x86\smoke.exe
.\native\forza\build_test.bat                   # packet conformance
.\tools\powershell\Test-DbceWheel.ps1
.\tools\tests\Test-SyncToolkit.ps1
.\tools\tests\Test-NativeBinding.ps1
```

`smoke.exe` walks the whole C ABI against a real wheel and applies no torque
unless given `--force`, so it is safe to run beside a live direct-drive base.

Needs the .NET SDK and Visual Studio Build Tools with the C++ workload and the
Windows SDK (`dinput8.lib`). No game needs to be installed.

## Licence

MIT. No game files are redistributed.
