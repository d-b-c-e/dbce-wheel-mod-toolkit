# dbce-wheel-mod-toolkit

The shared core for mods that add **force feedback, telemetry and wheel support** to racing games: one native FFB and
device layer with a C ABI, .NET bindings that load in Unity Mono, IL2CPP and CoreCLR, the shared F6 panel core,
installer and packager, settings store readers, a knowledge base, and templates for the next game.

Anything that knows a particular game (hooks, samplers, force signals and their tuning, cameras, game UI) stays in
that game's repo and consumes this one. Setup (bindings, cockpit, screens) belongs to Wheelkit, which comes with every
mod (STD-053); the in-game F6 panel is runtime only.

## What is here

| Folder | Contents |
|---|---|
| `native/wheelffb` | `WheelFfb.dll` (x64 and x86): DirectInput force feedback and device reads; C ABI in `include/wheelffb.h` |
| `native/` (others) | Force model, Forza telemetry packets, input shim, F6 panel (`panel`, `ui-host`), cameras, controls, settings, session writer, in-mod self-test |
| `dotnet/` | `Dbce.Wheel.*` libraries (netstandard2.0) and their tests |
| `shared/`, `unity/` | Engine-free display, FFB, UI and support sources; the Unity layer compiled into each plugin |
| `installer/`, `schema/` | The player installer, `dbce-mod.json` and the shared store schemas, with readers in PowerShell, C# and C++ |
| `tools/` | Sync into games (`Sync-Toolkit.ps1`, `Sync-Native.ps1`), packagers, probes, consumer report |
| `profiles/` | Force tunes as data, read at runtime by both force models |
| `knowledge/`, `docs/`, `templates/` | What has been learned, how-to pages and ADRs, starting points for a new game |

## How games use it

- Managed mods reference the `Dbce.Wheel.*` DLLs; native mods include `wheelffb.h` and load `WheelFfb.dll` at
  runtime, so a missing file costs force feedback rather than the game.
- `tools/Sync-Toolkit.ps1` vendors a pinned build into a game repo; native dinput8-proxy games use
  `tools/Sync-Native.ps1`. Never edit a vendored copy in a game: change it here and re-sync.
- `tools/package/New-ModPackage.ps1` builds a game's single player zip from its `dbce-mod.json`.
- Who uses which version: [docs/CONSUMERS.md](docs/CONSUMERS.md) and `tools/Get-ConsumerStatus.ps1`.
  More detail: [docs/CONSUMING.md](docs/CONSUMING.md).

## Key docs

- [STANDARDS.md](STANDARDS.md): product standards (append-only).
- [docs/DBCE-MOD-JSON.md](docs/DBCE-MOD-JSON.md), [docs/STORE.md](docs/STORE.md), [docs/SELFTEST.md](docs/SELFTEST.md).
- [docs/RECORDING-PLAYBACK.md](docs/RECORDING-PLAYBACK.md): recording and playback, developer tools only (STD-050).
- [docs/FFB-NORMALIZATION.md](docs/FFB-NORMALIZATION.md): device-free cross-game FFB comparison.
- [ADR-0001](docs/adr/0001-ffb-output-backends.md): DirectInput primary, SDL2 secondary, one interface.

## Building

```powershell
dotnet test Dbce.Wheel.sln
.\native\wheelffb\build.ps1                     # x64, checks every export against the header
.\native\wheelffb\build.bat x86                 # 32-bit games
.\native\panel\build_test.ps1
.\native\ui-host\build.ps1
.\tools\tests\Test-NativeBinding.ps1
.\tools\tests\Test-SyncToolkit.ps1
.\tools\tests\Test-SyncNative.ps1
.\tools\tests\Test-ModPackage.ps1
.\tools\tests\Test-SetupWheelkit.ps1
```

`AGENTS.md` lists every suite. `native\wheelffb\test\build_smoke.bat x86` walks the C ABI against a real wheel and
applies no torque unless given `--force`; run it whenever the native DLL changes.

Needs the .NET SDK and Visual Studio Build Tools with the C++ workload and the Windows SDK (`dinput8.lib`). No game
needs to be installed.

## Licence

MIT. No game files are redistributed.
