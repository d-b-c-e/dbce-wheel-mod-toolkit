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

## 3b. Native consumers (a dinput8 proxy, an emulator fork)

Vendor `include` and the build for your architecture, then include the header
and load the DLL at runtime — never import it, or a missing file stops the game
from starting instead of merely costing force feedback:

```powershell
.\tools\Sync-Toolkit.ps1 -LocalRepo E:\Source\dbce-wheel-mod-toolkit -Parts include,native-x86
```

```c
#include "wheelffb.h"                 // lib/toolkit/include
static WheelFfbApi ffb;
if (WheelFfb_LoadBeside(&ffb, myModule, L"WheelFfb.dll")) {
    ffb.SetLogPath(logPathBesideTheGame);          // optional
    ffb.SetPreferredDeviceGuid(&guidFromMyInputLayer);   // or ...Index / ...Device
    if (ffb.InitDirectInput((int)(INT_PTR)hwnd)) {
        int road = ffb.CreatePeriodicEffect(25);   // -1: no periodics, synthesise <=15 Hz
        ffb.SetDeviceForcesXY(magnitude, 0);       // -10000..10000
        ffb.UpdatePeriodicEffect(road, mag10000, freqMilliHz);
    }
} else log("WheelFfb.dll: %s missing", WheelFfb_MissingExport(&ffb));
```

Copy the DLL next to your own binary as a post-build step; a missing
`WheelFfb.dll` is a silent loss of all force feedback. Do **not** call
`WheelFfb_Unload` from `DllMain` — `FreeLibrary` under the loader lock is
forbidden. At exit, `PanicStop()` is the call that matters.

`native/forza/forza_packet.h` is the Forza encoder for the same audience:
header-only, `dbce::forza::build(layout, sled, dash, out, size, sentinel)`.

Installers and setup wizards take `-Parts powershell` and import
`DbceWheel.psm1` for Steam discovery, DirectInput enumeration and CRLF-safe
INI edits.

## 4. Force shaping and the model (optional)

`ForceShaper` is the conditioning chain (deadzone, dual-rate EMA, soft
saturation, slew with event bypass, low-speed fade, ramp, Strength/invert,
peak limit).

Reach for a **whole tune**, not half of one. `ForceProfile.SimLite()` (lateral
force through a trail, art of rally 0.2.2) and `ForceProfile.Arcade()` (spring
+ damper + lateral load + events) carry the model and the conditioning that
belong together. `ForceModelSettings.SimLite()`/`Arcade()` return the model
half only, and pairing either with a default `ForceShaper` gives you
conditioning no game asked for; that mistake shipped as `simlite@1`. Better
still, load a named version from the file beside the game with
`ForceProfile.LoadFrom`, so a tune can be swapped without a rebuild.

`ForceModel` turns a `ForceInputs` struct into a normalised force; its
`Validator.Report()`
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
- **OutRun2006Tweaks-FFB**: done for 0.2.0 — the C ABI replaced its device
  code, and `forza_packet.h` its 311-byte encoder. Outstanding: the steering
  field. Drive one lap with `FFBDiagnosticLog` and read `FFB STEERSCAN`.
- **milestone-wheel-tools**: done for 0.2.0 — `forza_packet.h` and
  `DbceWheel.psm1`. The proxy scaffold still has not moved here.
- **cruisn-collection**: Python tools now; the SDL2 backend later.
