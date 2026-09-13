# Shared player experience for wheel mods

Baseline **UX-1**, 2026-09-13. This is the product standard for consuming mods,
written in response to the owner's reports that Art of Sim Rally, iRacing
Arcade Wheel and Woden Rally Edge Wheel were becoming harder to use consistently.
It specifies the intended experience; it does **not** certify existing releases.
The toolkit does not yet provide a shared UI or calibration library.

The first [iRacing Arcade / Art of Sim Rally audit](ux-audits/2026-09-13-consumers.md)
records source-backed departures and a prioritized adoption plan.

Use this with the [install and first-drive guide](CONSUMER-SETUP.md),
[acceptance checklist](CONSUMER-UX-CHECKLIST.md) and
[technical integration guidance](CONSUMER-EXPERIENCE.md). Game hooks, drawing,
preferences and game-specific capabilities remain in each consuming repository.

## The promise to the player

Someone who has set up one of our mods should recognize the next one: install,
open **F6**, check the wheel and pedals, bind buttons, enable feedback, drive.
Routine use must not require editing configuration files, reading developer
logs, knowing an input backend, or understanding a force equation.

Consistency means the same actions, words, grouping and feedback. It does not
require changing a game's loader or copying an incompatible rendering backend.
Keep useful game-specific features, but fit them into this structure.

## 1. One entrance and one navigation order — UX-01

**F6** opens and closes **Wheel settings**. A bound **Settings** button does the
same thing. **Esc** cancels a pending capture first; otherwise it closes the
panel. Show a **Close** button too. Keep the original loader's entry as a
secondary route when needed. Check game key conflicts and make the shortcut
rebindable; document any unavoidable default exception rather than hiding it.

Use these pages, in this order:

| Page | What belongs here | First things visible |
|---|---|---|
| **Setup** | First use, reconnect, quick readiness check | Wheel, steering/throttle/brake bars, the next incomplete step |
| **Controls** | All supported bindings and calibration | Steering, Throttle, Brake; then Driving buttons, Menu buttons, Shifter |
| **FFB** | Wheel selection, strength, tuning and output status | FFB Off/On, selected wheel, Strength, current status, Stop FFB |
| **Cameras** | Cycle membership and view adjustment | Bonnet and Bumper Off/On, Change camera binding, selected view's adjustments |
| **Telemetry** | Dashboard connection and session recording | Telemetry Off/On, destination, connection facts; Recording controls |
| **Help** | Version, troubleshooting, support evidence and reset | Short symptom-based help, Create support file, advanced device details |

Setup links to the same Controls/FFB values, not another set of bindings. Keep
raw action maps, HID reports, native device indices and library versions in
advanced details. Recording is a section of Telemetry, not a competing top-level
page. Refresh is beside the relevant device picker, with full inventory in Help.

If a game has handling/transmission modifications, put them in an optional
**Driving** page between Cameras and Telemetry. Preserve the six core page names
and relative order. Explain the actual mode restrictions at the controls. Do not
make a gearbox experiment the first thing a wheel user must configure.

Unsupported pages stay recognizable with a brief capability message and help;
omit unsupported controls within them. An unsupported feature must not appear
enabled, silently do nothing, or offer invented telemetry as a substitute.

## 2. Consistent wording and controls — UX-02

Use sentence case and familiar player language. Labels name the setting; controls
show its value. Buttons name an action. State is visible in text, not color alone.

| Purpose | Standard wording / control | Avoid |
|---|---|---|
| Boolean setting | `FFB: [Off] [On]` | `ON Allow FFB`, `Enable feedback?`, a button whose text could mean current or next state |
| Boolean shaping | `Invert steering: [Off] [On]` | `Flip`, unlabeled checkboxes |
| Numeric setting | `Strength: 20%` with slider, readable value and default reset | A unitless number, a reference constant presented as everyday strength |
| Action assignment | `Change camera: MOZA R12 · Button 34` with **Bind**, **Clear** | `listen`, `capture path`, a GUID in the main row |
| Empty / missing binding | `Not bound` / `Saved device disconnected` | Blank values or replacing the saved device automatically |
| Calibration | **Calibrate**, **Save calibration**, **Cancel** | **Apply** without saying what changes |
| Reset | **Reset FFB settings** / **Restore default binding** | An ambiguous global **Reset** next to a single row |
| Successful persistence | `Saved` | A Save button that is unnecessary or leaves changes silently pending |
| Unsupported | `Unavailable in this game: tyre temperature is not exposed.` | Plausible zero readings or permanently disabled unexplained widgets |

An Off/On pair has fixed left-to-right order, a clear selected outline/fill, and
a text value that remains legible when disabled. A platform-native switch may
be used if it still has the same label and explicit state. A read-only status
such as `FFB inactive — paused` is not a second enable switch.

Use these action names across mods when the actions exist:

| Group | Names in display order |
|---|---|
| Axes | **Steering**, **Throttle**, **Brake**, **Handbrake** |
| Driving buttons | **Shift up**, **Shift down**, **Handbrake**, **Change camera**, **Look behind**, **Reset car** |
| Menu buttons | **Pause**, **Confirm**, **Back**, **Menu up**, **Menu down**, **Menu left**, **Menu right** |
| Mod buttons | **Settings**, **Stop FFB**, **Start/stop recording**, **Mark recording** |
| Extra game actions | Use the game's own recognizable names, below common actions |

Analog and button Handbrake assignments are clearly labeled when both exist.
Show button numbers starting at 1; translate zero-based backend values only at
the UI boundary and retain raw values in support details. Show a friendly device
name and readable axis name (for example `X axis`) before any technical path.

Every advanced setting gets one sentence explaining what the player will feel
or see, its unit, and a way to restore its default. Example:
`Smoothing: 35 ms — Higher values soften rapid changes but delay feedback.`
Keep reference loads, slip scaling and force components under **Advanced**.
Game tunes can differ; the label, unit and meaning of Strength must not.

## 3. Shared visual and interaction rules — UX-03

Use an opaque dark neutral panel with one accent color for selection. Reserve
warning/error colors for actual conditions. Keep a stable title/navigation area,
a scrollable page body, and visible Close and Stop FFB actions. Changing pages
resets that page's scroll sensibly and never moves the navigation off screen.

The reference layout uses an 8-unit spacing grid, 16-unit page padding and
32-unit minimum row/button height at 100% scale. Body text starts at 16 logical
pixels, secondary help at 14, headings at 20. These are design targets, not raw
Unity pixels: scale all text, hit targets and spacing together. Offer UI scale
or honor the host's scale setting. Never fix help text to a tiny font that ignores
the player's chosen scale. Adapt to 1280×720 and verify a readable 4K rendering.

Align labels and values in consistent columns. Put **Bind** / **Clear** at the
same position on each binding row. Long device names wrap or show their full
value on focus; they must not hide action buttons. Use section headings and short
help text instead of long paragraphs between every control.

Support mouse, keyboard focus and keyboard activation; wheel/pad navigation is
the target once menu bindings are available. Show the focused control. If a
release requires a mouse, say so in setup and record the gap in its UX checklist.
Opening the panel suppresses driving input, camera shortcuts and FFB. Pause the
game only when allowed and only resume a pause the panel itself introduced.
Restore cursor/focus and release held inputs on close, focus loss and cancellation.

## 4. Binding and calibration are one guided operation — UX-04

Do not ask a player to choose between Rewired, HID, DirectInput or a virtual
controller. Select the verified input route internally. Keep each role's stable
device identity: wheel, pedals, shifter and handbrake may be separate devices.
Reordering USB devices must not silently change those assignments.

1. Click **Bind** on the desired action. Show `Turn the wheel` / `Press the
   throttle` / `Press a button`, an explicit **Cancel**, and a timeout.
2. Ignore the click that opened capture, resting pedal values and unrelated
   simultaneous input. If ambiguous, explain and retry without losing the old binding.
3. For steering, record centre and both ends; verify left/centre/right in that
   order. For a pedal, record released and fully pressed positions. Detect reversed
   ranges and show the result. Do not replace calibration until **Save calibration**.
4. Show normalized bars: steering **Left — Centre — Right**, pedals **0–100%**.
   Include the values the game actually receives, or explicitly label a bar
   **Device input** if final game input has not been verified.
5. Allow **Bind**, **Clear**, **Calibrate**, inversion and a small deadzone adjustment
   in the same section. Offer default reset without deleting unrelated assignments.

Preserve stock keyboard/gamepad/menu controls when adding a wheel binding. Flag
conflicts within the same context; offer Replace/Keep both/Cancel only when that
choice is real. One assignment can deliberately serve different game contexts.
Do not let Settings or Stop FFB be consumed as an ordinary capture accidentally.
Hold actions (Look behind) and one-press actions (Change camera) must be labeled
and tested accordingly. Do not synthesize repeated presses while a button is held.

Unplugging a device preserves its bindings, releases its contributions and shows
`Saved device disconnected — reconnect it, then Refresh.` A different device is
an explicit replacement. Calibration/refresh uses no force output.

## 5. FFB has an understandable state — UX-05

Keep **FFB: Off/On** as the saved preference. Present **Strength** as a percentage
of the mod's available output; do not imply it is Nm or a guaranteed wheel-base
torque. Show the selected physical wheel and status beside the control.

| State | Player-facing example | Behavior |
|---|---|---|
| Disabled | `FFB off` | No output |
| Device absent | `Wheel disconnected — reconnect it, then Refresh.` | Retain selection; no substitution |
| Available | `Ready — feedback starts while driving.` | No force in setup/menus |
| Normal suspension | `FFB inactive — paused` / `game controls the camera` | Zero/stop output; keep preference |
| Active | `FFB active` | Only when the drive/ownership gates and latest output delivery permit it |
| Output error | `FFB unavailable — wheel could not be opened. Retry connection.` | Stop output; show details on demand |
| Stopped by player | `FFB stopped — choose Resume FFB when ready.` | Latched stop, never auto-resume after panic |

**F8** and **Stop FFB** immediately stop feedback and latch that stop. Keep a
visible stop action on every page. Normal pause/focus/scene suspension may resume
with a ramp when driving returns; panic requires deliberate **Resume FFB**.
Reconnection to the same saved wheel follows the consumer's tested recovery
policy; it never transfers output to another wheel. Changing the FFB device
stops output before selection/opening.

A development build may require **Start FFB for this session** as an additional
step while its output is experimental. Explain that once beside the control and
show why output is inactive. Do not describe this as a universal release behavior
or persist development arming. Stable, attended-validated builds can honor a saved
On preference through normal driving gates without repeated consent dialogs.
Use a modest first-use tune appropriate to the game; never overwrite an existing
owner's tune to harmonize defaults. Do not apply an unattended test force.

Diagnostics distinguish **calculated force**, **output call accepted**, and
**physical feel confirmed by the player**. A preview graph must never imply the
wheel received it. These distinctions come from the
[shared diagnostic APIs](CONSUMER-EXPERIENCE.md).

## 6. Cameras extend the game's own cycle — UX-06

The normal **Change camera** action cycles the enabled stock views in their
existing order, then **Bonnet**, then **Bumper**, then wraps to the stock cycle.
Disabled mod views are skipped. Keyboard, gamepad and wheel use that same action.
Do not require a separate bonnet key or a settings toggle on every drive. Keep
mod-only view indices out of the game's persistent stock-camera enumeration.

`Bonnet: Off/On` and `Bumper: Off/On` mean **include in the camera cycle**; say so
under the controls. Adjust one clearly selected view at a time: Height, Forward,
Pitch and Field of view where supported, with units and **Reset this view**.
Optional shortcuts supplement these controls; they are bindable and discoverable
here. **Look behind** is a held action; release restores the previous view and
focus loss clears it.

When the game takes camera/player control for an intro, replay, finish, photo
mode or another scripted sequence, release the mounted view **and stop FFB**.
Do not overwrite the new game-owned pose on the way out. An ordinary player
camera change stays within the same cycle. Resume a mounted view/FFB only when
the consumer has positively identified normal player control again; do not fight
another camera mod's writer. If ownership cannot be established, show the inactive
reason and leave the game in control. Preserve normal scoring and finish flow.

## 7. Telemetry and support are approachable — UX-07

**Telemetry: Off/On** controls the dashboard stream. Show the actual receiver
profile (for example Forza Horizon 5), host and port, with a short matching setup
instruction. Keep protocol layout and detailed channel diagnostics in Advanced.
One-way UDP can establish **Sending**, not **Dashboard connected**, without a
receiver acknowledgement. Distinguish sampled data, socket sends and a receiver
or physical shaker that was actually tested.

Place **Start recording**, **Stop recording**, **Mark moment** and **Open folder**
in a Recording section. Show elapsed time, size/limit, dropped samples and final
saved path. Recording is opt-in and bounded; hitting a limit or a write error is
visible. Do not silently discard an unsaved recording at exit. Preserve useful
metadata: game/mod/toolkit versions, settings, bindings, signal availability and
provenance, sample phase/timing, force calculation and delivery results.

Missing data is **Unavailable**, distinct from a measured zero. Label derived
or estimated channels. Field count is not a quality claim. Each consumer states
which channels were actually checked and how. Camera ownership and the reason
FFB is inactive belong in diagnostic captures as well as live status.

**Create support file** prepares a local, reviewable artifact with versions,
device/selection status, settings, recent errors and capture metadata. Explain
that it may include paths and device identifiers; never upload it automatically.
Keep contact/support instructions in Help with symptom-first recovery steps.

## 8. Save and reset rules — UX-08

Ordinary settings save automatically after a short debounce and on panel close;
show **Saved** or an actionable write error. Calibration and destructive resets
have explicit scoped actions. Network destination edits apply together through
**Apply connection** so a half-edited host/port cannot take effect. Keep the last
working configuration on failure and show both the requested and active value
when they differ.

Opening or closing a page must not change a setting. Changing a slider must not
re-enumerate devices, reconnect FFB or write a file on every render event. Reset
FFB settings leaves controls/cameras/telemetry intact. Reset all settings names
what it affects, preserves a backup and requires a deliberate confirmation.
Neither updating the mod nor a failed calibration deletes personal bindings.

## Adoption and exceptions

Each consumer records `UX baseline: UX-1` plus a dated checklist, exact source
revision/working-tree scope, built/installed versions and evidence. An audit can
identify a source defect without claiming it was seen live. Mark visual and
physical checks **Not tested** until performed; do not turn documentation targets
into release claims.

For an exception, record the rule ID, concrete game/loader limitation, player
impact, interim wording/route, and the acceptance check or follow-up that resolves
it. Different engines are a reason to adapt implementation, not a blanket reason
for different names and workflows. Review new UI strings and navigation changes
against this guide. Fix repeated cross-consumer gaps in shared capabilities where
possible, while leaving game content in its owning repo.
