# Controls contract (STD-033), schema 1

How a rig profile's control bindings reach a game, and how a test proves the game uses them. Two layers, kept apart:

1. **The logical binding:** one physical control per action, with what is needed to read it. It is the same for every
   game, in Wheelkit (C#) and in every mod (C++, C#, JS). It is defined here, with its exact text form and normalization.
2. **The storage adapter:** where one game keeps its bindings and how the logical binding is translated into that store,
   e.g. art of rally's `Settings.xml`, V-Rally 4's `InputDeviceConfig.lua` and `InputBindings.lua`, DiRT Rally 2.0's
   action maps, or the `[Controls]` INI section below for native mods that have no store of their own. A game has
   **one** store, which its F6 panel and Wheelkit both edit. A storage adapter never invents a second one.

Owner requirement, 2026-10-09: apply the rig profile's controls in every game, and prove in automated tests that the game
responds to them. Writing a file is not proof; see "Evidence" below.

## 1. Actions

The ids are Wheelkit's (`MappingProfile.Actions`), case-sensitive, and the same in every serialization:

| Group | Ids | Shape |
|---|---|---|
| Driving | `steer` | centred: an axis only |
| | `throttle`, `brake`, `clutch`, `handbrake` | pedal: an axis, or a button (on/off) |
| Gears | `shiftUp`, `shiftDown`, `neutral`, `reverse`, `gear1` .. `gear8` | digital: a button or a hat direction |
| Menus | `confirm`, `back`, `start`, `select`, `navUp`, `navDown`, `navLeft`, `navRight` | digital |
| In car | `camera`, `lookBack`, `reset`, `horn` | digital |
| Mod | `modMenu`, `muteFfb` | digital |

A binding whose kind does not fit its action's shape (steering on a button, a menu action on an axis) is invalid.
Unknown action ids are reported and skipped, never guessed.

Profile-level values: `Transmission` = `Automatic` | `Sequential` | `H-pattern` (Wheelkit's words).

## 2. The binding

A binding names one DirectInput object on one device instance:

| Field | Meaning |
|---|---|
| kind | `axis`, `button` or `hat` |
| index | axis 0-7 in DIJOYSTATE2 order (X Y Z Rx Ry Rz Slider0 Slider1); button 0-127; hat 0-3 |
| angle | hat only: the direction, 0-35999 in hundredths of a degree (DirectInput POV units; 0 up, 9000 right) |
| dev | the device **instance** GUID. Required. Runtime reads and injection match on it alone; there is no fallback to a different instance. |
| prod | the product GUID (`Data1` = PID << 16 \| VID). Required. It is identity evidence for adapters that write product-keyed native config (V-Rally's GUID lists). It never silently stands in for a missing instance at runtime. |
| range | axis only: the raw minimum and maximum the binding was captured in (Wheelkit captures 0..65535) |
| rest | axis only: the raw value at rest when captured |
| travel | axis only: `+1` or `-1`, the sign of (value - rest) when the control was moved during capture |
| inverted | axis only, optional: the player's explicit inversion (Wheelkit `Inverted`) |
| calibrated | optional: Wheelkit's `Calibrated` flag, carried through |
| name | optional: the device's display name, for logs and panels only; never used to match |

## 3. Normalization (identical in every implementation)

Inputs are a raw sample in the binding's own `range`. A game or proxy that reads the device in a different range
rescales linearly first: `v = min + (sample - sampleMin) * (max - min) / (sampleMax - sampleMin)`.

- **Centred axis (`steer`):** `centre = (min + max) / 2`, `half = (max - min) / 2`;
  `n = clamp((v - centre) / half, -1, 1)`, right positive (DirectInput's convention: X grows to the right);
  `n = -n` when `inverted`. `travel` and `rest` are ignored for steering, because Wheelkit's capture asks only to
  "move one control": the recorded travel is whichever way the player happened to turn, never an inversion.
- **Pedal axis (`throttle`, `brake`, `clutch`, `handbrake`):** released is `rest`; fully pressed is `max` when
  `travel = +1`, otherwise `min`; `n = clamp((v - rest) / (full - rest), 0, 1)`. The captured travel sets released
  and full first; then `inverted`, the player's explicit choice (Wheelkit offers it for pedals too), gives
  `n = 1 - n`, as art of rally's production `Binding.Normalize` does. A binding with `full == rest` is invalid.
- **Button:** the shared sample is a boolean, down (1) or up (0). Each reader converts at its own boundary, before
  normalization: a raw DirectInput byte is down when bit 0x80 is set (`diButtonDown`); a reader that already exports
  booleans (the toolkit's native reader, `ReadDeviceStateWithPov`) gives 1 for down, and 1 is never read as released.
- **Hat direction:** pressed when the POV is not centred (`-1`, or low word `0xFFFF`) and the circular distance between
  the POV and `angle` is at most 4500 (so a diagonal presses both neighbours, as most games treat it).

**Denormalization** (used only by test injection, section 6) is the exact inverse, with the result rounded to the
nearest integer, halves away from zero (C++ `std::lround`, C# `Math.Round(v, MidpointRounding.AwayFromZero)`), then
clamped to `range`; `n` is clamped to its shape's range first: steer `v = centre + n * half` (`-n` when inverted); pedal
`v = rest + n' * (full - rest)` with `n' = 1 - n` when inverted, else `n`; button `n >= 0.5` -> down (1), else up (0),
which a DirectInput writer delivers as 0x80 or 0; hat `n >= 0.5` -> `angle`, else -1.

## 4. Text form (one line per binding)

Used by the `[Controls]` INI storage adapter, in mod logs, and in the shared test vectors.

```
binding   = kind SP index [SP angle] *(SP attribute)
kind      = "axis" / "button" / "hat"
index     = 1*3DIGIT
angle     = 1*5DIGIT                                 ; hat only, 0-35999
attribute = "dev=" guid / "prod=" guid
          / "range=" int ".." int / "rest=" int / "travel=" ("+1" / "-1")
          / "inverted" / "calibrated" / "name=" quoted
int       = ["-"] 1*10DIGIT
guid      = "{" 8HEXDIG "-" 4HEXDIG "-" 4HEXDIG "-" 4HEXDIG "-" 12HEXDIG "}"
quoted    = DQUOTE *(%x20-21 / %x23-5B / %x5D-7E / "\" DQUOTE / "\\") DQUOTE
```

- Writers emit attributes in exactly this order: `dev prod range rest travel inverted calibrated name`, single spaces,
  GUIDs in lowercase. The same binding then always has the same bytes. Readers accept any order and GUIDs in either
  case.
- Required: `dev` and `prod` for every kind; `range`, `rest` and `travel` for axes. `angle` only for hats. A missing
  required field, a value out of range, a duplicate attribute or an unknown kind makes the binding **invalid**: it is
  reported and not applied. It is never guessed.
- Readers ignore unknown `key=value` attributes (no spaces in them) for forward compatibility; schema-1 writers never
  emit any.
- An empty value means "explicitly unbound". A missing key means the profile says nothing about that action.

Example (the owner's steering, 2026-10-09):

```
steer = axis 0 dev={d71b8350-61b7-11f1-8001-444553540000} prod={0006346e-0000-0000-0000-504944564944} range=0..65535 rest=33265 travel=-1 name="MOZA R12 Base"
```

## 5. The `[Controls]` INI storage adapter (`ini-controls-1`)

For native mods whose game has no editable binding store, or whose store the mod rewrites from this section at start
(V-Rally 4). It lives in the mod's own settings INI, the file Wheelkit and F6 already edit (STD-032):

```ini
[Controls]
Schema = 1
Profile = Moza KS Pro Triple
Revision = 439a4118e99353997c8df8555ca6b6c4f07db89590c2b0a5ea60ba02b130d36c
Transmission = Sequential
steer = axis 0 dev={...} prod={...} range=0..65535 rest=33265 travel=-1 name="MOZA R12 Base"
throttle = axis 2 dev={...} prod={...} range=0..65535 rest=0 travel=+1 name="MOZA R12 Base"
shiftUp = button 9 dev={...} prod={...} name="DS-8X Shifter"
navUp = hat 0 0 dev={...} prod={...} name="MOZA R12 Base"
```

`Schema` other than 1: the mod applies nothing and logs why. `Revision` is Wheelkit's profile content hash, echoed by
the mod so a reader can tell which profile is applied. Keys are the action ids of section 1.

## 6. Evidence, and the test injection

Three results, reported separately per action, never merged into one badge:

1. **Configured:** the mod read the bindings and wrote its game's store. Each mod logs one line at start:
   `controls: profile "<name>" revision <rev> via <adapter>: applied <n> (<ids>); unsupported <m> (<ids>); invalid <k> (<ids>)`.
   Wheelkit reads it back (as it reads the start line, STD-032).
2. **Input observed:** a raw device sample injected **before** the game's binding resolution makes the game's own
   input or car state respond. The mod's telemetry or memory reader is the observer (e.g. Forza steer byte 320, accel 315,
   brake 316, clutch 317, handbrake 318, gear 319). Synthetic pose or tape playback bypasses the bindings and never
   counts here.
3. **Owner accepted:** at the rig.

Test injection is development-only and gated twice. The mod reads its dev channel only while
`%LOCALAPPDATA%\dbce\<game>\inject.on` existed when the game started. That switch is separate from the force mute,
and a command file is honoured only when it is newer than the game process. Commands:

- `inject raw <kind> <index> [angle] dev=<guid> value=<raw> ms=<50-15000> [range=<min>..<max>]`: the game's next
  reads of that object on that device instance return `value` for `ms`. An axis value is in `range` (default 0..65535),
  rescaled to the range the game set; a button takes 1 (down) or 0; a hat its angle or -1.
- `inject action <id> <n> ms=<50-15000>`: resolves `<id>` through the applied bindings, denormalizes `n` (section 3),
  then does exactly what `inject raw` does. The game still interprets the raw sample itself.

The mod logs each injection with the raw value it delivered. While an injection runs, everything else about the
device is the real device. Injection never creates OS input, never focuses the game and never sends a force.

## 6a. The force fence for these runs

An unattended run that injects controls needs the wheel visible to the game, so the force mute that hides the wheel
cannot be used. Native DirectInput proxies use `native/dinput-proxy/dbce_fence.hpp`, a closed wrapper.
- The game never holds a real DirectInput or device object.
- Effects are software sinks, and no force command, escape or output reaches a device.
- Injection is delivered only into the standard joystick formats.
- `test_fence.cpp` checks this against a recording fake device.
- `native/dinput-proxy/tools/fence-live-probe.cpp` checks a built proxy on the real devices without the game.
  - It is read only; build it under the game's exe name.
  - First run: V-Rally 4 0.6.0, 2026-10-09 21:49. Every raw and action sample was delivered and ended as commanded.

## 7. Shared test vectors

`native/controls/controls-vectors.txt` holds parse, reject, normalize and denormalize cases. The C++ header
(`native/controls/dbce_controls.hpp`) and Wheelkit's C# both run all of them; a change to section 3 or 4 changes the
vectors in the same commit.
