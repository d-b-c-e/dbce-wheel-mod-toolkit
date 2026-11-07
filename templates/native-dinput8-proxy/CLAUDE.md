# <GAME> — working notes

> TODO: replace every `<GAME>` and resolve every `TODO` before this is useful.
> Do not delete the non-negotiables; each was paid for once already.

## What this is

A `dinput8.dll` wrapper placed beside `<GAME>.exe`. Windows loads it instead of
the system one, it forwards every call to the real `system32\dinput8.dll`, and
in between it hooks the game to add force feedback control, telemetry and wheel
support that the game does not have.

It is **<32/64>-bit**, because the game is.

## The toolkit boundary

`src/vendor/` is an exact copy of one
[dbce-wheel-mod-toolkit](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit)
commit and one dbce-triple-screen-toolkit commit, named in `src/vendor/VERSION`:

| Path | What it is |
|---|---|
| `panel/` | the F6 panel and its D3D9/D3D11/D3D12 overlays |
| `dinput-proxy/` | the force layer (`dbce_ffb_layer.h`), fence, test switches |
| `controls/` | bindings, injection and shaping tables |
| `settings/` | the STD-044 ini writer (`saveKey`, `saveKeys`) |
| `support/`, `display/`, `forza/` | the issue report, monitor and racing-row rules, the Forza packet |
| `third_party/imgui/`, `triple-screen/` | Dear ImGui; triple-screen geometry, layout and compositor |

```powershell
pwsh <toolkit>\tools\Sync-Native.ps1 -GameRepo .            # re-vendor at toolkit master + triple-screen main
pwsh <toolkit>\tools\Sync-Native.ps1 -GameRepo . -Check     # run from build.ps1
```

**Never edit a file under `src/vendor`.** Fix it in the toolkit, sync again and
commit the new `src/vendor`; `-Check` fails the build on any edited copy.

What stays in this repo is **what the game is doing**: hook offsets, where the
car state lives, which field means what, camera and menu adapters. That part
really is per game.

## Player settings

Follow [UX-1](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/master/docs/CONSUMER-UX.md)
and [Simple/Advanced placement (UX-01-S)](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/master/docs/CONSUMER-SETTINGS-VIEWS.md).
F6 opens Wheel settings in Simple by default; remember an explicit view choice.
Keep complete ordinary binding/calibration, camera shortcut rebinding, main FFB
controls and recovery accessible in Simple. Advanced adds detailed tuning and
diagnostics over the same saved values; switching views never resets them or
changes runtime behavior. Record each option's placement and the guidance hash
with [acceptance evidence](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/master/docs/CONSUMER-UX-CHECKLIST.md).

## Non-negotiables

1. **Do nothing heavy in `DllMain`.** `LoadLibrary`, threads and DirectInput
   under the loader lock deadlock. Defer to the first game tick or the first
   `DirectInput8Create`.
2. **Load the real `dinput8.dll` from `system32` by full path**, never by bare
   name, or the proxy loads itself.
3. **Zero the wheel while the game window still exists.** Call
   `dbce::ffb::StopForExit()` from `WM_CLOSE` and `WM_DESTROY`. By
   `DLL_PROCESS_DETACH` nothing reaches the device, which is how a wheel is left
   holding torque after Alt+F4.
4. **Never hard-kill the game with a wheel attached**, for the same reason.
5. **No behaviour change to the game beyond what the mod is for.** Observe;
   do not alter physics or difficulty.

## Build, deploy, test

TODO: the build command. `build.ps1` runs `Sync-Native.ps1 -Check` first.

The output is one file, `dinput8.dll`, copied beside the game's exe. It writes
`dbce-<game>.ini` and its log there. Close the game before deploying; it locks
the DLL.

TODO: where the logs land.

## Wiring the force layer, in order

The game drives its own force feedback through DirectInput; the force layer sits
on those calls and gives the player FFB Off/On, Strength and F8 Stop FFB.

```cpp
#include "vendor/dinput-proxy/dbce_ffb_layer.h"

// once, before the game uses DirectInput (not from DllMain)
dbce::ffb::Options o;
o.iniPath = iniPath;            // [FFB] Enabled, Strength
o.log = dlog;
o.gainEmulation = false;        // true when the game scales force with DIEFFECT.dwGain
dbce::ffb::Init(o);

// in your DirectInput8Create export, after the real call succeeds
dbce::ffb::HookDirectInput(*out);

// F6 panel FFB page
dbce::ffb::DrawPanelPage(advanced);

// WM_CLOSE / WM_DESTROY
dbce::ffb::StopForExit();
```

## Measured facts — do not re-derive

TODO: add one line per thing you had to discover. Offsets, what a field really
means, which approach failed and why. This section is the reason the next
person is faster than you were.

## Open questions

TODO: what is unverified, and what would settle it.
