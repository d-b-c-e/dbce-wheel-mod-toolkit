# Rewired (Unity input middleware) — what a mod can and cannot do

Rewired 1.1.x is common in Unity 2018–2020 racing titles (art of rally uses
1.1.55). Provenance: `[art-of-sim-rally, 2026-08-31 … 2026-09-03]` unless noted.

## How it presents devices

- Windows backend is **Raw Input** by default; saved binding keys are prefixed
  `WindowsRawInput…`. Raw Input parses HID reports itself.
- A device without a hardware definition is an **Unknown Controller**
  (`hardwareTypeGuid == Guid.Empty`): no racing-wheel template, no element
  names, no glyphs — but it *can* bind and drive, with two hidden penalties
  below. A MOZA R12 is one.
- Some devices it **cannot parse at all**: a Fanatec direct-drive base appears
  twice as `FANATEC Wheel`, 32 axes / 144 buttons (its ceiling), identical
  hardware ids, and never reports an element moving. Shifters and stalks that
  do not present as joysticks are **not listed at all** ("found 1 joysticks
  attached" while joy.cpl shows three).
- `ReInput.controllers.Joysticks`, `player.controllers.joystickCount`,
  `ContainsController`, `hardwareIdentifier`, `hardwareTypeGuid`, `axisCount`,
  `buttonCount` are the diagnostic surface; the support bundle prints them.

## The two hidden penalties for unrecognised wheels

1. **A 10% axis deadzone** applied by Rewired to unrecognised joysticks,
   separate from the game's own deadzone setting and shown nowhere. Zero it
   per axis on the joystick maps. (`ZeroAxisDeadzone`)
2. **Gamepad-style steering smoothing** if the game gates its direct-steering
   path on recognised-wheel status (art of rally's `CarController.SmoothSteer`
   fields `steerTime` etc.). Set the fields the recognised path sets.

## The game's binder usually only binds `Joysticks[0]`

art of rally's `ControlsRemapper.joystick` returns `player.controllers.Joysticks[0]`.
With a wheel plus a shifter, one is unbindable; with two same-named devices it
is not obvious which is index 0. Fix: a Harmony postfix on `get_joystick`
returning `GetLastActiveController(ControllerType.Joystick)` — "the device you
just moved is the device you are binding" — **latched**, because the remapper
reads the property repeatedly after the axis has recentred.

## Never switch the primary input source at runtime

`ReInput.configuration.windowsStandalonePrimaryInputSource` has a runtime
setter that calls `ResetAll()` — a full teardown and rebuild of controllers,
assignments and maps. Four attempts over two days, 2026-09-01/03:

- applied at mod load → keyboard dead, title screen impassable;
- applied 60 s after launch → keyboard survived (15/15 presses seen by Unity,
  Rewired's keyboard controller and the player's actions), DirectInput
  enumerated three devices where Raw Input saw one, reverse switch worked;
- applied from the panel, and again at the title screen pre-armed → **menus
  dead** while every probe said input was flowing (keyboard controller,
  player actions `UISubmit`/`UICancel`/`UIHorizontal`, keyboard maps enabled
  and identical, Rewired's UI input module alive with player 0);
- the game logged 48,216 "object created by a previous session … no longer
  valid" errors from game code caching Rewired objects (`ControllerButtonDisplay`,
  `Arcader`); refreshing all 84 references brought that to zero and the menus
  stayed dead. Cause not found.

Rule: **do not**. Devices Rewired cannot read are handled by reading the
device directly (`Dbce.Wheel.Input` direct-read) and feeding the car's input
method, which also avoids the full rebind the backend switch costs (joystick
maps are keyed by a hardware id that begins with the backend name).

## Useful facts

- `Player.GetAnyButtonDown()` covers all assigned controllers; menu code that
  caches `Player` objects breaks after any reset; `PadManager.GetPlayer()`-style
  fresh lookups survive.
- `UserDataStore_PlayerPrefs` reloads saved maps on initialise; art of rally
  stores them under `HKCU\Software\Funselektor Labs\art of rally`.
- Rewired's UI input module (`RewiredStandaloneInputModule`) re-resolves
  players on `InitializedEvent` and clears on `ShutDownEvent`.
- Rewired's own errors appear only in Unity's `Player.log`, without stack traces.
- Element glyphs: unrecognised wheels have no artwork; put the element name
  (e.g. `B12`) in the box instead of leaving it blank. (`GlyphTextFallback`)
- Do **not** route the wheel through xoutput/XInput to make Rewired recognise
  it: that hides the device from the DirectInput API force feedback needs.
