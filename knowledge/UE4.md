# Unreal Engine 4 titles — reading game state without a game SDK

Source: milestone-wheel-tools (Gravel, UE 4.17; scanner covers 4.17–4.24),
2026-08-29/30. Native C++ inside a `dinput8.dll` proxy.

## Find GNames / GObjects by validating structure, not by code signature

Scan the `.data` section for a **structure that validates** — survives
rebuilds where byte signatures do not (~10 ms):

- **GNames**: a chunk table whose entry 0 is `"None"` with index 0 and entry 1
  is `"ByteProperty"` with index 2.
- **GObjects**: an `FUObjectItem` array whose first three items carry
  `InternalIndex` 0, 1, 2. Chunked GObjects for 4.20+.

## `UProperty::Offset_Internal` moves between versions

| UE version | `Offset_Internal` |
|---|---|
| 4.17 (Gravel) | **+0x50** — no published table lists this |
| 4.19+ (after `BlueprintReplicationCondition` was added) | +0x44 |
| older SDK dumps | +0x4C |

Probe it at runtime: pick a property whose offset you can validate, and
require a **sibling property to read a different in-range offset** — "a lone
plausible value passes by accident".

## Read the HUD widget, not the vehicle

Gravel's `IgnitionGaugeLogicWidget` holds `VehicleCurrentRPM` (+0x300),
`VehicleMaxRPM` (+0x2fc), `VehicleCurrentGear` (+0x30c) and **no speed**. The
HUD is fed the player's car by construction, which sidesteps "which of several
vehicles is the player's". Speed came from the audio engine instead
(`FMOD-AUDIO-AS-TELEMETRY.md`).

## Read safely

Every read goes through `ReadProcessMemory` on the own process (`safeRead`),
so a wrong guess is a failed validation, not an access violation on the game
thread.

## Configs the game owns

- `WheelConfig.ini` (Milestone): section per device keyed
  `[/Wheel.Config/<PID><VID>]`; a device not listed gets **no wheel option at
  all**. Two-layer binding: physical → logical `Wheel_*` slot (INI), logical →
  action (`settings.sav` GVAS `ignitioninput` block). Surprises: `GearUp`
  listens to `Wheel_RightShoulder`, nothing reads `Wheel_GearUp`; `Handbrake`
  is a button on `Wheel_RightTrigger`. Axis numbering `Axis1=X … Axis6=Rz,
  Axis7=Slider0, Axis8=Slider1`. Pedal polarity per brand — see
  `DEVICE-QUIRKS.md`.
- The game **rewrites the INI on exit** → edit only while closed. Steam's
  *Verify integrity* silently restores the stock file.
- **CRLF**: a `.*?$` regex eats the `\r` and converts edited lines to bare LF;
  match `[^\r\n]*` and count bare LFs after writing as a self-check.

## Generic UE4 pieces worth reusing

The GNames/GObjects scanner and offset probe (`src/ue4.cpp`, 385 lines), the
GVAS tagged-property walker underneath the save parser, and the
`[ue4] discover=1` mode that dumps every property of the RPM-owning class.
