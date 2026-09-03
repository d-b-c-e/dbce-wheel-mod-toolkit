# Implementation plan

*Started 2026-09-03. Each step leaves `main` building and tests green. Ticks
are updated as work lands; anything unticked at the end of a session is the
next session's starting point.*

## Ground rules for this pass

- `iracing-arcade-wheel` is not modified: another session is working in it.
  It gets a message when the telemetry library is consumable.
- `art-of-sim-rally` changes only where the result is verifiable
  byte-for-byte (unit tests) or by a smoke launch (mod loads, FFB initialises,
  devices read). Nothing is deployed to the owner's install while the game is
  running.
- Consumers vendor **built** artifacts, pinned by version. No cross-repo
  `ProjectReference`.

## Step 1 — Telemetry library `dotnet/Dbce.Wheel.Telemetry`

- [x] Copy `ForzaPacket`, `TelemetryFrame`, `TelemetrySender` from
      art-of-sim-rally (`netstandard2.0`, no Unity refs), namespace
      `Dbce.Wheel.Telemetry`.
- [x] `ForzaLayout` enum: `Sled232`, `Fm7Dash311`, `Horizon324`; encoder
      emits any of the three. Offsets from milestone `docs/forza-format.md`
      and OutRun's `ForzaDashPacket`.
- [x] `IdleKeeper`: zeroed-but-not-degenerate frames while no game is live
      (same max/idle RPM constants; sentinel byte), from cruisn's ForzaKeeper.
- [x] `RpmSynthesizer`: plausible RPM from speed and a gear-ratio table for
      games with no RPM, from OutRun/cruisn.
- [x] Tests: move `ForzaPacketTests.cs` (210 lines) and add 311/232 layout,
      keeper and synthesizer tests. `Dbce.Wheel.sln` (classic `.sln`).
- [x] `tools/forza/forza_probe.py` and `forza_synth.py` — one copy each,
      layout detected by packet length.

## Step 2 — Knowledge base

- [x] `DIRECTINPUT-FFB.md`, `FFB-SIGNAL-DESIGN.md`, `DEVICE-QUIRKS.md`,
      `TELEMETRY-FORZA.md`, `REWIRED.md`, `UNATTENDED-TESTING.md` — written
      from first-hand material with provenance.
- [x] Engine modding pages, so the next game on the same engine starts from
      the lessons: `UNITY-MODDING.md` (Mono vs IL2CPP, UMM vs BepInEx, Harmony
      patterns, finding the car, cameras, input libraries, panels, packaging),
      plus per-game case files under `knowledge/games/` (art of rally, iRacing
      Arcade, Cruis'n, OutRun 2006, Gravel): engine, loader, what worked, what
      did not, where the code is.
- [x] `UNITY-IL2CPP-BEPINEX.md`, `UNITY-INPUT-SYSTEM.md`, `UE4.md`,
      `FMOD-AUDIO-AS-TELEMETRY.md`, `MAME.md`, `DINPUT8-PROXY.md`,
      `SUPPORT-BUNDLES.md`, `NEW-GAME-CHECKLIST.md` — from the survey.

## Step 3 — Native FFB core `native/wheelffb`

- [x] Seed from art-of-sim-rally's `UnityForceFeedback.cpp` (superset).
      Output `WheelFfb.dll`; consumers rename at vendor time if a game
      dictates the file name. All existing exports kept.
- [x] Log path parameterised: `DBCE_FFB_LOG_PATH` env var, default
      `%LOCALAPPDATA%\DbceWheel\ffb.log`, `SetLogPath()` export.
- [x] OutRun lifecycle rules: `PanicStop()` (zero → stop → `STOPALL` →
      `SETACTUATORSOFF` → `RESET` → unacquire → autocentre restore last);
      `InstallExitGuards()` window subclass on `WM_CLOSE` /
      `WM_QUERYENDSESSION` / `WM_DESTROY`; recreate-and-ramp (from zero over
      ~250 ms) instead of stepping back after an effect recreation.
- [x] cruisn hold watchdog: `SetHoldTimeoutMs()` releases force when no
      update arrives in time (off by default).
- [x] `GetLastHResult()`, `GetWheelFfbVersion()` exports (`GetVersion` collides with Win32). `build.ps1` verifies the
      full export list.
- [x] Smoke: P/Invoke from PowerShell enumerates devices with the MOZA on.

## Step 4 — Managed FFB library `dotnet/Dbce.Wheel.Ffb`

- [x] `WheelFfbNative`: P/Invoke binding, loads by absolute path
      (`LoadLibraryW` preload; CoreCLR resolver via reflection when present).
- [x] `ForceShaper`: normalise, gain, invert, clamp, dual-rate EMA, slew
      with event bypass, low-speed fade, warm-up ramp.
- [x] `ForceModel`: composable terms over `TelemetryFrame` — spring, damper,
      lateral load × grip factor, weight transfer, texture, events — with
      `Arcade` (OutRun constants) and `SimLite` (lateral force × trail)
      presets, and **input validators** (liveness, range, sign-by-speed-band).
- [x] `DevicePreference`: VID:PID / name / index parsing; virtual-pad list.
- [x] `FfbDiagnostics`: peak logger, 5 Hz trace line, support-bundle section.
- [x] Tests for shaper and model (pure math).

## Step 5 — Tools, packaging, release

- [x] `tools/ffbprobe/` standalone DirectInput probe (from the art of rally
      session) with build script.
- [x] `tools/dev/` unattended-test helpers (`press.ps1`, `shot.ps1`).
- [x] `tools/Sync-Toolkit.ps1` template for consumers (pinned version →
      `lib/toolkit/`).
- [x] `tools/package/package.ps1` → `dist/dbce-wheel-mod-toolkit-<ver>.zip`
      (native DLL, managed DLLs, tools, knowledge).
- [x] Tag `v0.1.0`, GitHub release with artifacts.

## Step 6 — Consumer integration

- [x] art-of-sim-rally: vendor `Dbce.Wheel.Telemetry.dll`, retarget
      `TelemetryPump` usings, drop the local Telemetry project and its tests
      (they live here now), update `package.ps1`. Build, test, smoke launch.
- [x] art-of-sim-rally: vendor `WheelFfb.dll` as `UnityForceFeedback.dll`
      (the P/Invoke name the mod uses), keep the log path via `SetLogPath`.
      Smoke launch: FFB initialised, three devices read.
- [ ] Message the iracing-arcade-wheel session with the consumption recipe.
- [ ] abm-personal: status cards for the five repos and this one.

## Deferred

- SDL2 backend in `native/wheelffb` (seed: cruisn `mvffb`).
- Direct-read input kit and Unity Input System kit (`dotnet/Dbce.Wheel.Input`).
- dinput8 proxy scaffold merge; InputBridge dedupe.
- Templates.
