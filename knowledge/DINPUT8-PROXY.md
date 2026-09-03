# dinput8.dll proxies — the scaffold and what to do inside it

Three exist in the portfolio (milestone-wheel-tools, launchbox/devreorder-bridge,
Launchbox-Racing `DInputDrivingType`), plus OutRun2006Tweaks-FFB's `Proxy.cpp`.
Source of most rules: milestone-wheel-tools, 2026-08-29/30.

## Scaffold

- Ship as `dinput8.dll` beside the game exe. Forward the six exports
  (`DirectInput8Create`, `DllCanUnloadNow`, `DllGetClassObject`,
  `DllRegisterServer`, `DllUnregisterServer`, `GetdfDIJoystick`) to the real
  `System32\dinput8.dll` via a `.def` and a `FWD` macro.
- Patch vtables after the real object is created: `IDirectInput8` 3 =
  `CreateDevice`, 4 = `EnumDevices`; `IDirectInputDevice8` 3 = `GetCapabilities`,
  5 = `GetProperty`, 6 = `SetProperty`, 9 = `GetDeviceState`, 11 =
  `SetDataFormat`, 15 = `GetDeviceInfo`, 18 = `CreateEffect`;
  `IDirectInputEffect` 6 = `SetParameters`. **Vtables are shared per class**, so
  every hook must re-check `self`.
- Do everything in the **first exported call**, never in `DllMain`
  (`LoadLibrary` and `CreateThread` under the loader lock).
- **Only one `dinput8.dll` per folder.** DevReorder, the FFB Arcade Plugin and
  any toolkit proxy collide; refuse to overwrite a foreign one (hash it, probe
  for your own marker string) and say so.

## Re-typing a direct-drive base as a wheel

DirectInput types any joystick with ≥6 axes as `DI8DEVTYPE_1STPERSON`/SIXDOF
(`0x18`), never `DI8DEVTYPE_DRIVING`. Games that filter by `dwDevType` (Milestone's
UE4 titles, Crazy Taxi 3, others from 2016–2018) never offer the base. Cannot be
fixed wheel-side. Rewrite `dwDevType` to
`DI8DEVTYPE_DRIVING | (DI8DEVTYPEDRIVING_THREEPEDALS << 8)` (preserving the high
16 bits) in **both** places a game can read it: the `EnumDevices` callback (shim
the callback) and `GetCapabilities` / `GetDeviceInfo`. Prior art:
`Tathanito/vjoyct3fix`. A Logitech G29 has 4 axes and types `0x14`, which is
why it "just works".

## Tapping input

Hook `GetDeviceState` on the game's own device: learn the axis ranges from
`DIPROP_RANGE` (and adopt ranges the game narrows later via `SetProperty`,
`DIPH_DEVICE` and `DIPH_BYOFFSET`), log whether the game asked for
`DIJOYSTATE` (80 B, 32 buttons) or `DIJOYSTATE2` (272 B, 128) — that decides
whether buttons ≥33 can ever bind — and filter **blank device twins** (the game
creates several objects for one wheel; only one is acquired; the others return
all-zero state at poll rate and overwrite good input: once any object has
produced a non-blank state, only such objects may write).

## Tapping force feedback

Hook `CreateEffect` and `SetParameters`, classify by GUID (constant / periodic
/ spring / damper / ramp), normalise `lMagnitude / 10000`, keep **per-effect
magnitudes and take the strongest** (Gravel runs two constant-force effects at
once; a single shared value latched whichever updated last, usually the
smaller), apply per-effect `dwGain` × user gain, flip sign for
`rglDirection[0] < 0` on single-axis effects.

## Producing force feedback from a proxy

OutRun2006Tweaks-FFB does it: open the FFB device on **its own exclusive
handle** while the remap layer polls non-exclusively; never re-acquire on
`Poll()` failure; zero the wheel on `WM_CLOSE`/`ExitProcess`, not at
`DLL_PROCESS_DETACH`. Full playbook in `DIRECTINPUT-FFB.md`. This is what the
toolkit's `native/wheelffb` C ABI replaces inside a proxy.

## PowerShell around a proxy

- PS7/.NET Core cannot marshal a delegate through a COM interface: pass the
  enum callback as a raw pointer (`Marshal.GetFunctionPointerForDelegate`), and
  give `DirectInput8Create` a real `GetModuleHandleW(IntPtr.Zero)` — a null
  string marshals to `E_INVALIDARG`.
- Two ctypes traps in Python: `GetModuleHandleW`'s HMODULE truncates without a
  `restype`; `IDirectInput8::EnumDevices` takes `dwDevType` **first**, then the
  callback.
- Steam is frequently not under Program Files: registry (`HKCU\Software\Valve\Steam`,
  `HKLM` + WOW6432Node) → `libraryfolders.vdf` → every library.
- `dinput8.dll` only exports `GetdfDIJoystick` (32 buttons); reaching buttons
  33–128 needs `c_dfDIJoystick2`, which is why a tiny native probe
  (`wheelprobe`) beats hand-building a 164-entry `DIDATAFORMAT` in a script.
