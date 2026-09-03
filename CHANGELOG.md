# Changelog

Notable changes to dbce-wheel-mod-toolkit. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The native C ABI
only ever gains exports.

## [0.1.0] - 2026-09-03

First release: the foundation, built while the survey of five racing-mod repos
was fresh. Nothing here is new capability; it is the capability that existed in
two to six copies, made one.

### Added

- **`native/wheelffb` — `WheelFfb.dll`**, the DirectInput force-feedback and
  device layer, seeded from art-of-sim-rally's `UnityForceFeedback.dll` (the
  superset) with OutRun2006Tweaks-FFB's lifecycle rules and cruisn-collection's
  hold watchdog folded in: one DirectInput instance per process, FFB-candidate
  fallback when the preferred device has no actuator, device capability query,
  axis reading through the held handle, `NOTEXCLUSIVEACQUIRED` re-acquire,
  own-window binding, `PanicStop`, `InstallExitGuards`, ramp-in after effect
  recreation, `SetHoldTimeoutMs`, `SetLogPath`, `GetLastHResult`,
  `GetWheelFfbVersion`. All 21 original exports kept. 28 exports verified by
  the build.
- **`Dbce.Wheel.Telemetry`** (`netstandard2.0`): the Forza "Data Out" encoder
  in all three layouts (232 sled, 311 FM7 dash, 324 Horizon), byte-identical
  to art-of-sim-rally's for Horizon; `IdleKeeper` (never a degenerate zero
  packet; gear 1); `RpmSynthesizer`. 39 tests, including the original suite.
- **`Dbce.Wheel.Ffb`** (`netstandard2.0`): `WheelFfbNative` binding that loads
  by absolute path on Mono and CoreCLR; `ForceShaper` conditioning chain;
  `ForceModel` with `SimLite` and `Arcade` presets and `InputValidator`
  (dead-input and sign-by-speed-band checks); `DevicePreference` (vid:pid /
  #index / name, virtual-pad blocklist); `FfbDiagnostics`. 21 tests.
- **Tools**: one `forza_probe.py` and one `forza_synth.py` (any layout),
  `ffbprobe` (scripted constant-force encodings against a real base),
  `press.ps1`/`shot.ps1` for unattended tests, `Sync-Toolkit.ps1` for
  consumers, `package.ps1`.
- **Knowledge base**: 15 pages with `[repo, date]` provenance and five
  per-game case files.
- **Docs**: proposal, implementation plan, rebuild roadmap, ADR-0001 (FFB
  output backends: DirectInput primary, SDL2 secondary, one interface).

### Not yet

SDL2 backend; direct-read input kit and Unity Input System kit; templates;
consumer integration (art-of-sim-rally first).
