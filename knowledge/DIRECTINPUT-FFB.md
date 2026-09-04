# DirectInput force feedback — the playbook

Everything here was paid for once. Provenance in brackets: `[repo, date]`.

## Look the HRESULT up before theorising

`dinput.h` (Windows SDK, `um\dinput.h`) is the only authority. Two days were
lost to misreading one code twice. The ones that matter:

| HRESULT | Name | What it actually means | What to do |
|---|---|---|---|
| `0x80040205` | `DIERR_NOTEXCLUSIVEACQUIRED` | The device is acquired, but not exclusively. Happens when the game loses the foreground (alt-tab) and the device comes back non-exclusive. Force feedback needs exclusive. | `Unacquire()`, `Acquire()`, retry the update. Rate-limit (250 ms). One R12 session refused 4,401 updates before this was found. `[art-of-sim-rally, 2026-09-02]` |
| `0x80040206` | `DIERR_INCOMPLETEEFFECT` | Effect parameters incomplete. Was *guessed* for the above; it was not the cause. | Recreate the effect. |
| `0x80040208` | `DIERR_EFFECTPLAYING` | Also guessed wrongly for the above. | Stop, then set. |
| `0x80040154` | `REGDB_E_CLASSNOTREG` / `DIERR_DEVICENOTREG` from `CreateEffect` | The device has no actuator. A Fanatec base presents two `FANATEC Wheel` devices and only one does FFB. | Try the next FFB-capable candidate instead of giving up. `[art-of-sim-rally, 2026-09-03]` |
| `0x80070057` | `E_INVALIDARG` from `SetParameters` or `CreateEffect` | Parameter block disagrees with the effect — usually axis count. Fanatec single-axis bases refuse a 2-axis constant force. | Create with 2 axes, fall back to 1, and record the count; every later `SetParameters` must use it. `[art-of-sim-rally, 2026-08]` |
| `0x8007000E`/`E_HANDLE`, `DIERR_NOTDOWNLOADED` | Effect handle dead / not on the device | Driver dropped the effect (sleep, hot-plug, another app). | Release, recreate, **restart at zero and ramp in over ~250 ms**; back off 500 ms on failure. `[OutRun2006Tweaks-FFB, 2026-08]` |
| `DIERR_INPUTLOST`, `DIERR_NOTACQUIRED` from `GetDeviceState` | Lost the device | | `Acquire()` and retry — **only** from `GetDeviceState`, never from a failed `Poll()`. `[OutRun2006Tweaks-FFB commit ce6ea43]` |
| `0x800700AA` | `ERROR_BUSY` from `SetProperty(DIPROP_AUTOCENTER)` | Property written while acquired. | Autocentre may only be written on an **unacquired** device. Restore it last, after `Unacquire`. `[OutRun2006Tweaks-FFB commit 2d2a828]` |
| `S_FALSE` from `Acquire()` | Already acquired | Not an error; do not log it per frame. | |
| `DI_DOWNLOADSKIPPED` (3) | Effect not downloaded because not acquired exclusively | Diagnostic: non-exclusive handle. | `[art-of-sim-rally ffbprobe, 2026-09-02]` |

## Encoding a constant force so every wheel agrees

Send **a signed magnitude** and a direction vector whose **length equals
|magnitude|** (`{|x|, |y|}`), with `DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS |
DIEP_START`, every update. Three wheels read the same call three ways:

- **MOZA R5** honours both direction and magnitude sign — signing both
  double-negates and every force points the same way ("right works, left
  inverted, Invert does nothing"). `[art-of-sim-rally, user report 2026-09-02]`
- **MOZA R12** ignores direction and reads the magnitude sign.
- **Single-axis Fanatec** ignores direction.

A constant unit direction `{1,0}` with signed magnitude made an R12 start
rejecting updates ~45 s in (it was actually the focus-loss case above, but the
matched-length encoding is the one shape that never failed in the probe).
`SetParameters` must describe the same axis count the effect was created with.

Which sign centres is a **per-wheel fact**, not derivable: `+Fy` centres on a
MOZA R12 through this encoding `[art-of-sim-rally, 2026-09-02]`; a positive game
byte pushes right and a positive SDL steering-axis level turns the R12 left
`[cruisn-collection, 2026-08]`. Always ship an Invert toggle, and measure.

## Lifecycle rules

1. **One DirectInput instance per process.** Releasing a "temporary" instance
   after an enumeration while the device table stayed populated left the
   next `CreateDevice` on a null pointer — a hard crash whenever FFB had
   failed to initialise. `EnsureDirectInput()` at every entry point; release
   only when no device of any kind is open. `[art-of-sim-rally, 2026-09-03]`
2. **Exclusive for FFB, and the condition attached.** FFB needs
   `DISCL_EXCLUSIVE | DISCL_BACKGROUND`. If the same process also *polls* the
   device, give polling its own `DISCL_NONEXCLUSIVE` handle — sharing one
   handle meant poll-side `Acquire()` destroyed downloaded effects, felt as
   torque → 0 → full-step jerks `[OutRun2006Tweaks-FFB commit 007e4cd]`. If
   another library in the process already owns the device (SDL in
   cannonball-se), open **non-exclusive** or you fight it
   `[cannonball-se ffeedback.cpp]`. Reading the FFB wheel's axes through the
   exclusive handle we already hold is fine; never open a second instance of
   a device we hold — it was the last thing in the log before an R12 started
   refusing updates `[art-of-sim-rally commit 10477e3]`.
3. **Bind the cooperative level to this process's own window**, found by PID
   (largest visible unowned window), not to whatever `GetForegroundWindow()`
   returned at init — at init that can be a console or overlay.
   `[art-of-sim-rally, 2026-09-02]`
4. **Zero the wheel while the game window still exists.** By
   `DLL_PROCESS_DETACH` the OS has force-unacquired the exclusive device and
   nothing reaches the base — that is how "stuck force after Alt+F4" happens.
   Hook `WM_CLOSE` / `WM_QUERYENDSESSION` / `WM_DESTROY` on the main window
   and `ExitProcess` if you can; `WM_ACTIVATEAPP(FALSE)` → zero forces.
   Panic-stop order: level 0 → `Stop` → `SendForceFeedbackCommand(DISFFC_STOPALL)`
   → `SETACTUATORSOFF` → `RESET` → `Unacquire` → restore autocentre.
   `[OutRun2006Tweaks-FFB, 2026-08]`
5. **At `DLL_PROCESS_DETACH` the COM objects are already dead.** Releases
   fault; wrap each individually (one `__try` around several `Release()`
   calls silently skips everything after the first fault). Catch, null,
   leak, move on. `[OutRun2006Tweaks-FFB commit d8394fb]`
6. **Hold watchdog.** If the game stops writing the force (pause, menu,
   crash) release it after ~500 ms; a paused direct-drive base holding torque
   is a hazard. Measure that the game writes every frame first so the
   watchdog fires only when it should. `[cruisn-collection, 2026-08]`
7. **Never hard-kill a game with FFB live.** The constant-force effect is not
   released when the process dies; the base keeps the last torque and the
   next launch reads as "no FFB". Close via `WM_CLOSE`. Recovery: power-cycle
   or the MOZA Stream Deck "Stop FFB" action. `[milestone-wheel-tools, cruisn-collection, 2026-08]`
8. **An effect whose `Start()` fails never plays, and only the constant force
   notices.** `Start(1, 0)` can return `DIERR_NOTEXCLUSIVEACQUIRED` (0x80040205)
   when it races a focus change: the device is acquired but not exclusively.
   A constant force survives that, because every update carries `DIEP_START`
   and re-downloads the effect. A periodic effect does not — the preferred
   update strategy is `SetParameters` *without* `DIEP_START` — so it silently
   plays nothing for the rest of the session while its parameters look
   perfect. Retry `Start` through an unacquire/acquire, and until a slot has
   genuinely started, force `DIEP_START` on its updates regardless of the
   probed strategy. Do not let the strategy probe draw a conclusion from an
   effect that never started; it will read "not started" as "this driver
   dislikes DIEP_START". [dbce-wheel-mod-toolkit, 2026-09-04, found by a smoke
   test on its first run]

9. **In a game modded with a `dinput8.dll` proxy, resolve `DirectInput8Create`
   from `system32` by full path.** An import named `dinput8.dll` binds to the
   proxy sitting beside the game exe, so a force-feedback layer meant to run
   *alongside* the proxy ends up running *through* it, inheriting whatever that
   layer filters or re-types. `LoadLibraryW(L"<system32>\\dinput8.dll")` plus
   `GetProcAddress` gives a clean instance; the proxies themselves load the real
   DLL the same way. [OutRun2006Tweaks-FFB / dbce-wheel-mod-toolkit, 2026-09-04]

10. **Periodic effects update strategy is driver-dependent.** Prefer
   `SetParameters` *without* `DIEP_START` (no phase reset); fall back to with.
   Probe once, cache. `[OutRun2006Tweaks-FFB]`
9. **Do nothing heavy in `DllMain`.** `LoadLibrary`, threads, DirectInput
   creation under the loader lock deadlock; defer to the first exported call
   or first game tick. `[milestone-wheel-tools, OutRun2006Tweaks-FFB]`
10. **Log by default, opt out.** An absent log is indistinguishable from a
    broken one; the ffb.log is what every remote diagnosis ran on.
    `[art-of-sim-rally, iracing-arcade-wheel]`

## Device selection

- Enumerate `DI8DEVCLASS_GAMECTRL` with `DIEDFL_ATTACHEDONLY`; check
  `DIDEVCAPS.dwFlags & DIDC_FORCEFEEDBACK`; record axes/buttons — two devices
  with the same product name are told apart by capabilities.
- Prefer **VID:PID** (`guidProduct.Data1 == (PID << 16) | VID`; MOZA R12 =
  `0006346e`) as the key `[cannonball-se, milestone-wheel-tools]`; then an
  index; then a name substring.
- Open the preferred candidate; if `CreateEffect` fails, **try the others**
  before failing. `[art-of-sim-rally, 2026-09-03]`
- Rank FFB-capable first **and blocklist virtual pads** (vJoy, ViGEm, XOutput,
  vXbox, vGamepad, "Emulated"): a default vJoy reports 8 axes / 128 buttons
  like a MOZA R12, advertises FFB, and often enumerates first. This produced a
  long debugging session and a wrongly-filed upstream bug.
  `[OutRun2006Tweaks-FFB commit 02e3028]`
- Devices with ≥6 axes are typed `DI8DEVTYPE_1STPERSON`/SIXDOF, never
  `DRIVING`; games that filter by `dwDevType` will not offer a direct-drive
  base at all. See `DINPUT8-PROXY.md` for the re-type fix.
  `[milestone-wheel-tools, 2026-08-29]`

## Reading axes for input (direct-read designs)

- Request `DIPROP_RANGE` 0..65535 on every device before acquiring so
  calibration is uniform; some drivers refuse and keep native ranges — log it.
- Calibrate relative to observed **rest** and **far** values, extend far as
  the control is used; pedals that idle at the top of their range then work
  without a polarity setting. `[art-of-sim-rally WheelInput, 2026-09-03]`
- **Steering: +1 is right, and the X axis increases to the right on every
  DirectInput wheel** — so a steering assignment must take the increasing side
  as far, not the side the user happened to turn. Assigning with a left turn
  made left positive and the car steered inverted. Offer a Flip anyway.
  `[art-of-sim-rally, 2026-09-03]`
- `c_dfDIJoystick` is 32 buttons; `c_dfDIJoystick2` is 128. Shifters and
  button boxes above button 32 need the latter. `[cruisn-collection, milestone-wheel-tools]`
- Some devices need `Poll()` before `GetDeviceState`; `DI_NOEFFECT` from
  `Poll` is normal for the rest.

## The probe

`tools/ffbprobe` drives a base with scripted constant-force sequences through
every encoding and logs the accepted/refused result per call. Run it before
changing an encoding; it settled in ten minutes what a day of in-game testing
had not. `[art-of-sim-rally, 2026-09-02]`
