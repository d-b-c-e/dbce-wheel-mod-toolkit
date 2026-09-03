# dbce-wheel-mod-toolkit

The shared core for a family of mods that add **force feedback, telemetry and
wheel support** to racing games that lack them. One native FFB and device
layer with a C ABI, thin .NET bindings that load in Unity Mono and CoreCLR
alike, shared tooling, a knowledge base of everything that has already been
paid for once, and templates for the next game.

**Status: foundation.** Structure, decisions and knowledge index are laid
down; code is being migrated from the consumer repos in the order below.

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
(`dotnet/Dbce.Wheel.Ffb`), direct wheel reading and the Unity Input System kit
(`dotnet/Dbce.Wheel.Input`), the dinput8 proxy scaffold (`native/dinput-proxy`),
tooling (`tools/`), knowledge (`knowledge/`), scaffolds (`templates/`).

**Not here:** anything that knows a particular game — hooks, samplers, force
signals and their tuning, physics changes and their online gates, cameras, UI
content. Those stay in each game's repo and consume this one.

## Consumers

- [art-of-sim-rally](https://github.com/d-b-c-e/art-of-sim-rally) — art of rally, Unity 2019 Mono, Unity Mod Manager
- iracing-arcade-wheel — iRacing Arcade, Unity 6 IL2CPP, BepInEx 6
- cruisn-collection — Cruis'n USA / World / Off Road / Exotica, MAME fork
- OutRun2006Tweaks-FFB — OutRun 2006, native dinput8 proxy
- milestone-wheel-tools — Gravel and other Milestone UE4 titles, native dinput8 proxy

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
.\native\wheelffb\build.ps1
```

Needs the .NET SDK and Visual Studio Build Tools with the C++ workload and the
Windows SDK (`dinput8.lib`). No game needs to be installed.

## Licence

MIT. No game files are redistributed.
