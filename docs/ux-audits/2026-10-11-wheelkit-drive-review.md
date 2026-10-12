# Wheelkit and DRIVE F6: shared UX review

2026-10-11. Owner-requested review by two independent agents: visual/usability and interaction/architecture,
consolidated by romi. **Recommendations for the overnight pilot, not an implemented UI or a runtime pass.**
No app/game launch, install, force change or heavy build was performed for this review.

## Evidence and owner outcome

- Owner screenshot: DRIVE `0.2.3-rc.2`, Simple / FFB. The panel opens but the owner finds its presentation rough.
- Wheelkit `0.6.18` packaged smoke images, PID 41780 at 21:19: profile FFB, DRIVE settings, Controls and live mappings.
  These are fixture-rendered app pages, not evidence of successful live settings changes.
- Private images retained in the portfolio hub's `artifacts/drive-ux-owner-20261011/`.
  Owner screenshot SHA-256: `429CF7A4ED6AE9AFF56AB77BF5F20D8952E55393A437F25BBA505CFF8E8D3D2C`.
- Owner confirms the game's title/car/stage menus are now centred. Side screens flash white during transitions
  before returning to black. Track this separately from F6 design; centred placement passes, transitions remain open.
- Owner intentionally deferred a new recording until Play with recording supports their arbitrary menu navigation,
  car/stage choices and drive. Preserve the existing recordings and their narrower qualification claims.

## Priority findings

| Priority | Finding | Pilot correction |
|---|---|---|
| 1 | F6 renders tabs, Off/On choices, +/- controls, Advanced navigation and Close as large raised buttons of similar weight. | Introduce semantic widgets: flat tabs, bounded switches, sliders with numbers, compact command buttons. Keep visible keyboard/controller focus distinct from selection. |
| 1 | FFB says On, then repeats `inactive: settings` in two places. It reads like a fault without explaining the temporary suppression. | One status area: `Enabled · Paused while settings are open`. Use the actual runtime reason; enabled preference and output activity are separate facts. |
| 1 | Steering/crash values need broad +/- buttons; road uses `0.5` while the neighboring strengths use percentages. | Aligned strength rows: label, slider, editable value/unit. Keep precise keyboard/controller steps. Declare conversion in the contract; do not silently relabel a multiplier as a percentage. |
| 1 | Shared bindings display different button numbers: Wheelkit `BindingDisplay` adds 1; DRIVE `ShimPanel.Describe` exposes the stored index; DRIVE's shifter page adds 1. | One-based player-facing button names everywhere, while preserving stored zero-based indices. Test the same binding in both hosts. |
| 1 | Wheelkit game settings are a long flat list with page suffixes and configuration-path detail; profile pages already have a clearer tab treatment. | Reuse the tab/row language for game settings. Group by category; move paths and keys to details/support. Avoid nested scrolling. |
| 2 | Global view selection and Close dominate F6's header; help paragraphs compete with values and leave excess space below. | Compact header and view selector; concise inline descriptions with focus-accessible help. Keep essential errors and recording Stop visible. |
| 2 | The screenshot has no dedicated Stop FFB action or visible feedback-device summary, despite the common UX guidance. | Show the effective device, and a plainly labelled Stop FFB action with its shortcut. A grey button must explain why it is unavailable. Never imply a successful stop without runtime acknowledgement. |
| 2 | `research` in the main title and duplicated status/footer text distract from ordinary play. | Small version/build status in a quiet location; one save/result footer and one output-status area. Keep diagnostic detail in Help/Advanced. |

The default layout should be compact without making text or hit targets too small at the rig. Measure against the
centre monitor's usable viewport and user UI scale, not the entire three-screen span.

## Proposed FFB page

```text
Wheel settings                          Simple / Advanced       Close
Controls    FFB    Cameras    Driving    Telemetry    Help

Force feedback                                      [Enabled]
MOZA R12 Base · Paused while settings are open        Stop FFB

Steering strength          ───────●────────                50%
Cornering resistance and return to centre.

Effects
Crashes                    [Enabled]  ─────●────           50%
Road rumble                           ───●──────     <unit/value>

Invert direction                                    [Off]

Saved to this game                               F6 / Esc closes
```

Device name is illustrative, discovered at runtime. Do not hard-code owner hardware. Road units await the shared
descriptor's explicit conversion. Tuning remains in the existing global Advanced view; do not introduce another
independent per-page expert toggle. Keep safe recovery and essential binding/device setup in Simple.

This is a layout guide, not a requirement to copy WPF pixel dimensions into Unity. Flat surfaces, spacing,
typography hierarchy, accent/focus treatment, labels and interactions should match within host constraints.

## Reuse what exists

- DRIVE already separates engine-independent panel commands (`ResearchPanel`, `IResearchUi`) from Unity drawing.
  Extend that seam with semantic controls rather than creating another panel system beside it.
- Wheelkit has `ModSetting`, `ModSettingRules` and the transactional INI/XML/JSON editor. Extend these rather than
  introducing another profile/config writer.
- Existing shared native overlays are useful host renderers. They are not a complete common settings contract
  consumed by both Wheelkit and every mod. DRIVE's Wheelkit catalog fields remain separately maintained today.
- Each mod publishes a versioned descriptor scoped to its mod/provider ID. It carries stable IDs, labels, category,
  widget kind, units/conversions, ranges/steps, defaults, dependencies, availability, help and application timing.
  Toolkit types/rules are reusable; game keys/hooks and complex adapters stay in the consumer.
- Share binding/action names and display rules. Current examples of drift include Accelerator/Throttle,
  Navigate up/Menu up, Mod menu/Settings and Mute force feedback/Stop FFB. Pick one player vocabulary against UX-1
  and migrate labels without renaming stored action IDs.
- Share validation, capture/calibration states and edit intentions. Keep device access behind host capabilities;
  using the UI must not create competing device readers or force writers.
- WPF, Unity and native overlays retain compatible renderers. Do not load game/Unity assemblies into Wheelkit or
  make public mods require a private toolkit checkout.

## Saving and configuration ownership

The game's effective configuration remains separate from a reusable rig profile. F6 adjusts this game; it does
not silently rewrite the global profile. Wheelkit must read those changes and preserve them until an explicit
Apply Rig Profile operation.

Make state vocabulary consistent: Editing, Saving, Saved, Next start, Active, Failed. **Saved is not proof of active
runtime behavior.** Retain intentional host differences: F6 may apply eligible live adjustments immediately;
Wheelkit's offline editor can use explicit Save. Neither should claim a live application without acknowledgement.
Profile Apply and calibration completion stay explicit. Do not change save semantics merely for matching pixels.

Wheelkit currently guards changes during its write transaction, but its initial editor read does not retain the
base revision. The pilot needs to detect a same-field F6 edit made after the editor opened: compare intended
changes against the base values, preserve unrelated newer fields, and show a conflict rather than silently win.

DRIVE's shim Controls page is currently read-only and the shim reads bindings at startup. A standalone binding
editor can save the same canonical file, but must say restart required unless an acknowledged reload path is
implemented. A completed bind in a UI alone must not earn a working/configured badge.

## Standalone setup recommendation

Keep the existing receipt-based standalone installer. Prefer a reusable, compact F6 binding/setup page writing
the same shim contract, with honest restart requirements. Wheelkit provides discovery, profiles, updates and
bulk Apply. If a game cannot offer usable in-game setup, evaluate a portable single-game host built from the
existing Wheelkit editor/services. Do not create a bespoke editor for every mod. This optional route is not a
new release blocker or a decision to require Wheelkit for all configuration.

## Bounded overnight pilot

1. Freeze a small DRIVE FFB descriptor and binding-display slice with the accepted values unchanged. Render it
   in Wheelkit and F6; check actual screenshots on one screen and the centre third, including large UI scale.
2. Exercise pointer plus keyboard/wheel-button navigation: visible focus, left/right adjustment, Confirm, Back,
   capture cancel, disconnect and unsupported states. Help cannot require hover; selection cannot rely on colour.
3. Verify both hosts produce equivalent effective settings, preserve unrelated keys, and show the real save/apply
   state. Include one stale same-field edit and the button-index comparison; use existing qualification fixtures.
4. After the DRIVE slice works, try the same descriptor/control model in OutRun decomp's native host. Expand by
   useful categories; do not require a portfolio rewrite before the first cleaner panel can ship.
5. Diagnose the side flashes with a bounded game-window clip around menu/scene transitions. Source leads:
   `MenuCentre.Begin/Stop/BeforeRender`, black-clear camera lifetime/order, newly created transition canvases and
   `TripleView` handover. A missed clear or full-span fade is a hypothesis, not established by the screenshot.
   Check the centre transition remains correct and sides do not flash; do not use display-resolution changes.

Full-session recording is a separate functional work item in Wheelkit's overnight queue. Its improved visible
status and Stop affordance should use this UI vocabulary; recording correctness must not depend on the redesign.
