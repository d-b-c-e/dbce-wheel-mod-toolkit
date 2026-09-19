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

Local candidate **0.13.0** adds finite periodic bursts and all-family focus/watchdog
stopping. It is not published. See [burst API and validation limits](docs/PERIODIC-BURSTS.md).

Source version **0.12.0** adds transactional updates, exact-module managed
loading, strict saved-wheel selection, force-state recovery and actionable
setup diagnostics. See [consumer experience](docs/CONSUMER-EXPERIENCE.md) for
integration examples and [art of rally adoption](docs/ART-OF-RALLY-ADOPTION.md)
for the compatibility curve and validation limits.

For a consistent experience across games, use the [shared UX standard](docs/CONSUMER-UX.md),
[install and first-drive guide](docs/CONSUMER-SETUP.md), and
[consumer audit checklist](docs/CONSUMER-UX-CHECKLIST.md). They define the intended
player experience; adoption and runtime verification remain per consumer.
The 2026-09-16 [Simple/Advanced standard](docs/CONSUMER-SETTINGS-VIEWS.md) makes
Simple the default for basic setup and driving. Advanced adds detailed tuning
and diagnostics over the same saved settings. It includes exact page placement,
upgrade rules and acceptance checks. The [consumer deployment ledger](docs/ux-audits/2026-09-19-deployment.md)
tracks implementation, installed packages and remaining live checks separately.
All seven consumers have local deployed builds as of September 19; the ledger
retains the specific game/physical acceptance still outstanding.
The [interactive reference](docs/reference/wheel-settings.html) provides a common
visual and interaction target; see its [preview guide](docs/reference/README.md).
It is an offline browser demo, with no game connection or force output.
The 2026-09-14 revision requires analog handbrake hardware support and an
FFB device dropdown that defaults to the wheel bound to Steering, while retaining
explicit overrides and saved device identities.
The [binding/camera workflows](docs/CONSUMER-CONTROLS-CAMERAS.md) define the
handbrake capture flow, live numpad defaults and in-panel adjustment rebinding.
The [Woden reconciliation audit](docs/ux-audits/2026-09-14-woden-controls-camera.md)
records the source gaps at that audit date and the required interaction checks.
See the [first cross-consumer UX audit](docs/ux-audits/2026-09-13-consumers.md)
for specific gaps in iRacing Arcade Wheel and Art of Sim Rally.

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
tooling (`tools/`), knowledge (`knowledge/`), and documentation starting points
in `templates/` (not compilable scaffolds or a shared UI).

**Not here:** anything that knows a particular game — hooks, samplers, force
signals and their tuning, physics changes and their online gates, cameras, UI
content. Those stay in each game's repo and consume this one.

## Consumers

- [art-of-sim-rally](https://github.com/d-b-c-e/art-of-sim-rally) — art of rally, Unity 2019 Mono, Unity Mod Manager
- iracing-arcade-wheel — iRacing Arcade, Unity 6 IL2CPP, BepInEx 6
- woden-rally-edge-wheel — Super Woden Rally Edge, Unity 6 IL2CPP, BepInEx 6
- drive-rally-wheel — #DRIVE Rally, Unity Mono, BepInEx 5; telemetry/research foundation
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
