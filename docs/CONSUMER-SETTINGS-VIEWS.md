# Simple and Advanced settings — UX-01-S

Normative [UX-1](CONSUMER-UX.md) detail, clarified 2026-09-23. Every consuming mod starts
with **Simple** settings: the minimum needed to connect, bind, check and drive
using a conservative, game-tested default tune. **Advanced** adds customization
and detailed diagnostics. This is a shared product contract; publishing it does
not change existing consumer UIs or certify their adoption.

Use the [interactive visual reference](reference/wheel-settings.html) and its
[preview guide and verification record](reference/README.md) when implementing
the layout. It demonstrates the same flows using browser-only sample data;
it is not a shared game renderer or evidence of consumer runtime behavior.

## One view selector, one set of settings

Keep **View: [Simple] [Advanced]** in the stable Wheel settings header, visible
on every page. Use that wording and order, a clear selected state, and keyboard
focus/activation. Do not scatter separate Expert, Debug or Advanced mode switches
through individual pages. Selecting Advanced needs no warning or confirmation.

- New configurations and upgrades with no saved view preference open in Simple.
  Remember an explicit view choice per mod across panel closes and game restarts.
  Existing bindings or custom tuning do not imply an Advanced preference.
- First use opens Controls, or Setup when it provides a distinct guided workflow.
  Omit a Setup/Overview that merely duplicates Controls/FFB. Thereafter retain
  the selected page when switching views, keeping the five core page names and
  relative order. If an Advanced-only section disappears, return to its Simple
  parent or the consumer's first-use page and restore focus. Preserve serialized
  page meanings when removing a page; see the migration contract below.
- Advanced includes the same basic controls in the same order, followed by
  clearly grouped extras. Both views edit the **same saved values**. There is no
  Simple profile, second binding store or separate Apply step for the view.
- A view change saves only presentation preferences (view and page). It must
  not reset a tune, change a binding/camera pose, reconnect a device, start/stop telemetry or
  recording, or change FFB Off/On. Normal panel-open suppression still applies.
- During input capture, calibration or unapplied connection edits, keep the
  selector and links that would change view disabled with
  `Finish or cancel the current edit to change view.`
  Preserve working values on cancellation; never silently discard an edit.

**Simple is a presentation choice, not a claim that defaults are active.**
Hidden settings continue to work. If hidden tuning differs from the consumer's
documented defaults, show one quiet page-level summary, for example
`Custom FFB tuning active. Review in Advanced`. The link selects Advanced and
opens that section. Do not list every changed number in Simple or offer an
automatic reset on the way back. Bindings and calibrations are ordinary setup;
do not label them advanced customization.

## What belongs in each view

These are placement requirements, not a list of features every game must invent.
Use the normal UX-1 capability messages and game restrictions. **Simple on
demand** controls open in a plainly named section within Simple, without config
editing. Explicitly labeled Advanced links in the table are shortcuts to
optional details; required setup must stay in Simple.

| Page | Simple: visible initially | Simple: on demand | Advanced adds |
|---|---|---|---|
| **Setup (optional)** | A distinct guided first-use step, check and next action; omit duplicated overview content | Binding/calibration or recovery for the current step, using the same settings | Detailed inspection via its owning page; no second setup wizard |
| **Controls** | Steering, Throttle, Brake, Handbrake (axis) rows with friendly assignment, live value and Bind/Calibrate/Clear | Grouped Driving buttons, Menu buttons, Mod buttons and Shifter bindings; clutch where supported; all ordinary keyboard/wheel rebinding; calibration with direction/inversion, endpoints and deadzone | Response curves, sensitivity shaping, saturation, manual raw-range editing, digital handbrake threshold tuning, raw action/device details |
| **FFB** | FFB Off/On, FFB device dropdown, Strength, plain status/recovery | Refresh or explicit device replacement; brief explanation of Strength | Per-effect gains, model/profile selection, smoothing, damping/friction, slip/load scaling, editable limits, response graphs and delivery counters |
| **Cameras** | Bonnet and Bumper Off/On, Change camera binding, short live-adjustment hint | Look behind binding, **Adjustment bindings** with the full rebind workflow and actual saved keys | Selected mount's position/tilt/FOV sliders, numeric tuning, adjustment speed/step settings and scoped pose reset |
| **Telemetry** | Telemetry Off/On, tested receiver preset, concise actual destination and Sending/Off/Unavailable status | Matching receiver setup instructions, **Connection settings (Advanced)** link; if recording is active, status and Stop recording | Editable host/port, protocol/rate/channel choices, signal availability/provenance, recording configuration and start/mark/open-folder tools |
| **Help** | Version, short symptom-based help, UI scale if not supplied by the host, Create support file | Clear next recovery action and **Details (Advanced)** for an error; local support-file location | Full device inventory/identifiers, raw values, logs/counters and broad reset/backup tools |

**Close**, **Stop FFB**, save failure and current blocking errors stay visible in
both views. A connected device or ready state needs one short status, not a
permanent diagnostics dashboard. Stop FFB remains available on every page.
An installed feature is **Available**, not proof the player's setup is **Ready**.
With no device or required binding, Simple shows the specific next action even
when the installed binary supports calibration and feedback.
Ordinary control-level default restoration remains beside its control in either
view; moving broader reset tools to Advanced does not remove that affordance.

The optional **Driving** page is Advanced by default for handling experiments,
assist tuning and physics overrides. If a supported transmission selector or
other setting is required for ordinary driving, expose that small operational
section in Simple as well, in the standard page position. Keep physical shifter
and shift-button bindings in Controls. Document why each Simple addition is
necessary; do not copy every game-specific parameter into it.

### Keep the initial screen short

Simple is not an Advanced panel with a few sliders removed. Use the allowlist
above: do not show empty expert groups, disabled expert rows, raw device values, model
names, dense graphs or long explanatory paragraphs. Common button-binding groups
start collapsed and are clearly labeled; the four main axis rows remain easy to
find. Showing a group is navigation, not another operating mode.

Apply [UX-03-G](CONSUMER-UX.md#binding-ownership-and-useful-space--ux-03-g):
bound each action and its editor visually, keep help local, and use recognizable
flat Show/Hide disclosures. Perceptible surfaces/borders and spacing must separate
groups in the real host. Use compact flat tabs with a clear selected indicator;
keep instructions left and compact commands right, with a named stacked fallback.
Reserve raised/button styling for commands, at content-sized or bounded widths;
do not stretch support, reset, preset or native-menu commands across a wide page.
Use compact segments for short mutually exclusive settings, and flat links for
Advanced navigation. Stack/expand only when the narrow layout needs it for access.
At the default host size, initial Controls must include
one complete binding group with its actions. Fewer options do not compensate
for ambiguous button ownership or a header that consumes the usable page.

At 1280×720 and the default readable scale, a retained Setup's current guided
step, next action and persistent controls must fit without scrolling. Its first screen must not
present all bindings or the entire device inventory at once. Longer optional
binding lists may scroll inside the page; keep navigation and the view selector
fixed. Do not shrink text to meet this requirement. Verify high DPI/4K as well.

Keep first-use instructions to connect → axes → desired buttons → check FFB →
drive. Handbrake ownership, telemetry, recording, physics choices and camera
tuning are optional. Readiness must not report an incomplete setup merely
because one of these optional features is unconfigured.

### Do not make basic setup depend on Advanced

A Simple command that opens the game's own binder follows
[UX-03-H](CONSUMER-UX.md#programmatic-native-menu-handoff--ux-03-h): release the
actual initiating control, pass a short neutral-frame barrier and explicitly
transfer ownership. Resting mapped wheel/pedal axes or a held gear must not trap
that destination behind the overlay's input block.

- **Handbrake:** Bind must start capture directly in Simple. Complete rest/full
  calibration, inversion, deadzone, Save/Cancel and a live 0–100% preview there.
  Axis and button bindings coexist. For a digital-only game, explain the
  limitation and show effective activation/release points and Game input On/Off
  in calibration. Their fine tuning can live in Advanced; verified defaults
  must work with the calibrated range. See [the exact workflow](CONSUMER-CONTROLS-CAMERAS.md).
- **FFB device:** Keep the dropdown in Simple, defaulting to Use steering wheel
  and preserving an explicit override. A missing/ambiguous/non-FFB selection
  shows its reason and the relevant Bind/Refresh/select action there. No silent
  fallback, Next-device-only picker, or second session-start switch.
  The bounded expanded list uses flat choices and a visible selected marker;
  a saved-disconnected choice remains visible but unselectable. Refresh is a
  separate compact command beside the picker, not a device-choice row.
- **Camera adjustments:** Live numpad defaults work regardless of settings view.
  Simple Cameras exposes Adjustment bindings directly, so someone without a
  numpad can rebind movement, tilt, FOV and reset without entering Advanced.
  Show actual saved shortcuts on demand, not a stale hardcoded legend. Detailed
  pose sliders move to Advanced; camera cycle and ownership behavior do not.
- **Telemetry:** A verified common receiver preset must work with On and matching
  receiver instructions. Show the real destination even for a saved custom one.
  Connection settings (Advanced) goes directly to the editable destination and
  preserves atomic Apply connection/Cancel. Do not silently replace custom ports
  when returning to Simple. If a game needs extra information even for the normal
  preset, expose only that required field and document the exception.
- **Errors:** Simple shows what stopped working and one next action. Technical
  codes and dumps go behind Details (Advanced). A write failure, recording limit,
  lost device or FFB inactive reason must never disappear because its underlying
  setting is Advanced. Show active recording and its stop action in Simple even
  when an agent or Advanced started it.

## Defaults and migration

Ship tested game-specific input and camera defaults, conservative FFB tuning and
effective output/lifecycle limits. Those safeguards remain active in both views;
view mode must never gate them. Keep UX-05's single saved FFB Off/On preference:
new configurations default On, saved Off stays Off, and F8/Stop saves Off until
the player chooses On. Selecting either view cannot enable output or remove an
ownership/focus/pause/stale-data gate. No unattended torque is needed for adoption.

An upgrade adds a view preference without replacing the owner's settings,
calibrations, independent device identities or camera shortcuts. Back up migrated
configuration as usual. A settings file containing only older fields is valid;
use Simple for the absent view field. Unknown view values fall back to Simple
without resetting unrelated data. Never infer "new user" from a missing view
field and rerun first-use defaults over an existing tune.

Page identity is saved meaning, not the position of a visible tab. Prefer stable
page IDs; a consumer that already stores numeric IDs must retain their meanings
or migrate them explicitly with a versioned, idempotent map. For example, if old
IDs are Setup=0, Controls=1, FFB=2 and Cameras=3, hiding Setup must not reinterpret
the saved 2 as Cameras. Redirect only the removed Setup selection to Controls;
preserve all surviving page choices and the Simple/Advanced preference. Define
a safe first-use fallback for unknown IDs without resetting runtime settings.
Verify each legacy page ID, both views, unknown IDs and a second load/save cycle.

Reset FFB settings, Reset this view and Restore numpad defaults retain their
existing, distinct scopes. Returning to Simple invokes none of them. Agent-led
recording retains its one-launch bounds, expiry and output policy; hiding its
configuration does not change that contract.

## Implementation and reconciliation

Classify each existing setting/action before moving controls. Keep this inventory
in the consumer's UX adoption notes; new options need a placement decision in
the same change. Unclassified tuning belongs in Advanced until a normal setup
need justifies Simple. Do not expose every new option in both views by default.

| Setting/action (saved key) | Page | Placement: Simple / Simple on demand / Advanced only | Default and unit | Reason or game exception | Source and interaction evidence |
|---|---|---|---|---|---|
| Example: Strength (`strength`) | FFB | Simple (also in Advanced) | Consumer's tested default, % | Main feedback adjustment | Fill with actual source/package evidence |

Keep presentation preference separate from runtime configuration decisions.
Do not rebuild a wheel connection or materialize a default settings object
when rendering a different view. Route duplicate entrances to the same binding,
validation and save operations. The toolkit does not yet supply these widgets;
use the game's verified renderer and the common wording/layout contract.

Adopt this per consumer in order: inventory the current controls, apply the
placement/default/migration rules, then walk the packaged UI. Track source
implementation and attended verification separately. A new view selector alone,
with all the old controls still exposed in Simple, is not reconciliation.

Complete the [UX checklist](CONSUMER-UX-CHECKLIST.md), including **UX-01-S**:

1. **Fresh configuration:** F6 opens Simple Controls, or a distinct guided Setup
   if retained. Confirm the page is useful rather than a duplicate overview.
   Complete a first drive using only Simple, including direct analog handbrake setup and camera shortcut
   rebinding where required by UX-04/06. Verify no required step sends the player
   into Advanced or a config file.
2. **Existing configuration:** Start from a copy with custom tuning, saved FFB
   Off, camera bindings and explicit FFB device, but no view preference. Confirm
   Simple appears, the old values remain effective and hidden tuning is summarized.
3. **Round trip:** Change a value in Advanced, return to Simple, then reopen
   Advanced. Check the same saved/effective value, meaningful custom summary and
   retained bindings/device. View changes must not cause output, reconnection or
   stream/capture changes. Test invalid/missing view values and a save failure.
4. **Persistence/editing:** Explicit Advanced survives close/restart; explicit
   Simple does too. Switch from an Advanced-only section; check the fallback page
   and keyboard focus. During capture/calibration/connection edits, finish or
   cancel before switching; the last working assignment remains intact on cancel.
5. **Failures and discovery:** In Simple, inspect a disconnected wheel, ambiguous
   device, unavailable output, stopped FFB, write failure and active/failed capture
   where applicable. Check recovery/stop actions and one-click routes to the
   matching Advanced detail. Inspect both views at 720p and high DPI/4K.
6. **Page migration:** Load every old page ID after removing/reordering pages.
   Surviving choices retain their meaning, removed Setup goes to Controls, and
   unknown values use the documented fallback. Repeat save/load and verify no
   binding, tune, output preference or view choice changed.

Record guidance commit/hash, source and packaged version, the per-setting
inventory and results/screenshots. Source/configuration fixtures can establish
policy and persistence; they cannot establish readability or attended wheel
behavior. Mark live steps Not tested until performed. Earlier consumer audits
remain historical; none establishes adoption of this 2026-09-22 clarification.
