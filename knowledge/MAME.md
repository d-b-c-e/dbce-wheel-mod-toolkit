# MAME forks for arcade racers — input, FFB and the GPL boundary

Source: cruisn-collection + `mame-src` branch `poc/quadlog` (MAME 0.286 fork,
V-Unit and Zeus2 drivers), 2026-08-17 … 2026-09-03; `mame-racing`.

## Boundary

The emulator side is **GPL-2.0+ by derivation**. Anything that lives in the
driver (FFB engine, telemetry emitter, steering response) stays in the MAME
fork and is exported as a patch series. The launcher, tools and knowledge are
the project's own. A toolkit DLL can be *loaded* by the fork (MIT DLL, GPL
host is fine) but toolkit code must not be pasted into the driver.

## Force feedback from the driver, not from a plugin

"Don't use the FFB plugin when you work at driver level" (Endprodukt). The
driver has the game's actual wheel-motor byte; an external `dinput8.dll` hijack
(FFB Arcade Plugin) sees only memory it has to guess at, with a fixed effect
length that makes forces "come and go", per-game ini keys that silently no-op
strength, and an SDL GUID match that breaks across SDL builds (`DEVICE-QUIRKS.md`).
The engine that replaced it: SDL2 haptics loaded at runtime, one constant
effect on `SDL_HAPTIC_STEERING_AXIS`, level updated in place; three-pass device
selection (name or `vid:pid` → first wheel → any constant-force device, and
**off** if the named device cannot do haptics rather than falling through);
`SDL_JOYSTICK_RAWINPUT=0` hint to force DirectInput enumeration; conditioning
**upstream of output** — slew per game update, peak clamp, hold watchdog
(500 ms) because all four games write the byte every frame even when zero.
Measured laws per game in `FFB-SIGNAL-DESIGN.md`.

## Input: `winhybrid` is not `dinput`

- MAME's default Windows joystick provider is **`winhybrid`**, not the `dinput`
  module; patching only the latter left every wheel at 32 buttons. The fix set
  `DIJOYSTATE2` (128 buttons) as the primary format (mame-src `3cac3d67`).
- Buttons 33–48 map to `ADDSW1-16`; **49+ are unaddressable**
  (`OTHER_SWITCH`). `JOYCODE_x_BUTTON33+` does not parse and **invalidates the
  whole input sequence including `OR KEYCODE_1`** — a wheel button killed
  keyboard Start until the token was translated.
- glfw returns a **compacted** axis list; MAME/DirectInput name axes by fixed
  slot and skip absent ones. Dense devices never show the bug; a Fanatec CSW
  with separate pedals "binds fine and does nothing". Enumerate the slot layout
  from DirectInput (`dinput_axes.py`).
- Centre-resting pedals (MOZA on the base) read 50% pressed with a full-axis
  binding; emit half-axis tokens `_POS_ABSOLUTE` / `_NEG_ABSOLUTE` from the
  recorded press direction.
- Three device objects for one wheel (Gravel-style blank twins) → only an
  object that has produced a non-blank state may write.
- Steering feel: gain then clamp to full lock then signed power curve, patched
  into `ioport.cpp` for `IPT_PADDLE`. Rotation range is the wheel software's job.

## Headless testing

- `keybd_event` never reaches MAME's rawinput; drive inputs from a Lua plugin
  with `ioport_field:set_value()`.
- GDI capture of the GL overlay is black; snapshot inside the process.
- Running with `-nvram_directory fixtures/...` **writes** into the fixtures;
  seed a scratch copy, and beware `git add -A` sweeping the corruption in.
- Bit-exactness claims need four decimal places.

## Telemetry from an arcade board

- Displayed speed existed in no dumped memory (r < 0.55 for every decoding;
  attract mode is drone-driven, so shape-hunting it finds drone cars). OCR the
  HUD digits off the framebuffer: correct by construction.
- RPM from DSP RAM at a hunted address, confirmed by correlation with the
  on-screen tach (r = +0.91); TMS320C3x floats need their own decoder.
- Forza 324-byte encoder in the driver; neutral shows 1st; a launcher-side
  `ForzaKeeper` keeps the dash at zero between games.

## Launcher-side pieces that generalise

Press-to-bind wizard with **settle-then-arm** (an earlier version bound the
wheel's return spring to GAS), Raw Input >32-button listener, DirectInput axis
slot reader, stale-process guard, GitHub-Releases in-place updater, support
bundle, `ffb_trace_report` oscillation verdict. Most are on the toolkit's
migration list.
