# dbce-wheel-mod-toolkit — agent guide

Portfolio rules: `E:\Source\dbce-project-mgmt\HOW-WE-WORK.md` (read first; it replaces older process, lease, review and testing instructions). Current focus: `E:\Source\dbce-project-mgmt\NOW.md`. Feature status lives in the portfolio grid (`python E:\Source\dbce-project-mgmt\tools\portfolio_grid.py plan <game>`), not in this file.

## Purpose
Shared core for mods that add force feedback, telemetry and wheel support to racing games: device to forces, frame to telemetry, device identification, diagnostics, installers, shared input shim and F6 panel code, knowledge base and templates. The game side (hooks, samplers, force signals, cameras, game UI) stays in each game's repo.

## What is in it
- `native/wheelffb` — C-ABI DirectInput FFB DLL (`include/wheelffb.h`, `WheelFfb.def`). Other C++ modules: `native/forza`, `forcemodel`, `shim`, `panel`, `ue4ss-panel`, `session`, `settings`, `telemetry`, `support`. `native/ui-host` builds `dbce_ui.dll` (x86 and x64), the F6 panel core for non-native mods (`dbce_ui.h`; C# wrapper `shared/ui/DbceUi.cs`, compiled for net35 and net6.0 by `dotnet/Dbce.Wheel.UiHost`). `native/ui-exe` builds `dbce-ui.exe`, the same core in a normal window for game-closed setup (`--setup <dbce-mod.json>`). `native/selftest/dbce_selftest.hpp` is the in-mod self-test interpreter (`docs/SELFTEST.md`), ticked by the panel's `Config::selfTestTick`.
- `dotnet/` — `Dbce.Wheel.Telemetry`, `Ffb`, `Input`, `Display`, `Ui`, `Shim`, `Playback`, `Recording`, `Course` (netstandard2.0) with matching `*.Tests`. `Display` and `Ui` build the engine-free sources in `shared/display`, `shared/ui` and `shared/support`.
- `shared/` — display, ffb, session, support and UI code that games consume; `profiles/` — force tunes as data.
- `unity/` — the shared Unity layer compiled into each plugin, and `Dbce.Unity.props` (runtime defines; the DLLs, or the same sources on Mono 2.0). `installer/` — slot for the shared installer.
- `tools/` — `Sync-Toolkit.ps1`, `Sync-Native.ps1`, `package/package.ps1` (the toolkit zip), `package/New-ModPackage.ps1` (a game's player zip from its `dbce-mod.json`), `powershell/`, `playback/`, `window-capture/`, `ffbprobe/`, `Get-ConsumerStatus.ps1`; post-build checks `Check-Net35Api.ps1` (Mono 2.0) and `Find-StrippedInteropCalls.ps1` (IL2CPP).
- `schema/dbce-mod.schema.json` — the one per-mod file `dbce-mod.json` (`docs/DBCE-MOD-JSON.md`), with fixtures; readers in `installer/DbceModFile.ps1`, `shared/mod/DbceModFile.cs` and `native/panel/dbce_mod_file.h`, tested by `tools\tests\Test-DbceModFile.ps1`, `dotnet run --project dotnet/Dbce.Wheel.ModFile.Tests` and `native\panel\build_test.ps1`. `schema/dbce-selftest.schema.json` and `schema/dbce-selftest-result.schema.json` are the self-test script and `result.json`, with fixtures in `schema/fixtures/selftest`.
- `schema/dbce-store-profiles.schema.json` and `schema/dbce-store-runtime.schema.json` — the shared store in `%LOCALAPPDATA%\Wheelkit\store` (profiles, the shared cockpit, each game's profile, each game's runtime settings; `docs/STORE.md`). Files only gain fields. Readers in `installer/DbceStore.ps1`, `shared/mod/DbceStore.cs` and `native/panel/dbce_store.h`, tested by `tools\tests\Test-DbceStore.ps1` and the same two reader tests.
- `knowledge/` (every item carries `[repo, YYYY-MM-DD]` provenance), `docs/` (`STATE.md`, `CONSUMERS.md`, `PLAN.md`, ADRs), `STANDARDS.md` (product standards, append-only).

## Build and test
- `dotnet test Dbce.Wheel.sln`; `native\wheelffb\build.ps1` (x64, checks exports against the header); `native\wheelffb\build.bat x86` (32-bit games); `native\forza\build_test.bat`; `native\forcemodel\build_conformance.bat`; `native\panel\build_test.ps1`; `native\ui-host\build.ps1` (dbce_ui.dll x86 and x64, WARP host check, 2 MB limit); `native\ui-exe\build.ps1` (dbce-ui.exe, `--setup` byte-diff check, 5 MB limit); `native\selftest\build_test.ps1` (self-test interpreter on a fake host, every fixture); `tools\tests\Test-SelfTestSchema.ps1`; `tools\tests\Test-DbceStore.ps1`; `tools\powershell\Test-DbceWheel.ps1`; `tools\tests\Test-SyncToolkit.ps1`; `tools\tests\Test-UnityConsumer.ps1` (dummy plugin for Mono, Mono 2.0 and IL2CPP); `tools\tests\Test-SyncNative.ps1`; `tools\tests\Test-NativeBinding.ps1`; `tools\tests\Test-ModPackage.ps1` (player packager and `installer/Install.ps1` on a fixture game folder).
- `tools\package\package.ps1 -Version X.Y.Z` runs the offline suites and builds the zip in `dist\`.
- Hardware smoke (`native\wheelffb\test\build_smoke.bat x86`) needs a wheel and applies no torque without `--force`; run it whenever the native DLL changes.

## How games consume it
- `tools\Sync-Toolkit.ps1 -Version <tag>` (release) or `-LocalRepo <path>` (local checkout) vendors native, dotnet, include, profiles, powershell, tools, knowledge, shared, unity or installer parts into a game repo. Its SHA-256 `MANIFEST.txt` makes it refuse to overwrite vendored files edited in the game. Wheelkit installs and configures the resulting mod packages.
- Native dinput8-proxy games use `tools\Sync-Native.ps1 -GameRepo <game> [-Commit <wheel>] [-TripleCommit <triple>]` instead: it copies a fixed folder list (no tests) into `src/vendor` with a two-line `VERSION`. `-Check` exits 1 when any vendored file differs from those commits; `-DryRun` lists the deltas.
- `docs/CONSUMERS.md` is the register of which game uses which version; `tools\Get-ConsumerStatus.ps1` reports it.

## Changing it safely
- This repo is public; lanes commit and push here (owner, 2026-11-06). The branch is `master`: `git pull --rebase` before pushing.
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
