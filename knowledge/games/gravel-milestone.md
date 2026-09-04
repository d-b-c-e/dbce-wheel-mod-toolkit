# Case file: Gravel (and other Milestone UE4 racers)

| | |
|---|---|
| Repo | `E:\Source\milestone-wheel-tools` |
| Game | Gravel (Steam 558260, delisted — offsets are permanent), UE 4.17. MXGP3, MXGP PRO, MotoGP 18/19, Monster Energy Supercross listed as untested targets |
| Approach | `dinput8.dll` proxy (C++17, MinGW), observe-only except one field; PowerShell installer and bind wizard; Python tools; INI config read once at start. Forza encoder and installer helpers are the toolkit's since 2026-09-04 |
| State | Gravel verified on a MOZA R12: wheel fix, inputs, FFB tap, telemetry, physics-from-audio, impacts |

## What was wrong

Direct-drive bases (≥6 axes) are typed SIXDOF by DirectInput and the game
offers **no wheel option at all** for a device not in its `WheelConfig.ini`; the
rebind screen cannot be used with one (a pedal with the wrong polarity block
reads permanently held and the press-to-bind listener latches onto it); the
game ships zero telemetry.

## What the mod does

- **Re-types** `dwDevType` to `DRIVING | THREEPEDALS` in both read sites
  (`EnumDevices` callback and `GetCapabilities`/`GetDeviceInfo`).
- **Taps** the game's own input (`GetDeviceState`, range learning, blank-twin
  filter) and FFB (`CreateEffect`/`SetParameters`, per-effect strongest-wins,
  sign from direction) — it consumes FFB, it does not produce it.
- **Telemetry from FMOD parameters** (speed, slip, suspension, torque,
  collisions) plus RPM/gear from a UE4 HUD widget found by reflection
  (`Offset_Internal +0x50` in 4.17); Forza 232/311/324 selectable; derived
  channels (accX from steering torque, impacts as decaying spikes).
- `WheelSetup.ps1` press-to-bind wizard driven by a tiny native `wheelprobe`
  (128 buttons); writes the two-layer binding (`WheelConfig.ini` slot map and
  `settings.sav` GVAS actions) with CRLF preserved.
- Installer: registry Steam discovery, DirectInput enumeration from PowerShell
  with the two COM marshalling workarounds, refuses to overwrite a foreign
  `dinput8.dll`.

## Findings that transferred

SIXDOF re-type; vtable index table; `guidProduct.Data1 = (PID<<16)|VID`;
pedal polarity by brand (MOZA low, Logitech/Fanatec high); the 323-vs-324
Forza length finding; delay-load thunk self-unhooking; UE4 `Offset_Internal`
per version and the sibling-property probe; audio parameters as telemetry and
call-count classification; never hard-kill with FFB live; CRLF-safe INI edits;
Steam verify restores stock files; "every wrong turn came from inferring a
value that could have been read from a log".

## Debt recorded

`impact_gain` default mismatch between header and loader; a dead line in the
listener; `Install.ps1` shells out to Python for a job `WheelSetup.ps1` does in
PowerShell; `Measure-WheelAxes.ps1` (winmm, cannot see Rx/Ry) superseded by
`wheelprobe`; committed `dist/` binaries kept in step by convention; no tests.

## Where to look

`CLAUDE.md` (numbered traps), `docs/directinput-sixdof.md`, `docs/wheelconfig.md`,
`docs/forza-format.md`, `docs/troubleshooting.md`, `src/proxy.cpp`, `src/ue4.cpp`,
`src/fmod_tap.cpp`, `games/gravel/README.md`.
