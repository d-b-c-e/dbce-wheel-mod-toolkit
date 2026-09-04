# Changelog

Notable changes to dbce-wheel-mod-toolkit. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The native C ABI
only ever gains exports.

## [0.2.1] - 2026-09-04

### Fixed

- **`Set-IniValueCrlf` duplicated a key written as `key = value`.** The key
  regex required the tight `key=value` form, so on a config using spaces around
  the separator - OutRun 2006's does - it appended a second line instead of
  replacing the first, leaving two copies of the key and letting the game's
  parser decide which wins. Found by using it on a real game config. An
  existing line now keeps its indent and separator exactly; a newly appended
  key copies the spacing of a sibling key in its section. Four cases added,
  plus an assertion on every existing case that the key appears exactly once
  afterwards.

### Added

- **`docs/CONSUMERS.md`** - the register of who vendors what, where each pin
  lives, and which area of a release obliges which consumer to move.
- **`tools/Get-ConsumerStatus.ps1`** - reads each consumer's real pin off disk
  and prints the `Sync-Toolkit.ps1` command for anything behind, so the
  register cannot quietly go stale.

## [0.2.0] - 2026-09-04

The native half. 0.1.0 could serve a Unity mod; this release is what a native
mod - a dinput8 proxy - needs, and the three consumers that needed it are on it.

### Added

- **Hardware periodic effects** in `native/wheelffb`: `CreatePeriodicEffect`
  (returns a slot), `UpdatePeriodicEffect` (envelope, safe to call every frame),
  `ReleasePeriodicEffects`. Road texture and tyre slip rendered by the wheelbase
  instead of synthesised through the constant force, which loses ~26% to
  zero-order-hold roll-off at 60 Hz and another 30-60% to soft saturation when
  it rides on steering load. Carries OutRun2006Tweaks-FFB's driver
  update-strategy probe (`SetParameters` without `DIEP_START` preferred),
  its recreate-after-handle-loss hold-off, and its change gate.
- **`SetPreferredDeviceGuid`**: pick the wheel by DirectInput instance GUID,
  beating index and name. For a mod whose own input layer already chose a
  device - matching by name is guesswork when a Fanatec base shows two
  identically named devices.
- **`native/wheelffb/include/wheelffb.h`**: the C ABI as one macro list, with a
  runtime loader (`WheelFfb_LoadBeside`) so a missing DLL disables force
  feedback rather than stopping the game from starting. `build.ps1` fails if the
  header and `WheelFfb.def` disagree.
- **`native/forza/forza_packet.h`**: the Forza encoder as a header-only C++11
  file for native mods, with a conformance test mirroring the C# one.
- **x86 build**: `build.bat x86`, for 32-bit games. Both builds verified at 32
  undecorated exports.
- **`native/wheelffb/test`**: a smoke test that loads the DLL the way a proxy
  does and walks every stage against a real wheel, applying no torque unless
  asked.
- **`tools/powershell/DbceWheel.psm1`**: registry Steam discovery, DirectInput
  enumeration with the two COM marshalling workarounds, CRLF-safe INI editing,
  and a test suite.

### Fixed

- **An effect whose `Start` lost a race with a focus change never played.**
  `Start` returning `DIERR_NOTEXCLUSIVEACQUIRED` was survivable for the constant
  force, because every update carries `DIEP_START`, but not for a periodic
  effect, whose updates deliberately do not. `Start` is now retried through a
  re-acquire, each slot remembers whether it truly started, and until it has,
  its updates carry `DIEP_START` whatever the probed strategy says. Found by the
  new smoke test on its first run.
- **`DirectInput8Create` is resolved from `system32` by full path.** Inside a
  game modded with a `dinput8.dll` proxy, an import bound to the proxy beside
  the game exe - the layer the toolkit is meant to sit alongside, not inside.
- **`Set-IniValueCrlf` split a CRLF** when appending a key to a file's last
  section: .NET's `\Z` also matches before a trailing newline. Latent in
  milestone-wheel-tools, which only ever replaced.

### Consumers

- **iracing-arcade-wheel** on the telemetry library and the native DLL.
- **milestone-wheel-tools** on `forza_packet.h` and `DbceWheel.psm1`; Steam
  discovery had been written out three times in that one repo.
- **OutRun2006Tweaks-FFB** on the C ABI: 543 lines of its own DirectInput code
  removed, its force model untouched.

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
