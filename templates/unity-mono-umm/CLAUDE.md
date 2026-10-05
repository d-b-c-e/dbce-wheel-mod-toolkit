# <GAME> — working notes

> TODO: replace every `<GAME>` and resolve every `TODO` before this is useful.
> Do not delete the non-negotiables; each was paid for once already.

## What this is

A Unity Mod Manager mod for `<GAME>` (Unity `<version>`, **Mono**), using
Harmony to patch the game and the dbce wheel toolkit to add force feedback,
telemetry and wheel support.

Mono is what makes this shape easy: real reflection, ordinary assemblies, and
Harmony patches that behave. If the game is IL2CPP instead, use the
`unity-il2cpp-bepinex` template — nothing here transfers cleanly.

## The toolkit boundary

`lib/toolkit/` is vendored from
[dbce-wheel-mod-toolkit](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit),
pinned in `lib/toolkit/VERSION`:

| Path | What it is |
|---|---|
| `native/WheelFfb.dll` | the DirectInput device and effect layer |
| `dotnet/Dbce.Wheel.Ffb.dll` | the binding, the force model, the shaping chain, device selection |
| `dotnet/Dbce.Wheel.Telemetry.dll` | the Forza encoder, idle keeper, RPM synthesis |

```powershell
.\tools\Sync-Toolkit.ps1 -Version vX.Y.Z -Parts native,dotnet
```

**Fix force, device or packet bugs in the toolkit, not here**, then re-sync and
commit the bumped `lib/toolkit`. Shipping DLLs means the copy *is* the update;
no rebuild is needed for a toolkit bump alone.

What stays here is **what the car is doing**: which component holds the car
state, which field is slip angle, where the camera lives, what the UI shows.

## Player settings

Follow [UX-1](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/master/docs/CONSUMER-UX.md)
and [Simple/Advanced placement (UX-01-S)](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/master/docs/CONSUMER-SETTINGS-VIEWS.md).
F6 opens Wheel settings in Simple by default; remember an explicit view choice.
Keep complete ordinary binding/calibration, camera shortcut rebinding, main FFB
controls and recovery accessible in Simple. Advanced adds detailed tuning and
diagnostics over the same saved values; switching views never resets them or
changes runtime behavior. Record each option's placement and the guidance hash
with [acceptance evidence](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/master/docs/CONSUMER-UX-CHECKLIST.md).
The toolkit does not supply a shared UI renderer or calibration workflow yet.

## Non-negotiables

1. **Target `netstandard2.0` with no Unity references in shared code.** It is
   what lets one assembly load under this runtime and be unit-tested on modern
   .NET. Unity-specific code goes in a clearly named file, not in the model.
2. **Load the native DLL by absolute path before any `DllImport`.**
   `WheelFfbNative.Load(dir)` does the `LoadLibraryW` preload; a mod folder is
   not on the loader's search path, and the failure looks like a missing
   method deep inside the game.
3. **Never switch the game's input library backend at runtime.** Rewired's
   `ResetAll()` killed the menus four separate times and the cause was never
   found. Read the device directly instead.
4. **Zero the wheel before the game window goes away.** Call
   `InstallExitGuards()` after init, and `PanicStop()` on your own quit path.
5. **Never hard-kill the game with a wheel attached.** A live constant-force
   effect is not released when a process dies, so the base keeps the last
   torque and the next launch reads as "no force feedback".
6. **Measure the sign on real hardware** before tuning. A centring force pushes
   *against* the steering, and getting it backwards feels like a broken wheel
   rather than a wrong number.
7. **One everyday Strength control in Simple.** Start from a tested game tune;
   detailed profiles and component adjustments belong in Advanced under UX-01-S.
   Preserve existing tunes in both views.
8. **No physics or difficulty changes**, and nothing that touches an online
   mode.

## Build, deploy, test

TODO: the build command, and where the mod folder goes.

Close the game before deploying; it locks the DLLs.

TODO: where the logs land. Keep the FFB log path explicit with
`WheelFfbNative.LogTo(...)` so support requests have one place to look.

## Wiring, in order

```csharp
// once, at load
WheelFfbNative.Load(modDirectory);
WheelFfbNative.LogTo(Path.Combine(logDirectory, "ffb.log"));
var device = WheelFfbNative.ListFfbDevices();            // show these to the user
WheelFfbNative.PreferGuid(settings.DeviceGuid);          // pin by identity, not name
WheelFfbNative.Initialise();
WheelFfbNative.ExitGuards();

var profile = ForceProfile.LoadFrom(modDirectory, settings.Profile, out var why);
var model   = new ForceModel(profile.Model);
var shaper  = profile.Shaper;

// every frame
var i = new ForceInputs {
    Steer = /* TODO */, HasSteer = true,
    SpeedMps = /* TODO */,
    FrontLateralForce = /* TODO */, HasFrontLateralForce = true,
};
float f = shaper.Shape(model.Compute(i, dt), i.SpeedMps * 3.6f, dt, model.LastWasEvent);
WheelFfbNative.SetForce((int)(f * 10000));

// on quit
WheelFfbNative.Panic();
```

Fill only what the game exposes; leave the rest at default with its `Has*`
flag false and the model uses what it has. `model.Validator` will tell you when
an input is dead or the sign is wrong — put its report in the support bundle.

## Measured facts — do not re-derive

TODO: one line per thing you had to discover. Which component holds the car
state, what a field's units really are, which approach failed and why.

## Open questions

TODO: what is unverified, and what would settle it.
