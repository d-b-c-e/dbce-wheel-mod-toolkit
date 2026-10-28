# dbce-wheel-mod-toolkit — Codex working notes

**October 11 FFB reference update:** STD-031 supersedes the old level below.
Art source `8bc94b1` and `profiles/normalization/art-of-rally-50-v2.json` make
new default steering 50 match legacy30 by setting the reference to 11500/0.6 N.
Independent effects and explicit saved tunes are unchanged. Original-input
paired trials pass; installation/runtime adoption and feel remain pending.
Do not pair the v2 label with v1 output CSVs. See docs/FFB-NORMALIZATION.md.

**Historical FFB reference (owner, 2026-10-06):** Art of Rally is the fixed gold standard
at Strength 50 (STD-021). Use `profiles/normalization/art-of-rally-50-v1.json`
with the exact source/defaults/per-effect requests. Tune other games at their
50 setting; do not retune Art or substitute other displayed strengths. Original
signals survive physical-output muting; final feel remains an attended check.

**FFB normalization (2026-10-06):** read `docs/FFB-NORMALIZATION.md` and
`knowledge/FFB-NORMALIZATION-2026-10-06.md`. Offline baselines/trials cover Art,
Woden, iRacing and DRIVE; F-Zero has a labelled synthetic component response.
Use original analysis signals and actual production models, preserve effect
families and immutable evidence. Runtime tunes/physical acceptance remain open.

**Recording/playback:** start with `docs/RECORDING-PLAYBACK-RUNBOOK.md` for exact
commands, known-good Art reference, stop/recovery and adapter status. Read
`knowledge/RECORDING-PLAYBACK-LESSONS.md` before changing the approach. These
supersede earlier input-replay assumptions; never infer deterministic physics
or physical force from trajectory/signal replay. Shared APIs: `docs/PLAYBACK-CORE.md`.

> **Resuming a session? Read `docs/STATE.md` first.** It records what is
> verified and how, what is built but has never been driven in a game, the next
> actions in priority order, and the assumptions still worth re-checking. Do not
> describe a route as working if STATE.md lists it as never having run.

## Repository Purpose

This isolated runtime branch adds experimental `0.16.0-external.1` (native
0.10.0, 55 exports) from preserved POV source `397a5f22`. Read
[EXTERNAL-FFB.md](docs/EXTERNAL-FFB.md) and `dist/EXTERNAL-HANDOFF.json` for its
contract and exact clean-source provenance. It is not an official release or
hardware acceptance. The standing desktop pause forbids device discovery,
acquisition, shown windows, game/virtual-pad/input use and real torque. Use
`tools/package/package.ps1 -Offline`; compile smoke only. Keep the prior
`0.16.0-pov.1` artifacts immutable and do not alter the frozen v0.15 checkout.
External policy and device locks must remain separate; the device gate must
also reject same-thread driver reentry, not only concurrent threads. New tests
must stay on fake COM/platform calls until an explicitly coordinated hardware slot.

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

Consumers, and what each is on. **`docs/CONSUMERS.md` is the register**;
`tools/Get-ConsumerStatus.ps1` reads the real pins off disk and says who is
behind, so prefer it over the table below, which is a summary:

| Repo | Runtime | On |
|---|---|---|
| `art-of-sim-rally` | Unity 2019 Mono, Unity Mod Manager | 0.2.0 — telemetry + native DLL |
| `iracing-arcade-wheel` | Unity 6 IL2CPP, BepInEx 6 | 0.2.0 — telemetry + native DLL. **`lib/` is gitignored there**; its pin lives in its CHANGELOG |
| `OutRun2006Tweaks-FFB` | native dinput8 proxy, x86 | 0.2.0 — the whole FFB device layer via the C ABI |
| `milestone-wheel-tools` | native dinput8 proxy, UE4 | 0.2.0 — `forza_packet.h` + `DbceWheel.psm1` |
| `cruisn-collection` | MAME fork, SDL2 | not yet; it is the seed for the SDL2 backend, not a consumer of the DirectInput one |

Adjacent: `cannonball-se`, `mame-racing`, `xoutput-redux`, `wheel-db`,
`controller-input-db`.

The founding survey and rationale: `docs/PROPOSAL.md` (2026-09-03).

## Repository Structure

| Path | Contents |
|---|---|
| `native/wheelffb/` | C-ABI DLL: FFB output + device layer. DirectInput backend (primary); SDL2 backend is planned, not written. See ADR-0001. |
| `native/wheelffb/include/wheelffb.h` | **The C ABI**, as one `WHEELFFB_API_LIST` macro plus a runtime loader (`WheelFfb_LoadBeside`). Native consumers include this and never link an import library. `build.ps1` fails if it disagrees with `WheelFfb.def`. |
| `native/wheelffb/test/` | `smoke.c` — walks the whole ABI against a real wheel. **Applies no torque unless given `--force`**, so it is safe beside a live direct-drive base. |
| `native/shim/` | The shared input shim behind STD-033 shared controls (Core, nonexclusive DirectInput provider, host) and `dbce_shim.dll`'s C ABI 2. **The canonical source**, moved from outrun2-decomp 2026-10-13: consumers vendor or rebuild from here. Game adapters stay in their repos. `build_test.ps1` builds the DLL (clang-cl, /MT) and runs its fake-device checks; `PROVENANCE.md` lists the variants consumers still ship. |
| `native/forza/` | `forza_packet.h`, the Forza encoder as a header-only C++11 file for native mods, plus its conformance test. |
| `native/forcemodel/` | `force_model.h` and `force_profile.h` — the C++ port of the force model and the profile reader, plus the cross-language conformance check and its golden file. |
| `profiles/` | `force-profiles.ini` — **the tunes, as data.** Deployed beside the game and read at runtime, so a feel can be swapped without a build. Versions are immutable; add one rather than editing one. |
| `dotnet/Dbce.Wheel.Telemetry/` | `netstandard2.0`. Forza "Data Out" 232/311/324 encoders, `TelemetryFrame` (the car-state seam), idle keeper, RPM synthesis. Tests alongside. |
| `dotnet/Dbce.Wheel.Ffb/` | `netstandard2.0`. Cdecl delegates bound to the exact native module (Mono-compatible APIs and CoreCLR), force-shaping chain, `ForceModel`, device selection, diagnostics. |
| `dotnet/Dbce.Wheel.Shim/` | `net6.0`. Managed ABI 2 loading, action/frame contract and exact Controls section/Core-hash checks for IL2CPP consumers. Consumer receipt, device isolation and game hooks stay in game repos. Fixtures alongside. |
| `tools/powershell/` | `DbceWheel.psm1` — registry Steam discovery, DirectInput enumeration (with the two COM marshalling workarounds), CRLF-safe INI editing — and its test suite. |
| `tools/` | `Sync-Toolkit.ps1` (consumers vendor with this), `ffbprobe/` (scripted constant force against a real base), `forza/` (probe and synth), `dev/` (unattended-test helpers), `package/package.ps1`. |
| `knowledge/` | The knowledge base. Every page names the repo and date each item came from. `knowledge/README.md` is the index; `knowledge/games/` holds the per-consumer case files. |
| `docs/adr/` | Architecture decisions. Start with ADR-0001 (FFB output backends). |

**No general input/calibration assembly yet** — `dotnet/Dbce.Wheel.Input/`
contains vendorable binding and input source but no project. The managed Shim
project is an ABI bridge, not a game adapter or calibration UI. `templates/`
contains README and working-notes starting points, not compilable projects or
shared UI. See [POV input](docs/POV-INPUT.md).

## Runtime matrix — the reason the core is native

| Consumer runtime | Managed lib target | Native loading |
|---|---|---|
| Unity 2019.4 Mono (UMM) | `netstandard2.0`, **no Unity references** | `LoadLibraryW` plus Cdecl export delegates |
| .NET 6 CoreCLR (BepInEx 6 IL2CPP) | same assembly | the same Cdecl export delegates |
| Native C++ (proxies, MAME forks) | n/a | link or `LoadLibrary` the C ABI |
| Python / PowerShell tooling | n/a | ctypes / `Add-Type` against the C ABI |

`netstandard2.0` with zero Unity or loader references is what lets one
assembly load in both Unity runtimes and be unit-tested on modern .NET. Do
not add a Unity reference to a shared managed project; Unity-specific code
goes in a clearly named sub-package.

## Conventions

- **Solution stays classic `.sln`** (`dotnet new sln --format sln`); the .NET 10
  SDK's `.slnx` cannot be opened by older SDKs.
- Native DLL builds **x64 and x86** with MSVC (`build.bat x86` for 32-bit games
  such as OutRun 2006; output goes to `build\x86\`). `build.ps1` verifies every
  export with `dumpbin` *and* checks `include/wheelffb.h` against `WheelFfb.def`.
  A mangled or missing export fails silently in-game.
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
4. **Retry `Start()` through a re-acquire, and force `DIEP_START` until a slot
   has really started.** `Start` returning `DIERR_NOTEXCLUSIVEACQUIRED` is
   survivable for a constant force, because every update carries `DIEP_START`.
   A periodic effect's updates deliberately do not, so a slot that lost a race
   with a focus change sets its parameters perfectly and plays nothing for the
   whole session. Never let the update-strategy probe draw a conclusion from an
   effect that never started (2026-09-04, found by `test/smoke.c` on its first
   run).
5. **Resolve `DirectInput8Create` from `system32` by full path.** Inside a game
   modded with a `dinput8.dll` proxy - which is most of the native consumers -
   an import named `dinput8.dll` binds to the proxy beside the game exe, so
   this layer ends up running *through* the layer it is meant to sit beside.
6. **Zero the wheel while the game window still exists.** Hook `WM_CLOSE`,
   `WM_QUERYENDSESSION` and `ExitProcess`; by `DLL_PROCESS_DETACH` nothing
   reaches the device. A hold watchdog releases force when the game stops
   writing. Never hard-kill a game with FFB live.
7. **Autocentre may only be written on an unacquired device** (`0x800700AA`
   otherwise). Restore it last, after `Unacquire`.
8. **Rank FFB-capable devices first and blocklist virtual pads** (vJoy, ViGEm,
   XOutput, vXbox). vJoy reports 8 axes / 128 buttons like a MOZA R12 and
   advertises FFB. Prefer VID:PID as the device key.
9. **Never switch a game's input library backend at runtime** (Rewired
   `ResetAll()` killed the menus four times). Read the device directly instead.
10. **No physics or assist changes in this repo**, ever. Those are per-game
   decisions with per-game online gates.
11. **Manifests, configs and knowledge are data.** Nothing here executes game
   files or ships game assets.

## Decisions

- **ADR-0001**: DirectInput is the primary FFB backend on Windows; SDL2 haptics
  is a secondary backend behind the same interface for processes that already
  carry SDL (MAME forks, cannonball). On Windows SDL2 *is* a DirectInput
  wrapper, so it cannot be better at the hardware; it trades control and
  diagnosability for portability and ergonomics. Which one *feels* better is
  settled by the conformance probe, not by claim. See `docs/adr/0001-ffb-output-backends.md`.

## Where the work stands

Source version 0.12.0 adds transactional sync, exact-module binding, strict GUID
selection, input-slot identity preservation, panic recovery, AxleForceCurve
compatibility v1, finite force-state recovery and structured consumer diagnostics.
Read docs/STATE.md and docs/CONSUMER-EXPERIENCE.md before making adoption claims.
The native component is now **0.11.0 with 60 exports** on master (2026-10-12: GetReadSlotCaps for
the OutRun input shim's WheelFfb bridge; before it 0.10.0 plus the test-injection exports, merged
2026-10-04: opt-in force tracing, shaped sine bursts, finite constant impacts,
POV snapshots, actuator rearm on every open, persistent-effect invalidation on
reacquire, opt-in external sessions, attended comparison tool). Package and
component versions differ. **Native lock order:** `g_apiMutex` (every export)
-> `g_lock` (effects) -> external policy lock; never join the hold watchdog
under `g_lock`; `FreeDirectInput` waits for it without a timeout. Legacy exports
check the external-session latch before and after taking `g_apiMutex`.
The dinput proxy header scaffold is implemented and tested on x86/x64.
Game UI, calibration, shared input kit, and SDL consolidation remain separate work.

## Testing

`tools/package/package.ps1` runs the offline suites and builds the release zip.
The hardware smoke is separate and requires a wheel; default operation is zero-output.

```powershell
dotnet test Dbce.Wheel.sln                     # managed regression suite
.\native\wheelffb\build.ps1                   # x64 + export/header check
.\native\wheelffb\build.bat x86               # 32-bit games
.\native\wheelffb\test\build_pov_read_test.bat # fake COM devices, no hardware acquisition
.\native\wheelffb\test\build_read_slot_caps_test.bat # GetReadSlotCaps on fake COM devices (x64|x86)
.\native\wheelffb\test\build_burst_test.bat     # then burst_test.exe --pre-init-worker; watchdog/lock regressions
.\native\wheelffb\test\build_constant_burst_test.bat; build_logging_test.bat; build_external_session_test.bat
.\native\wheelffb\test\build_constant_recovery_test.bat # then constant_recovery_test.exe loss|recreate|recreate-reject|boundaries|cancel
.\native\wheelffb\test\build_lifecycle_test.bat # actual DLL: export count/version, no device calls (all take x64|x86)
.\tools\attended-effects\test.bat            # policy only; NEVER launch attended_effects.exe unattended
.\native\forza\build_test.bat                 # packet conformance
.\native\forcemodel\build_conformance.bat     # C++ model vs the golden file
.\tools\powershell\Test-DbceWheel.ps1         # INI edge cases + discovery
.\tools\tests\Test-SyncToolkit.ps1          # real filesystem failures/rollback
.\tools\tests\Test-NativeBinding.ps1        # fake native DLL, no wheel acquisition
.\tools\package\package.ps1 -Version X.Y.Z    # all of the above, then dist\
```

**Run the smoke test whenever the native DLL changes.** It is the only thing
that exercises the ABI the way a consumer does, and it found a real bug on its
first run:

```powershell
.\native\wheelffb\test\build_smoke.bat x86    # then build\x86\smoke.exe
```

It applies **no torque** unless given `--force`, so it is safe to run unattended
beside a powered direct-drive base.

Nothing in this repo needs a game installed. Device-level behaviour is verified
against a real base and recorded in `knowledge/`, with the wheel model and date.

## Working on this machine

- The owner's rig: MOZA R12 base, SIMAGIC DS-8X shifter, MOZA multi-function
  stalk; Steam on `D:\Program Files (x86)\Steam`. Fanatec and MOZA R5
  behaviour comes from user support bundles, not local hardware.
- `Stop-Process` from a Bash-spawned PowerShell does not stop a game; the
  native PowerShell tool does. Deploy DLLs only when the game is closed.
- Long heredocs in the Bash tool get mangled; write scripts to a file first.
- `ilspycmd` is installed for decompiling game assemblies.

## Shared product UI guidance (owner, October 18)

Keep user interfaces simple and task-focused; do not expose every internal knob.
Read the October 18 simple-UI section of
`E:\Source\dbce-project-mgmt\docs\agent-work-policy.md` before UI work.
Use selective Advanced controls and a separate developer opt-in. Group visual
settings into explicit Off/Low/Medium/High/Ultra presets with expandable details
and Custom for overrides; preserve saved settings and essential setup workflows.

<!-- BEGIN project-file-location -->
## Project files and temporary work

Keep project-related research, evidence, scripts, packages, and other working files inside the relevant project folder whenever practical. Keep disposable or machine-specific outputs git-ignored when they do not belong in version control; never ignore files that should be committed merely to hide them. When a project folder is unsuitable, use `E:\Source\temp\<project>` instead of creating scattered drive-root folders. Record the location of material needed by later work and update references when relocating it.

Clean up temporary files and disposable copies promptly once they are no longer useful. Preserve active work, user data, settings, saves, required evidence, backups, rollback artifacts, and worktrees until their retention or replacement is established. Follow existing project, rig, and data-loss safeguards; do not remove files merely because they are old or untracked.
<!-- END project-file-location -->
