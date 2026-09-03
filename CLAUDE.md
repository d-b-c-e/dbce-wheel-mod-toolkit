# dbce-wheel-mod-toolkit — Claude working notes

## Repository Purpose

The shared core for a family of mods that add **force feedback, telemetry and
wheel support** to racing games that lack them (or do them badly). It exists
because five projects had grown six DirectInput FFB implementations, five Forza
encoders in two incompatible layouts, three dinput8 proxy scaffolds and a
knowledge base scattered one page per repo — every FFB or binding bug was being
fixed in one place and left standing in the others.

The seam is fixed and deliberate: **the game side stays in each game's repo**
(hooks, samplers, force *signals*, physics changes, cameras, UI content). This
repo owns everything on the *other* side of the car-state struct: device →
forces, frame → telemetry, device identification, diagnostics, installers,
knowledge and templates.

Consumers today: `art-of-sim-rally` (Unity 2019 Mono, Unity Mod Manager),
`iracing-arcade-wheel` (Unity 6 IL2CPP, BepInEx 6), `cruisn-collection`
(MAME fork, SDL2), `OutRun2006Tweaks-FFB` (native dinput8 proxy),
`milestone-wheel-tools` (native dinput8 proxy, UE4). Adjacent: `cannonball-se`,
`mame-racing`, `xoutput-redux`, `wheel-db`, `controller-input-db`.

The founding survey and rationale: `docs/PROPOSAL.md` (2026-09-03).

## Repository Structure

| Path | Contents |
|---|---|
| `native/wheelffb/` | C-ABI DLL: FFB output + device layer. DirectInput backend (primary), SDL2 haptics backend (secondary). See ADR-0001. |
| `native/dinput-proxy/` | dinput8 proxy scaffold, SIXDOF→DRIVING re-type, input/FFB taps. Optional module. |
| `dotnet/Dbce.Wheel.Telemetry/` | `netstandard2.0`. Forza "Data Out" 232/311/324 encoders, `TelemetryFrame` (the car-state seam), idle keeper, RPM synthesis. Tests alongside. |
| `dotnet/Dbce.Wheel.Ffb/` | `netstandard2.0`. P/Invoke to the native DLL (Mono preload and CoreCLR resolver paths), force-shaping chain, device selection, diagnostics, support bundle. |
| `dotnet/Dbce.Wheel.Input/` | Direct-read binding (assign-by-moving, rest/far calibration); Unity Input System kit as a sub-package. |
| `tools/` | wheelprobe, forza probe/synth, ffb trace report, dinput axis reader, raw-input listener; PowerShell module (Steam discovery, device enumeration, CRLF-safe INI, installer scaffold, unattended-test helpers). |
| `knowledge/` | The knowledge base. Every page names the repo and date each item came from. `knowledge/README.md` is the migration tracker. |
| `templates/` | New-game scaffolds: `unity-mono-umm/`, `unity-il2cpp-bepinex/`, `native-dinput8-proxy/`, each with a CLAUDE.md skeleton carrying the non-negotiables. |
| `docs/adr/` | Architecture decisions. Start with ADR-0001 (FFB output backends). |

## Runtime matrix — the reason the core is native

| Consumer runtime | Managed lib target | Native loading |
|---|---|---|
| Unity 2019.4 Mono (UMM) | `netstandard2.0`, **no Unity references** | `LoadLibraryW` preload, then `DllImport` |
| .NET 6 CoreCLR (BepInEx 6 IL2CPP) | same assembly | `NativeLibrary.SetDllImportResolver` |
| Native C++ (proxies, MAME forks) | n/a | link or `LoadLibrary` the C ABI |
| Python / PowerShell tooling | n/a | ctypes / `Add-Type` against the C ABI |

`netstandard2.0` with zero Unity or loader references is what lets one
assembly load in both Unity runtimes and be unit-tested on modern .NET. Do
not add a Unity reference to a shared managed project; Unity-specific code
goes in a clearly named sub-package.

## Conventions

- **Solution stays classic `.sln`** (`dotnet new sln --format sln`); the .NET 10
  SDK's `.slnx` cannot be opened by older SDKs.
- Native DLL is **x64 only**, built with MSVC; `build.ps1` verifies every export
  with `dumpbin`. A mangled or missing export fails silently in-game.
- `BOOL` across the C ABI is the 4-byte Win32 `BOOL`; never C++ `bool`.
- The C ABI is the contract. Add exports; never change or remove one. Consumers
  vendor a *built* DLL, pinned by version, via `Sync-Toolkit.ps1`.
- Every knowledge item carries `[repo, YYYY-MM-DD]` provenance.
- Dates YYYY-MM-DD. Commits explain the why.

## Non-negotiable design rules (inherited, all paid for)

1. **One DirectInput instance per process.** Releasing a "temporary" instance
   while the device table stayed populated crashed a Fanatec user's game
   (art-of-sim-rally, 2026-09-03). `EnsureDirectInput()` at every entry point;
   release only when nothing else holds a device.
2. **Signed magnitude, direction length equal to |magnitude|, re-sent every
   update.** The only constant-force encoding that behaves on MOZA R5 (honours
   direction and sign), MOZA R12 (magnitude sign only) and single-axis Fanatec.
   `SetParameters` must match the effect's axis count or every update is
   `E_INVALIDARG`.
3. **Re-acquire on `DIERR_NOTEXCLUSIVEACQUIRED` (0x80040205)**; recreate and
   ramp in from zero over ~250 ms on `E_HANDLE` / `DIERR_NOTDOWNLOADED`; never
   re-acquire on a failed `Poll()` — it destroys downloaded effects. Look
   HRESULTs up in `dinput.h` before theorising; this code was misread twice.
4. **Zero the wheel while the game window still exists.** Hook `WM_CLOSE`,
   `WM_QUERYENDSESSION` and `ExitProcess`; by `DLL_PROCESS_DETACH` nothing
   reaches the device. A hold watchdog releases force when the game stops
   writing. Never hard-kill a game with FFB live.
5. **Autocentre may only be written on an unacquired device** (`0x800700AA`
   otherwise). Restore it last, after `Unacquire`.
6. **Rank FFB-capable devices first and blocklist virtual pads** (vJoy, ViGEm,
   XOutput, vXbox). vJoy reports 8 axes / 128 buttons like a MOZA R12 and
   advertises FFB. Prefer VID:PID as the device key.
7. **Never switch a game's input library backend at runtime** (Rewired
   `ResetAll()` killed the menus four times). Read the device directly instead.
8. **No physics or assist changes in this repo**, ever. Those are per-game
   decisions with per-game online gates.
9. **Manifests, configs and knowledge are data.** Nothing here executes game
   files or ships game assets.

## Decisions

- **ADR-0001**: DirectInput is the primary FFB backend on Windows; SDL2 haptics
  is a secondary backend behind the same interface for processes that already
  carry SDL (MAME forks, cannonball). On Windows SDL2 *is* a DirectInput
  wrapper, so it cannot be better at the hardware; it trades control and
  diagnosability for portability and ergonomics. Which one *feels* better is
  settled by the conformance probe, not by claim. See `docs/adr/0001-ffb-output-backends.md`.

## Sequence (from the proposal)

1. Repo, knowledge base, portfolio cards — this commit begins it.
2. Telemetry library + existing tests (from art-of-sim-rally), consumed by both Unity mods.
3. Native FFB core unified on art-of-sim-rally's superset, with OutRun's lifecycle rules and cruisn's watchdog; both Unity mods ship one binary.
4. Device identification + wheel-db quirk table.
5. Input kit, IL2CPP-safe IMGUI widgets, templates, bootstrap checklist.
6. Deferred: proxy scaffold merge, InputBridge dedupe.

## Testing

```powershell
dotnet test Dbce.Wheel.sln
.\native\wheelffb\build.ps1        # verifies exports
```

Nothing in this repo needs a game installed. Device-level behaviour is
verified with `tools/ffbprobe` against a real base and recorded in
`knowledge/`, with the wheel model and date.

## Working on this machine

- The owner's rig: MOZA R12 base, SIMAGIC DS-8X shifter, MOZA multi-function
  stalk; Steam on `D:\Program Files (x86)\Steam`. Fanatec and MOZA R5
  behaviour comes from user support bundles, not local hardware.
- `Stop-Process` from a Bash-spawned PowerShell does not stop a game; the
  native PowerShell tool does. Deploy DLLs only when the game is closed.
- Long heredocs in the Bash tool get mangled; write scripts to a file first.
- `ilspycmd` is installed for decompiling game assemblies.
