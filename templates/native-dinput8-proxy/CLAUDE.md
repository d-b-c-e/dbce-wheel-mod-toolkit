# <GAME> — working notes

> TODO: replace every `<GAME>` and resolve every `TODO` before this is useful.
> Do not delete the non-negotiables; each was paid for once already.

## What this is

A `dinput8.dll` wrapper placed beside `<GAME>.exe`. Windows loads it instead of
the system one, it forwards every call to the real `system32\dinput8.dll`, and
in between it hooks the game to add force feedback, telemetry and wheel support
that the game does not have.

It is **<32/64>-bit**, because the game is. That decides which `WheelFfb.dll`
you vendor and nothing else.

## The toolkit boundary

`lib/toolkit/` is vendored from
[dbce-wheel-mod-toolkit](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit),
pinned in `lib/toolkit/VERSION`:

| Path | What it is |
|---|---|
| `include/wheelffb.h` | the C ABI of `WheelFfb.dll` and its runtime loader |
| `include/force_model.h`, `force_profile.h` | the shared force model and the versioned tunes |
| `include/forza_packet.h` | the Forza telemetry encoder |
| `native/x86\|x64/WheelFfb.dll` | the DirectInput device and effect layer |
| `profiles/force-profiles.ini` | the tunes, deployed beside the game and read at runtime |

```powershell
.\tools\Sync-Toolkit.ps1 -Version vX.Y.Z -Parts include,native-<arch>,profiles
```

**Fix device, effect, packet or force-model bugs in the toolkit, not here.**
Then re-sync, rebuild — the headers are compiled in — and commit the bumped
`lib/toolkit`. A second copy of that code is how two projects here came to fix
the same bug on different days.

What stays in this repo is **what the car is doing**: where the car state
lives, which field means what, the game's own surface or collision data, and
how a crash is detected. That part really is per game.

## Non-negotiables

1. **Do nothing heavy in `DllMain`.** `LoadLibrary`, threads and DirectInput
   under the loader lock deadlock. Defer to the first game tick; that is also
   when a valid window handle exists, which DirectInput needs.
2. **Load `WheelFfb.dll` at runtime, never import it.** A missing file must
   cost force feedback, not stop the game from starting. `WheelFfb_LoadBeside`
   does this; copy the DLL next to your own as a post-build step, because a
   deploy that forgets it silently loses all force feedback.
3. **Force feedback gets its own `EXCLUSIVE | BACKGROUND` device handle**,
   separate from whatever polls input. A poll-side `Acquire()` on a shared
   handle destroys downloaded effects mid-corner.
4. **Zero the wheel while the game window still exists.** Hook `WM_CLOSE` and
   `ExitProcess`. By `DLL_PROCESS_DETACH` nothing reaches the device, which is
   how a wheel is left holding torque after Alt+F4.
5. **Never hard-kill the game with a wheel attached**, for the same reason.
6. **Measure the sign on real hardware** before tuning. A centring force pushes
   *against* the steering; getting it backwards feels like a broken wheel, and
   every project here has done it once.
7. **No behaviour change to the game beyond what the mod is for.** Observe;
   do not alter physics or difficulty.

## Build, deploy, test

TODO: the build command.

Output must contain **three** files, and a deploy that misses one is a silent
loss:

```
dinput8.dll          this mod
WheelFfb.dll         the force-feedback layer
force-profiles.ini   the tunes, read at runtime
```

Close the game before deploying; it locks the DLL.

TODO: where the logs land.

## Wiring, in order

```c
#include "wheelffb.h"        // lib/toolkit/include
#include "force_profile.h"

static WheelFfbApi   ffb;
static dbce::force::Profile profile;
static dbce::force::Model*  model;
static dbce::force::Shaper* shaper;

// once, on the first game tick
WheelFfb_LoadBeside(&ffb, myModule, L"WheelFfb.dll");
dbce::force::load_profile_dir(gameFolder, iniProfileName, profile, &why);
model  = new dbce::force::Model(profile.model);
shaper = new dbce::force::Shaper(profile.shaper);
ffb.SetPreferredDeviceGuid(&guidYourInputLayerChose);   // or ...Index
ffb.InitDirectInput((int)(INT_PTR)hwnd);

// every tick
dbce::force::Inputs in;
in.steer = /* TODO */;            in.has_steer = true;
in.speed_mps = /* TODO */;
in.lateral_g = /* TODO */;        in.has_lateral_g = true;
float f = shaper->shape(model->compute(in, dt), in.speed_mps * 3.6f, dt, model->last_was_event);
ffb.SetDeviceForcesXY((int)(f * 10000.f), 0);

// at exit, from a real window message - not DllMain
ffb.PanicStop();
```

Fill only the inputs the game actually exposes and leave the rest false; the
model uses what it has.

## Measured facts — do not re-derive

TODO: add one line per thing you had to discover. Offsets, what a field really
means, which approach failed and why. This section is the reason the next
person is faster than you were.

## Open questions

TODO: what is unverified, and what would settle it.
