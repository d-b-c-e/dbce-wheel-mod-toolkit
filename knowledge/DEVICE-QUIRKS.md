# Device quirks — what specific hardware does that the API does not tell you

Hardware facts, not game facts. Candidates for `wheel-db` as data. Provenance
in brackets.

## Fanatec direct-drive bases (CSL DD, DD Pro, ClubSport DD; PC and compatibility modes)

- Present as **two DirectInput devices with the same product name**
  `FANATEC Wheel`. Under DirectInput they differ: one is 8 axes / 108 buttons,
  the other 12 axes / 63 buttons. **Only one has the actuator**; `CreateEffect`
  on the other fails with `0x80040154`. Both modes behave the same.
  `[art-of-sim-rally, user support bundles 2026-09-02/03]`
- **Rewired's Raw Input backend cannot parse them**: both appear as 32 axes /
  144 buttons (its ceiling for an unparsed descriptor) with identical hardware
  ids, assigned to the player, and never report an element moving. The game's
  controls screen therefore never responds. No binding fix helps; read the
  device directly.
- Constant force needs a **single-axis** effect (2-axis `CreateEffect` fails
  `E_INVALIDARG`, retry with 1). Direction is ignored; magnitude sign rules.
- Pedals **idle high** in raw terms (see polarity below).
- CSL DD "FFB works once per session" was a zombie process still holding the
  device — check for stale game processes. `[cruisn-collection tester]`
- Fanatec CSW 2.5 with separate USB pedals: glfw reports a **compacted** axis
  list while DirectInput uses fixed slots and skips absent ones — pedals "bind
  fine, do nothing in game" until the layout is read from DirectInput.
  `[cruisn-collection]`

## MOZA bases (R5, R12) and accessories

- **R12**: ignores the constant-force direction vector, reads the magnitude
  sign. `+Fy` through the toolkit's encoding centres. Positive SDL2 steering-axis
  level turns it left. `[art-of-sim-rally, cruisn-collection]`
- **R5**: honours both direction and magnitude sign — sending the sign in both
  double-negates (one-sided inversion). `[art-of-sim-rally user report]`
- **Pedals idle low** (`&1.0&0.0` in Milestone's WheelConfig terms) where
  Logitech and Fanatec idle high (`&-1.0&1.0`). Copying a Logitech block onto
  a MOZA leaves the game convinced a pedal is permanently held — which also
  breaks press-to-bind listeners, since they latch onto the stuck axis.
  `[milestone-wheel-tools docs/wheelconfig.md]`
- **SR-P pedals on the base rest at centre (0.0)** in MAME/glfw terms, so a
  full-axis binding reads 50% pressed; bind half-axes with the recorded
  direction. `[cruisn-collection]`
- R12 base exposes 8 axes / 128 buttons (DirectInput) — the same shape as a
  default vJoy device, which is why virtual pads get mistaken for it.
- Multi-function stalk enumerates as a separate 32-button device with no axes;
  a DS-8X shifter as 11 buttons, no axes. Both invisible to Rewired's Raw Input
  and to Unity's default `supportedDevices` (Gamepad only).
  `[art-of-sim-rally, iracing-arcade-wheel]`
- Pit House and SimHub both want UDP `127.0.0.1:8000` for Forza telemetry; let
  SimHub take the feed and drive the MOZA dash via its plugin.
  `[OutRun2006Tweaks-FFB commit 7194ae8]`
- Recovery for a stranded constant force: power-cycle, or the MOZA Stream Deck
  "Stop FFB" action. `[moza-streamdeck-plugin]`

## Logitech (G29 and family)

- 4 axes → typed `DI8DEVTYPE_DRIVING` and offered by games that filter on
  device type, where an 8-axis direct-drive base is typed SIXDOF and hidden.
  `[milestone-wheel-tools docs/directinput-sixdof.md]`
- Pedals idle high.

## Virtual devices

- **vJoy** reports 8 axes / 128 buttons, advertises FFB, and often enumerates
  before the real wheel. A game or mod that takes "first FFB device" or ranks
  by axis count picks it — no axis input, forces silently absorbed. This
  produced a wrongly-filed upstream bug against a Sega Rally port. Blocklist
  vJoy, ViGEm, XOutput, vXbox, vGamepad, "Emulated" and rank FFB-capable real
  devices first. `[OutRun2006Tweaks-FFB commit 02e3028; abm-personal TODO]`
- **X-Arcade** exposes DirectInput interfaces even in XInput mode and corrupts
  DirectInput enumeration outright — power it off for wheel games.
  `[abm-personal TODO]`
- **DevReorder** and any other `dinput8.dll` proxy: only one can live in a
  game folder. `[milestone-wheel-tools, launchbox/devreorder-bridge]`

## Generic HID facts

- `guidProduct.Data1 == (PID << 16) | VID`. Games use it as a config key
  (Milestone's `WheelConfig.ini` section names). MOZA R12 = `0006346e`.
- ≥6 axes ⇒ DirectInput types the device `0x18` (SIXDOF/1STPERSON), never
  DRIVING. Cannot be fixed wheel-side; the base always exposes all axes.
- Unity's Input System builds HID layouts only for Generic Desktop page axes
  and the Button page; **Simulation Controls page** elements (steering 0xC8,
  accelerator 0xC4, brake 0xC5, clutch 0xC6) are dropped — the device appears,
  buttons work, pedals never register. Needs a custom layout JSON.
  `[iracing-arcade-wheel docs/CONTROLS.md]`
- SDL 2.26 changed the joystick GUID layout (bytes 2–3 became a CRC16 of the
  name), so a GUID harvested under one SDL build matches nothing under
  another. The FFB Arcade Plugin's `DeviceGUID=` match is a 16-byte memcmp.
  `[cruisn-collection RESULTS.md]`
- SDL3 preview (3.1.8) `SDL_UpdateHapticEffect` was silently ignored on
  MOZA/direct-drive bases; destroy/recreate at 60 Hz vibrates. Do not build
  real-time constant force on SDL3 haptics until re-verified.
  `[OutRun2006Tweaks-FFB commit eddaa28]`
