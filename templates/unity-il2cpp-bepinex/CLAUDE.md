# <GAME> — working notes

> TODO: replace every `<GAME>` and resolve every `TODO` before this is useful.
> Do not delete the non-negotiables; each was paid for once already.

## What this is

A BepInEx 6 plugin for `<GAME>` (Unity `<version>`, **IL2CPP**), using Harmony
through Il2CppInterop and the dbce wheel toolkit to add force feedback,
telemetry and wheel support.

IL2CPP is the hard shape. The game's C# was compiled to native code and the
managed types you patch are generated interop wrappers, not the originals. Most
Mono-era advice is wrong here, which is why this is a separate template.

## The toolkit boundary

`lib/toolkit/` is vendored from
[dbce-wheel-mod-toolkit](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit),
pinned in `lib/toolkit/VERSION`:

| Path | What it is |
|---|---|
| `native/WheelFfb.dll` | the DirectInput device and effect layer |
| `dotnet/Dbce.Wheel.Ffb.dll` | the binding, the force model, the shaping chain |
| `dotnet/Dbce.Wheel.Telemetry.dll` | the Forza encoder, idle keeper, RPM synthesis |

```powershell
.\tools\Sync-Toolkit.ps1 -Version vX.Y.Z -Parts native,dotnet
```

The binding installs a `NativeLibrary.SetDllImportResolver` through reflection
on this runtime, so the same assembly works here and under Mono.

TODO: if this repo gitignores `lib/`, say so here and record the pinned version
in `CHANGELOG.md` instead, or the version becomes unknowable from the repo.

## Non-negotiables

1. **An injected MonoBehaviour only receives Unity messages.** `Awake`,
   `Update` and friends work; your own methods are never called by the engine.
   Register the type before you add it.
2. **Count your prefix calls.** IL2CPP inlines aggressively, so a Harmony patch
   can attach to a method the game never actually calls at runtime. A hook that
   silently never fires looks exactly like a hook that fires and does nothing.
3. **Constants are not emitted.** A `const` you can see in a decompile may have
   no field to read at runtime; read the value from an instance instead.
4. **IMGUI is stripped.** `OnGUI` is not available, so any panel needs the
   toolkit's `Widgets` approach rather than `GUILayout`.
5. **The Unity Input System drops anything not in `supportedDevices`.** A wheel
   can be plugged in, working elsewhere, and invisible to the game until that
   list is extended.
6. **Load the native DLL by absolute path before any `DllImport`.**
   `WheelFfbNative.Load(dir)` handles both runtimes.
7. **Zero the wheel before the window goes away**, and never hard-kill the game
   with a wheel attached; the base keeps the last torque.
8. **Measure the sign on real hardware** before tuning anything.
9. **No physics or difficulty changes, and nothing that touches an online
   mode.** If the game has one, gate every behaviour change on being offline
   and fail closed when you cannot tell.

## Build, deploy, test

TODO: the build command and the plugin folder.

TODO: name the interop assemblies this plugin references and where they are
generated, because a fresh checkout cannot build without them.

Close the game before deploying; it locks the DLLs.

## Wiring, in order

Same as the Mono shape — the toolkit API is identical:

```csharp
WheelFfbNative.Load(pluginDirectory);
WheelFfbNative.LogTo(Path.Combine(logDirectory, "ffb.log"));
WheelFfbNative.PreferGuid(settings.DeviceGuid);
WheelFfbNative.Initialise();
WheelFfbNative.ExitGuards();

var profile = ForceProfile.LoadFrom(pluginDirectory, settings.Profile, out var why);
var model   = new ForceModel(profile.Model);
var shaper  = profile.Shaper;

// every frame, from an injected MonoBehaviour's Update
var i = new ForceInputs { /* TODO: fill from the car state */ };
float f = shaper.Shape(model.Compute(i, dt), i.SpeedMps * 3.6f, dt, model.LastWasEvent);
WheelFfbNative.SetForce((int)(f * 10000));
```

Getting the car state is the whole job here. Expect to spend far longer finding
it than wiring it.

## Measured facts — do not re-derive

TODO: one line per thing you had to discover. Which type holds the car state,
which hooks actually fire, what the interop wrapper does to a field's type.

## Open questions

TODO: what is unverified, and what would settle it.
