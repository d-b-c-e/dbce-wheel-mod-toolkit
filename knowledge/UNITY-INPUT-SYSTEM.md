# Unity Input System (the new one) — traps and the working patterns

Source: iracing-arcade-wheel, 2026-09-02/03, Unity 6 IL2CPP. The patterns are
engine-level and apply under Mono too.

## `supportedDevices` silently drops everything unlisted

`InputSystem.settings.supportedDevices` was `Mouse, Keyboard, Gamepad`. Every
other HID device is discovered, filed as *unsupported*, and never becomes a
device — a wheel base, shifter or stalk cannot appear in the game's controls
screen no matter what. Set it to an **empty** list (empty = support everything;
under IL2CPP: `new ReadOnlyArray<string>(new Il2CppStringArray(0))`). Verified:
before, Keyboard + Mouse; after, `HID::SIMAGIC DS-8X Shifter` and `HID::Gudsen
MOZA Multi-function Stalk`. The 35 other HIDs on the machine stayed
unsupported because no layout matched them — nothing junk appears.

## Interactive rebinding: use a private capture action

- `PerformInteractiveRebinding` **refuses an enabled action**, and disabling
  one of the game's actions inside its enabled map and re-resolving left that
  action **permanently dead** (Throttle ignored every key afterwards).
- Pattern: run the rebind on a private, never-enabled capture action
  (`new InputAction("capture", Value, "<Keyboard>/f24")`), then apply the
  captured `overridePath` to the real binding with `ApplyBindingOverride(index, path)`.
  Exclude `<Mouse>`, `<Pointer>`, `<Touchscreen>` and your own hotkeys.
  `WithExpectedControlType` from the composite part / action type. 10 s
  timeout, Esc cancels. Poll completion from the frame tick.
- `Clear` = `ApplyBindingOverride(index, "")`; `Reset` = `RemoveBindingOverride(index)`.

## Persist only overrides, and re-apply them

Store `{Map, Action, Id, Original, Path, Processors, Interactions}` per
overridden binding, keyed by the binding **GUID** with a map/action/path
fallback, as JSON in the plugin config folder. Apply on asset-found,
asset-replaced and scene-load, and **verify every ~3 s** — another package's
own binding load (InputIcons' `loadAndSaveInputBindingOverrides`) undoes yours
on scene load. Count repairs.

## Pedal / invert / deadzone as processors on the override

Compose `invert` → `normalize(min=-1,max=1,zero=-1)` → `axisDeadzone(min=d,max=1)`
→ `clamp(min=0,max=1)` into the binding's processors string so they persist
with the binding; parse back with a paren-aware split and strip the game's own
leading processors first.

## HID layouts only cover the Generic Desktop page

Unity builds layouts for Generic Desktop axes (X/Y/Z/Rx/Ry/Rz/Slider/Dial/
Wheel) and the Button page. **Simulation Controls page** elements — steering
0xC8, accelerator 0xC4, brake 0xC5, clutch 0xC6 — and vendor pages are
dropped: the device appears, buttons work, pedals never register. Register a
layout JSON (`InputSystem.RegisterLayout(json)`) from a folder of
`layouts\*.json` with the byte offsets; diagnose the gap by walking
`hid.hidDescriptor.elements` per usage page and comparing against the
`AxisControl`s the layout exposes.

## Finding the game's actions

Primary: the game's own manager (`InputIconsManagerSO.Instance.usedActionAssets[0]`
in iRacing). Fallback: `Resources.FindObjectsOfTypeAll(InputActionAsset)` scored
by map count, bonus for a `Driving` map. Watch for the asset being replaced.
`InputSystem.ListEnabledActions()` dumped once when a race is live is the
authoritative "what does the game offer" — and it found an unused
`Driving/NextCamera` action worth borrowing for a view cycle.

## Mod-owned bindings

Bind by "press F-key, then any button on any non-keyboard device"; save the
`ButtonControl.path` string; resolve with `InputSystem.FindControl(path)`,
cache by path, evict on throw. Register the mod's own keys as
`<Keyboard>/name` exclusions so a rebind cannot capture them.

## Focus and injection

- The Input System disables the keyboard while the game is unfocused
  (`backgroundBehavior`); exclusive fullscreen minimises on any focus loss.
- The game ignores OS `SendInput` (Raw Input drops device-less events);
  inject with `StateEvent.From(keyboard, out evt)` at state offset 24 and
  `InputSystem.QueueEvent`.

## Shared-input startup before a primary player exists

[dbce-mods-iracing-arcade, 2026-10-13, source `fb6c9a9`]

The stock LoginScene joins an unpaired gamepad through
`PlayerInputManager.JoinPlayersWhenButtonIsPressed`. Waiting for an existing
primary player before publishing any wheel menu input creates a startup cycle:
the input needed to create the player never reaches the game. The adapter can
open one neutral, unpaired menu pad first, while leaving gameplay axes unowned.
Verify the game's actual join mode, prefab/actions, device eligibility and focus;
do not assume every Unity game uses this route. Preserve stock keyboard input.

Use a released-then-pressed physical menu action, not an automatically generated
join press. Losing an action's live status revokes its release proof. Unity can
pair the device and deliver the same event to newly enabled actions in one
update; do not publish another press during handoff. Neutralize the pad, verify
the exact primary user/action asset, and then admit the axis source.

Cache the native read at the shared source boundary for the entire engine frame,
including failed reads. Caching only inside axis getters is insufficient: menu
bootstrap may already have read the provider before admitting it to those getters.
The regression sequence is menu tick(N), admission, getter(N): one native read;
getter(N+1): one new read. A fixture with separate menu and axis sources misses it.

Cleanup must retain the owned pad reference if removal fails, retry independently
of native shutdown, and keep native bindings quarantined until removal is verified.
Before refreshing native routing, clear its deferred-restoration quarantine; if
refresh throws, restore the pending state. Otherwise a apparently successful
refresh may take its early exit and leave the native controls disabled.

The cited candidate passed focused fixtures and a clean plugin build. Runtime
joining and physical wheel input are still unverified. An in-process diagnostic
keyboard event can test stock keyboard joining and adapter handoff; it does not
qualify wheel-button forwarding. Keep this separate from latched/injected shared
frames, which the ordinary input route deliberately refuses.
