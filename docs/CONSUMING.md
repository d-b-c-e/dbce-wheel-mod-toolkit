# Consuming the toolkit from a game mod

Consumers vendor **built artifacts**, pinned by version. No cross-repo project
references: a toolkit change never reaches a game without an explicit,
diffable bump in that game's repo.

## 1. Vendor

Copy `tools/Sync-Toolkit.ps1` into the consumer's `tools/` and run it there:

```powershell
.\tools\Sync-Toolkit.ps1 -Version v0.1.0                       # from the GitHub release
.\tools\Sync-Toolkit.ps1 -LocalRepo E:\Source\dbce-wheel-mod-toolkit   # from a local build
```

It fills `lib\toolkit\native\WheelFfb.dll`, `lib\toolkit\dotnet\Dbce.Wheel.*.dll`
(+ xml docs) and writes `lib\toolkit\VERSION`. Un-ignore the folder
(`!lib/toolkit/` and `!lib/toolkit/**` after a `lib/` rule) so the pinned
binaries are committed with the mod.

## 2. Telemetry

Replace the local encoder project with a reference to the vendored assembly:

```xml
<Reference Include="Dbce.Wheel.Telemetry">
  <HintPath>..\..\lib\toolkit\dotnet\Dbce.Wheel.Telemetry.dll</HintPath>
  <Private>true</Private>
</Reference>
```

`using Dbce.Wheel.Telemetry;` — `ForzaPacket`, `TelemetryFrame`,
`TelemetrySender` are the same types the two Unity mods already had.
`ForzaPacket.Write(frame, buffer, ForzaLayout.Horizon324)` is byte-identical
to the old `Write(frame, buffer)`; `Fm7Dash311` and `Sled232` are new.
`IdleKeeper` keeps a dash at zero between stages; `RpmSynthesizer` invents a
plausible rpm for games without one. Ship `Dbce.Wheel.Telemetry.dll` beside
the mod and list it in the installer's expected files.

## 3. Native FFB layer

`WheelFfb.dll` keeps every export the two Unity mods' `UnityForceFeedback.dll`
had, so existing P/Invokes work unchanged. Ship it under the file name the
mod imports (art of rally: copy as `UnityForceFeedback.dll`). Recommended
calls in addition to what you already make:

| Call | When | Why |
|---|---|---|
| `SetLogPath(path)` | right after `LoadLibrary`, before `InitDirectInput` | keep your mod's historical `ffb.log` location; default is `%LOCALAPPDATA%\DbceWheel\ffb.log` |
| `InstallExitGuards()` | after `InitDirectInput` succeeds | zero and release the wheel on `WM_CLOSE`/session end, before the window dies |
| `SetHoldTimeoutMs(500)` | once, if the game writes force every frame | release force when updates stop (pause, menus, crash) |
| `PanicStop()` | your own quit path | everything off, autocentre restored last |
| `GetLastHResult()` | on a failed update, for the support bundle | the actual HRESULT, for `knowledge/DIRECTINPUT-FFB.md` |

Or use the managed binding: `Dbce.Wheel.Ffb.WheelFfbNative.Load(dir)` then
`Initialise(name, index)`, `SetForce(int)`, `ListFfbDevices()`, `ListAllDevices()`
(with capability labels), `OpenRead`/`Read` for shifters and pedals. It imports
the module name `WheelFfb`, so a mod using it ships the DLL as `WheelFfb.dll`.

## 4. Force shaping and the model (optional)

`ForceShaper` is the conditioning chain (deadzone, dual-rate EMA, soft
saturation, slew with event bypass, low-speed fade, ramp, Strength/invert,
peak limit). `ForceModel` with `ForceModelSettings.SimLite()` (lateral force
through a trail) or `Arcade()` (spring + damper + lateral load + events) turns
a `ForceInputs` struct into a normalised force; its `Validator.Report()`
belongs in your support bundle — it flags a dead steer input and a force that
fails to centre by speed band. `FfbDiagnostics` formats the peak report and
the 5 Hz trace.

## 5. Bumping

Fix FFB lifecycle or packet bugs **here**, cut a release, then in each
consumer: `Sync-Toolkit.ps1 -Version vX.Y.Z`, build, smoke-launch, commit the
new `lib/toolkit` with a changelog line. The C ABI only ever gains exports.

## Per-game notes

- **art-of-sim-rally**: done for 0.1.0 (telemetry + native). `WheelInput`
  and the Fy × trail model are the next pieces to swap for `Dbce.Wheel.Ffb`.
- **iracing-arcade-wheel**: its telemetry project is a byte-identical copy
  (namespace only) and its native DLL is the same file three fixes behind
  (single-instance crash fix, candidate fallback, capability labels, axis
  reading) — both slices are mechanical. Before trusting its slip × load FFB
  proxy at the base, log sign(force) vs sign(steer) by speed band
  (`knowledge/FFB-SIGNAL-DESIGN.md`).
- **OutRun2006Tweaks-FFB**: consume the C ABI from the proxy in place of
  `hooks_dinputffb.cpp`'s device code; use `Fm7Dash311`; fix the dead steer
  input first.
- **milestone-wheel-tools**: Forza encoder as a C header when one exists;
  the proxy scaffold moves here later.
- **cruisn-collection**: Python tools now; the SDL2 backend later.
