# dbce-wheel-mod-toolkit — agent guide

Portfolio rules: `E:\Source\dbce-project-mgmt\HOW-WE-WORK.md` (read first; it replaces older process, lease, review and testing instructions). Current focus: `E:\Source\dbce-project-mgmt\NOW.md`. Feature status lives in the portfolio grid (`python E:\Source\dbce-project-mgmt\tools\portfolio_grid.py plan <game>`), not in this file.

## Purpose
Shared core for mods that add force feedback, telemetry and wheel support to racing games: device to forces, frame to telemetry, device identification, diagnostics, installers, shared input shim and F6 panel code, knowledge base and templates. The game side (hooks, samplers, force signals, cameras, game UI) stays in each game's repo.

## What is in it
- `native/wheelffb` — C-ABI DirectInput FFB DLL (`include/wheelffb.h`, `WheelFfb.def`). Other C++ modules: `native/forza`, `forcemodel`, `shim`, `panel`, `ue4ss-panel`, `session`, `settings`, `telemetry`, `support`.
- `dotnet/` — `Dbce.Wheel.Telemetry`, `Ffb`, `Input`, `Shim`, `Playback`, `Recording`, `Course` (netstandard2.0) with matching `*.Tests`.
- `shared/` — display, ffb, session, support and UI code that games consume; `profiles/` — force tunes as data.
- `tools/` — `Sync-Toolkit.ps1`, `package/package.ps1`, `powershell/`, `playback/`, `window-capture/`, `ffbprobe/`, `Get-ConsumerStatus.ps1`.
- `knowledge/` (every item carries `[repo, YYYY-MM-DD]` provenance), `docs/` (`STATE.md`, `CONSUMERS.md`, `PLAN.md`, ADRs), `STANDARDS.md` (product standards, append-only).

## Build and test
- `dotnet test Dbce.Wheel.sln`; `native\wheelffb\build.ps1` (x64, checks exports against the header); `native\wheelffb\build.bat x86` (32-bit games); `native\forza\build_test.bat`; `native\forcemodel\build_conformance.bat`; `tools\powershell\Test-DbceWheel.ps1`; `tools\tests\Test-SyncToolkit.ps1`; `tools\tests\Test-NativeBinding.ps1`.
- `tools\package\package.ps1 -Version X.Y.Z` runs the offline suites and builds the zip in `dist\`.
- Hardware smoke (`native\wheelffb\test\build_smoke.bat x86`) needs a wheel and applies no torque without `--force`; run it whenever the native DLL changes.

## How games consume it
- `tools\Sync-Toolkit.ps1 -Version <tag>` (release) or `-LocalRepo <path>` (local checkout) vendors native, dotnet, include, profiles, powershell, tools or knowledge parts into a game repo and records provenance. Wheelkit installs and configures the resulting mod packages.
- `docs/CONSUMERS.md` is the register of which game uses which version; `tools\Get-ConsumerStatus.ps1` reports it.

## Changing it safely
- This repo is public: commit, but the owner pushes. The branch is `master`.
- Small commits; `git pull --rebase` first; run the affected tests and keep consumers building.
- The C ABI is the contract: add exports, never change or remove one. `BOOL` is the 4-byte Win32 BOOL, never C++ `bool`.
- One DirectInput instance per process; resolve `DirectInput8Create` from `system32` by full path; zero the wheel while the game window still exists; never hard-kill a game with FFB live.
- No physics or assist changes here; no game files or assets. Keep the classic `.sln` format. No secrets, device GUIDs or personal profiles.

## Key docs
- `STANDARDS.md` (product standards), `docs/STATE.md`, `docs/CONSUMERS.md`, `README.md`, `docs/adr/` (ADR-0001 FFB output backends), `docs/RECORDING-PLAYBACK-RUNBOOK.md`

<!-- BEGIN project-file-location -->
## Project files and temporary work

Keep project-related research, evidence, scripts, packages, and other working files inside the relevant project folder whenever practical. Keep disposable or machine-specific outputs git-ignored when they do not belong in version control; never ignore files that should be committed merely to hide them. When a project folder is unsuitable, use `E:\Source\temp\<project>` instead of creating scattered drive-root folders. Record the location of material needed by later work and update references when relocating it.

Clean up temporary files and disposable copies promptly once they are no longer useful. Preserve active work, user data, settings, saves, required evidence, backups, rollback artifacts, and worktrees until their retention or replacement is established. Follow existing project, rig, and data-loss safeguards; do not remove files merely because they are old or untracked.
<!-- END project-file-location -->
