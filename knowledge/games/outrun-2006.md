# Case file: OutRun 2006: Coast 2 Coast

| | |
|---|---|
| Repos | `E:\Source\OutRun2006Tweaks-FFB` (fork of `emoose/OutRun2006Tweaks`; the injected DLL) + `E:\Source\outrun2006\outrun2006-redux` (config tool, SimHub plugin, installer, **the FFB design docs**) |
| Game | OutRun 2006 (2006, Sumo Digital), proprietary engine, 32-bit x86, D3D9; DRM-stripped replacement EXE from upstream |
| Approach | `DINPUT8.dll` wrapper proxy + safetyhook inline hooks on game functions (`GamePlCar_Ctrl` exe+0xA8330 etc.); C++20 |
| State | validated at a MOZA R12 2026-08-11 (422-line drive log); deployed binary post-dates that drive by 31 minutes — one lap closes it |

## What the mod does

- **FFB produced natively via DirectInput 8**: one `GUID_ConstantForce` plus two
  `GUID_Sine` periodics (road texture, tyre slip), on its **own exclusive
  handle** while the remap layer polls non-exclusively.
- **"Center-out" force model**: spring (steer × speed curve) + damper (steer
  rate) + lateral load × grip factor (lightens in a drift), × weight-transfer
  modulation, + crash impulse (direction from 8-frames-back lateral history)
  and symmetric gear-shift double pulse; dual-rate EMA, `tanh`, slew with event
  bypass, deadband, warm-up ramp. The game's own decompiled surface LUT drives
  texture. Mirrors a C# test bench (`FfbTestService.cs`) in the redux repo.
- Lifecycle: `PanicStop` on `WM_CLOSE`/`WM_QUERYENDSESSION`/`ExitProcess`,
  recreate-and-ramp on `E_HANDLE`/`NOTDOWNLOADED`, autocentre restored only
  when unacquired, never re-acquire on failed `Poll`, 250 ms watchdog through
  menu transitions.
- Input remap: DirectInput directly, three slots (wheel, shifter, aux), FFB-first
  scoring with a virtual-pad blocklist, rescaled steering deadzone, H-pattern →
  sequential synthesiser. INI-only (no bind UI in the fork).
- Telemetry: **Forza 311-byte FM7 Dash** to 127.0.0.1:8000 with synthesised
  RPM, plus a 76-byte shared-memory struct; missing pedals reported as −1.

## What was tried and abandoned

- **SDL3 haptics** (preview 3.1.8): `SDL_UpdateHapticEffect` silently ignored
  on MOZA/DD bases; destroy/recreate at 60 Hz vibrates. Ripped out (`eddaa28`).
- Steering angle for crash direction (corrupted at impact). Lateral slide as
  the whole force (inverted the grip relationship). Directional shift kick.
- `SetThreadAffinityMask` to core 0 on Windows 11 24H2 → white-screen hang
  with NVIDIA D3D9 threads; auto-disabled by OS build check.

## Known debt (recorded, not fixed)

- **Steer input `field_1D0` reads zero through corners** — the spring and damper
  terms produce almost nothing; the lateral term does the work the redesign
  meant to demote. Recorded as a telemetry bug; its FFB consequence unnoticed.
  Fix the offset before comparing feel.
- Speed over-reads ~2×; three dead INI settings; four INI sections read by code
  but absent from the shipped template; README describes the removed SDL3
  engine and a torque table that no longer exists; no CLAUDE.md; no tests.

## Findings that transferred

The DirectInput lifecycle playbook (`DIRECTINPUT-FFB.md`), the signal-processing
rules (`FFB-SIGNAL-DESIGN.md`), the vJoy misdetection and blocklist, the 15 Hz
cap on synthesised vibration, the 311-vs-324 layout split, port 8000
contention, −1 for missing data, loader-lock and 24H2 scheduler gotchas.

## Where to look

`src/hooks_dinputffb.cpp` (comments and commit bodies are the documentation),
`src/hooks_inputremap.cpp`, `docs/ffb-validation-2026-08-11.log`,
`outrun2006-redux/docs/FFB-RESEARCH.md` (939-line survey of 11 implementations),
`outrun2006-redux/docs/FFB-REDESIGN-FABLE.md` (the spec).
