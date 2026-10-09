# Shared player experience for wheel mods

Baseline **UX-1**, clarified 2026-09-23 (Simple/Advanced revision 2026-09-16;
initially 2026-09-13). This is the product standard for consuming mods,
written in response to the owner's reports that Art of Sim Rally, iRacing
Arcade Wheel and Woden Rally Edge Wheel were becoming harder to use consistently.
It specifies the intended experience; it does **not** certify existing releases.
The toolkit does not yet provide a shared UI or calibration library.

The subsequent [RC14 owner-test findings](ux-audits/2026-09-22-art-rc14-commands-handoff.md)
extend compact command sizing across the panel and define **UX-03-H**, a scoped
release barrier for programmatic transitions from the overlay to native menus.

The September 22 [rendered Art feedback](ux-audits/2026-09-22-art-rendered-hierarchy.md)
requires perceptible group boundaries, distinct compact navigation and grouped
commands. Setup is now optional: retain it only for a distinct guided first-use
workflow, with explicit migration of any removed page's saved identity.

The September 21 [Art visual hierarchy review](ux-audits/2026-09-21-art-visual-hierarchy.md)
adds **UX-03-G**: visibly group each binding and require useful content in the
initial viewport. A short option list alone does not make Simple understandable.

The September 19 rollout clarifies actual host-content bounds, direct stock-input
consumers and pointer cancellation, and the distinction between installed feature
support and a connected, configured player setup. See the
[deployment evidence](ux-audits/2026-09-19-deployment.md) for discovered gaps and
the verification limits of each consumer; written guidance is not adoption proof.

The 2026-09-16 revision adds [Simple and Advanced settings](CONSUMER-SETTINGS-VIEWS.md)
(UX-01-S): Simple by default, complete basic setup there, and Advanced for detailed
tuning/diagnostics. Both views share the same values; changing views never resets
an owner's tune. Each consumer needs a settings inventory and interaction checks.

The 2026-09-14 clarification makes analog handbrake hardware support and the
steering-bound default FFB dropdown explicit requirements (UX-04/05). Older
consumer audits have not automatically been checked against this revision.
The [binding and camera workflow detail](CONSUMER-CONTROLS-CAMERAS.md) also
specifies direct binding/cancellation, default numpad adjustments and camera
shortcut rebinding (UX-04-H/06-K). Record the guidance commit/hash, not only
the revision date, when reviewing a consumer.

The first [iRacing Arcade / Art of Sim Rally audit](ux-audits/2026-09-13-consumers.md)
records source-backed departures and a prioritized adoption plan.
The [Woden interaction audit](ux-audits/2026-09-14-woden-controls-camera.md)
distinguishes missing implementation from previously underspecified workflows.

Use this with the [install and first-drive guide](CONSUMER-SETUP.md),
[acceptance checklist](CONSUMER-UX-CHECKLIST.md) and
[technical integration guidance](CONSUMER-EXPERIENCE.md). Game hooks, drawing,
preferences and game-specific capabilities remain in each consuming repository.

## The promise to the player

Someone who has set up one of our mods should recognize the next one: install,
open **F6**, check the wheel and pedals in Simple, bind buttons, check feedback, drive.
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

Keep **View: [Simple] [Advanced]** in the header on every page. Default to Simple
when there is no saved view preference; remember an explicit choice. The
[view placement contract](CONSUMER-SETTINGS-VIEWS.md) defines the initial controls,
on-demand basic setup and Advanced additions. View choice changes presentation
only; hidden settings remain effective. Essential bindings, recovery and Stop FFB
stay reachable in Simple. Do not add separate per-page expert-mode toggles.

Use these pages in this order, omitting Setup when it adds no distinct guided
workflow. Controls, FFB, Cameras, Telemetry and Help are the five core pages.
The table summarizes page ownership; the linked
view contract determines which details appear initially:

| Page | What belongs here | First things visible |
|---|---|---|
| **Setup (optional)** | Distinct guided first-use workflow | The current incomplete step, its check and next action; omit a duplicate overview |
| **Controls** | All supported bindings and calibration | Steering, Throttle, Brake, Handbrake; then Driving buttons, Menu buttons, Shifter |
| **FFB** | Wheel selection, strength, tuning and output status | FFB Off/On, FFB device dropdown defaulting to the bound steering wheel, Steering strength, independent effect strengths (STD-027), current status, Stop FFB |
| **Cameras** | Cycle membership and view adjustment | Bonnet and Bumper Off/On, Change camera binding, live-adjustment hint; Adjustment bindings on demand |
| **Telemetry** | Dashboard connection and session recording | Telemetry Off/On, receiver preset/destination, sending facts; status/Stop if recording |
| **Help** | Version, troubleshooting, support evidence and reset | Version, short symptom-based help, UI scale, Create support file; device details in Advanced |

Hide any core page the mod has nothing to offer on in this game (STD-017): for example, no
Cameras tab when the game has its own bonnet/bumper views and the mod adds no camera setting.
Help always stays. Visibility follows what the build supports, not momentary state.

Remove Setup/Overview pages that merely repeat Controls/FFB values and links.
Keep Setup only when it guides the player through distinct, state-aware setup
steps; it uses the same bindings and values as the regular pages. First use opens
that workflow when present, otherwise Controls. Preserve saved page meanings
when removing or reordering navigation: see [page migration](CONSUMER-SETTINGS-VIEWS.md#defaults-and-migration).
Do not silently turn a saved FFB page into Cameras by shifting numeric indices.
Keep raw action maps, HID reports, native device indices and library versions in
advanced details. Recording is a section of Telemetry, not a competing top-level
page. Refresh is beside the relevant device picker, with full inventory in Help.

If a game has handling/transmission modifications, put them in an optional
**Driving** page between Cameras and Telemetry. Preserve the five core page names
and relative order. Show experimental handling/assist tuning in Advanced; only
ordinary-driving essentials such as a required transmission selector belong in
Simple. Explain the actual mode restrictions at the controls. Do not
make a gearbox experiment the first thing a wheel user must configure.

Unsupported core pages stay recognizable with a brief capability message and help;
omit unsupported controls within them. An unsupported feature must not appear
enabled, silently do nothing, or offer invented telemetry as a substitute.

## 2. Consistent wording and controls — UX-02

Use sentence case and familiar player language. Labels name the setting; controls
show its value. Buttons name an action. State is visible in text, not color alone.

| Purpose | Standard wording / control | Avoid |
|---|---|---|
| Settings view | `View: [Simple] [Advanced]` | `Expert mode`, `Debug UI`, multiple competing Advanced switches |
| Boolean setting | `FFB: [Off] [On]` | `ON Allow FFB`, `Enable feedback?`, a button whose text could mean current or next state |
| Boolean shaping | `Invert steering: [Off] [On]` | `Flip`, unlabeled checkboxes |
| Numeric setting | `Strength: 20%` with slider, readable value and default reset | A unitless number, a reference constant presented as everyday strength |
| Action assignment | `Change camera: MOZA R12 · Button 34` with **Bind**, **Clear** | `listen`, `capture path`, a GUID in the main row |
| FFB selection | `FFB device: [Use steering wheel — MOZA R12 v]` with a dropdown | A standalone **Next device** button, raw index, or silent first-device fallback |
| Empty / missing binding | `Not bound` / `Saved device disconnected` | Blank values or replacing the saved device automatically |
| Calibration | **Calibrate**, **Save calibration**, **Cancel** | **Apply** without saying what changes |
| Reset | **Reset FFB settings** / **Restore default binding** | An ambiguous global **Reset** next to a single row |
| Successful persistence | `Saved` | A Save button that is unnecessary or leaves changes silently pending |
| Unsupported | `Unavailable in this game: tyre temperature is not exposed.` | Plausible zero readings or permanently disabled unexplained widgets |
| Installed capability | `Controls support: Available` | `Ready to drive` when only the binary's capabilities were checked |

Keep capability, saved setup and current device readiness distinct. An installed
calibration feature does not mean a wheel is connected or its axes are bound.
Use **Choose your steering wheel**, **Not bound**, or **Saved device disconnected**
for the next required action. Reserve **Ready to drive** for the consumer's checked
required controls and device availability; optional hardware must not block it.
If availability has not been checked, say **Device saved**, not **Connected**.
These statuses do not claim attended driving or physical force validation.

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

Use **Handbrake (axis)** and **Handbrake (button)** where the two rows need
disambiguation. Analog e-brake hardware support is required as described in UX-04;
a digital-only game action does not justify omitting the hardware axis binding.
Show button numbers starting at 1; translate zero-based backend values only at
the UI boundary and retain raw values in support details. Show a friendly device
name and readable axis name (for example `X axis`) before any technical path.

Every advanced setting gets one sentence explaining what the player will feel
or see, its unit, and a way to restore its default. Example:
`Smoothing: 35 ms — Higher values soften rapid changes but delay feedback.`
Keep reference loads, slip scaling and force components in the global **Advanced**
view, with basic controls retained above them.
Game tunes can differ; the label, unit and meaning of Strength must not.

## 3. Shared visual and interaction rules — UX-03

Use an opaque dark neutral panel with one accent color for selection. Reserve
warning/error colors for actual conditions. Keep a stable title/navigation area,
a scrollable page body, and visible Close and Stop FFB actions. Changing pages
resets that page's scroll sensibly and never moves the navigation off screen.

Navigation must look different from commands. Prefer flat page tabs with a clear
selected indicator, such as an accent underline, and a separate keyboard-focus
indicator. Keep tabs compact enough for one row in the default host when readable
labels and hit targets permit; wrap at narrow/custom scales instead of shrinking
text or clipping. The View selector and Off/On segments communicate selection.
Reserve raised/button styling for commands such as Bind, Save, Close and Stop FFB;
use flat disclosures for Show/Hide, and plain text for status. A section expander
must not compete visually with a command just because both accept a click.

**Command sizing — UX-03-C.** Ordinary commands use compact content-sized or bounded fixed-width buttons,
usually right-aligned beside their explanation. In a wide panel, a full-width
raised command reads as a section divider; this applies to support, reset,
preset and native-menu commands as well as binding actions. Reserve full-row
surface treatments for actual groups or navigation, not ordinary commands.
Use compact segmented controls for short mutually exclusive values and flat
links/disclosures for **Review in Advanced**, **Details**, and **Show/Hide**.
Narrow layouts may stack and expand commands only when reachability requires it;
keep the target clear and do not use expansion as the wide-layout default.

The reference layout uses an 8-unit spacing grid, 16-unit page padding and
32-unit minimum row/button height at 100% scale. Body text starts at 16 logical
pixels, secondary help at 14, headings at 20. These are design targets, not raw
Unity pixels: scale all text, hit targets and spacing together. Offer UI scale
or honor the host's scale setting. Never fix help text to a tiny font that ignores
the player's chosen scale. Adapt to 1280×720 and verify a readable 4K rendering.
Measure the actual settings content rectangle, including a loader's smaller host
window. A 4K screen does not imply a 4K panel. Wrap navigation and label/action
rows within that rectangle; keep Close and Stop FFB reachable at the default and
explicit scales. Do not enlarge or overwrite the owner's loader window settings
to make an overflowing layout appear to pass.

Align labels and values in consistent columns. Put **Bind** / **Clear** at the
same position on each binding row. Long device names wrap or show their full
value on focus; they must not hide action buttons. Use section headings and short
help text instead of long paragraphs between every control.

### Binding ownership and useful space — UX-03-G

Each repeated binding is one visibly bounded group: **action name → current
assignment / live value → actions → active editor**. Use a perceptibly separate
surface or visible border/separator **plus inter-group spacing**. If the fill
contrast is too weak to distinguish adjacent cards, add a visible boundary;
merely drawing a box in code does not establish grouping. Verify separation in
the real host at normal viewing size, including disabled controls and partial
scrolling. Proximity alone is insufficient when rows
wrap or scroll. A compact table row can satisfy this just as a card can. Keep
the action name stronger than its secondary device/calibration detail. The
container encloses Bind, Calibrate, Clear and any provisional Save/Cancel.
One camera adjustment group contains both its keyboard and USB assignments.

Within a binding group, keep its short instruction left-aligned and compact
command buttons grouped on the right when space permits. Avoid stretching each
Bind/Calibrate/Clear across the page: repeated full-width buttons add noise and
weaken the hierarchy. At narrow widths, stack inside the same group, keep usable
hit targets and repeat the action's target name. Help must wrap without pushing
commands outside their container or hiding a required precondition.

Use more space between groups than within them (target 16 versus 8 logical
units, scaled with the content). Include container padding when deciding
whether actions fit side by side. In stacked/wrapped layouts, repeat the target
where necessary: **Bind steering**, **Calibrate steering**, **Clear steering**.
Scrolling past part of a group must not make its buttons look owned by the
next heading. Preserve full friendly names and disconnected/error messages.

Keep routine save status compact, beside existing header controls where it fits.
Put capture instructions beside the named control being edited. Long permanent
instructions and raw calibration summaries must not displace the basic actions;
show instructions during the operation and detailed summaries in Advanced.
Essential preconditions must be visible before capture starts: for example,
centre the wheel or release the pedal/button before clicking Bind when that
click records the resting state. A later instruction cannot repair that sample.
Do not hide errors, calibration controls, Save/Cancel or recovery to gain space.
Use **Show shifter bindings** / **Hide shifter bindings** or a recognizable
disclosure arrow for optional groups; expansion is navigation, not an Off/On
setting. Keep an active editor visible until it is saved or cancelled.

At the actual default host size and readable scale, Controls must initially
show at least one **complete actionable binding group**, including its buttons.
When retained, Setup must show its current guided step, next action and persistent controls without
scrolling. A screenshot showing only page headings is not acceptance evidence.
Check long names, partial scrolling, active edits, errors and narrow/custom
scales too. Reducing header content must reclaim space in the page layout;
do not leave the old fixed-height budget unchanged. Document host constraints
and pending rendered checks explicitly. Source or browser fixtures cannot
certify a game's actual renderer.

Support mouse, keyboard focus and keyboard activation; wheel/pad navigation is
the target once menu bindings are available. Show the focused control. If a
release requires a mouse, say so in setup and record the gap in its UX checklist.
Opening the panel suppresses driving input, camera shortcuts and FFB. Pause the
game only when allowed and only resume a pause the panel itself introduced.
Restore cursor/focus and release held inputs on close, focus loss and cancellation.
The stock menu must not also receive pointer, navigation or submit events while
the mod panel owns input. Test the real stock dispatch route, including consumers
that retain action references rather than calling patched input getters. On
ordinary close, prevent the opening/closing gesture and genuinely held navigation,
submit/cancel and Settings controls from replaying into the underlying menu.
A held D-pad must not move it merely because the overlay closed. Identify the
actual gestures; do not infer them from every non-zero mapped game axis/button.
Verify the aggregation, release policy and actual open/close interaction.
Programmatic transitions to a native menu use the scoped handoff below, not a
global wait for all mapped controls to become neutral.
Cover direct game shortcut components as well as the UI event dispatcher: a
Welcome screen may invoke a button directly from its own Update method. Cancel
retained pointer presses/drags before handing control back so closing the panel
cannot replay a queued click/drop. Preserve native selection and action maps;
disabling the whole input backend is not the isolation mechanism. Include the
opening key edge and first-use screens in the actual interaction check.

### Programmatic native-menu handoff — UX-03-H

A command such as **Open game bindings** deliberately transfers input ownership
from the overlay to the game's own screen. Treat it as an explicit transition:

1. Record and consume the actual initiating mouse button, key or physical control.
   Keep the pending transition guarded until that initiating control is released.
2. After release, require a short neutral-frame barrier before completing the
   transition. Neutral refers to the initiating gesture, not the full game's
   mapped input state. Document and test the consumer's frame count; a timer
   alone must not bypass a still-held initiating control.
3. Cancel retained pointer presses/drags and queued activation from the overlay.
   Once the release/barrier and destination/focus checks pass, explicitly release
   overlay ownership and its pending-close/global input block so the destination
   screen can receive fresh input. Preserve the intended destination focus.
4. Preserve cancellation and focus-loss guards. A cancelled or invalid transition
   must not later open the destination unexpectedly or leave a stale input lock.

**Do not wait on all mapped game UI axes/buttons.** Wheel or pedal maps can rest
non-zero, and an H-pattern shifter button may remain held indefinitely. Those
unrelated states must not keep a programmatically opened native menu blocked.
This scoped transition does not permit the initiating click/submit to leak into
the destination, disable the game's input backend or weaken cancellation guards.

Verify both outcomes: a held initiating mouse/key/control cannot activate the
destination, and after its release plus the barrier the native screen works even
with an unrelated pedal axis non-zero or shifter gear held. Check fresh input,
return navigation, cancellation and focus loss; retain source/fixture and actual
game interaction evidence separately.

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
This applies equally when several roles share the primary steering device and
when a former pedal device becomes the primary device. Failed reads must remove
that device's current, previous-frame and reported effective contributions;
cached samples and unresolved numeric device slots must not feed another binding.

### Analog handbrake hardware is part of the controls baseline

Follow the exact [binding workflow](CONSUMER-CONTROLS-CAMERAS.md): Bind starts
capture, the old assignment remains effective after cancellation, and axis and
button bindings coexist. Do not add a global Button/Axis choice that disables
one binding or commits a new input mode before calibration has been saved.

For games with a handbrake action, support an **analog handbrake/e-brake axis**,
including a separate USB device. Keep the player's button binding available too;
owning or binding a handbrake is optional and never blocks ordinary driving.
No in-game handbrake action is a documented game limitation, not an invitation
to invent one through unrequested physics changes.

Provide Bind, Clear, Calibrate, Save calibration/Cancel, released/fully pulled
endpoints, **Invert handbrake: Off/On**, deadzone and a normalized **0–100%** bar.
Persist the exact device identity and calibration independently of steering and
pedals. Preserve the last working calibration on cancel or failed capture.

Where the verified game route supports analog input, preserve the calibrated
0–100% command through to that route. Do not reduce it to a button threshold.
If the game only accepts a held digital handbrake, still accept the physical
axis and expose a labeled fallback: `Game handbrake: On/Off only`. Provide an
**Activation point** and a lower **Release point**, both in percent, so resting
noise does not chatter. Simple calibration shows their effective values; tuning
them is in Advanced with tested defaults for ordinary setup. Show analog
**Device input** separately from digital **Game input**. Record this game
limitation and its verified input seam in the
consumer's UX adoption notes; do not call the resulting game response analog.

Combine additive button and axis bindings predictably: use the greater analog
contribution (a held button contributes 100%), or logical OR for a digital game
action. Preserve stock contributions; do not sum/clamp them into an unexplained
response. Release each contribution when its input is released, disconnected,
or suppressed by menu/focus/player-ownership gates. No stale held handbrake may
survive a transition, and disconnecting one device must not cancel another held
binding. Capture calibrated input and the final analog/digital command separately
when that final route is observable.

## 5. FFB has an understandable state — UX-05

Keep **FFB: Off/On** as the single saved preference, defaulting to On for a new
configuration. Preserve an existing owner's Off choice. Do not require a second
session-start/arming control. On authorizes output only through the normal exact
device, ownership, focus, pause, camera, source-validity and watchdog gates.
[Owner request: woden-rally-edge-wheel, 2026-09-14]

Present **Strength** as a percentage
of the mod's available output; do not imply it is Nm or a guaranteed wheel-base
torque. Show the selected physical wheel and status beside the control.

### FFB device defaults to the wheel bound to Steering

Default **FFB device** to **Use steering wheel**, resolving the physical device
already bound to **Steering**. Show the resolved friendly name in the control,
for example `Use steering wheel — MOZA R12`. The player should not have to choose
the same wheel a second time. A default device choice does not override a saved
FFB Off preference or bypass driving gates.

Use a **dropdown** wherever the renderer can support one, with Use steering
wheel first, available physical FFB devices, and the retained missing selection
when disconnected. Use a visible, directly selectable device list as the fallback
when a dropdown cannot be implemented; document the renderer limitation.
**Next device alone is not an acceptable picker.** Distinguish duplicate friendly
names with a readable identifier suffix; keep full GUIDs in device details.

Device choices are mutually exclusive values, not commands. Keep the current
selector and expanded list bounded; use flat choice rows with a visible selected
marker, not full-width raised buttons. Keep a saved-but-disconnected choice
visible, labeled and disabled/unselectable without clearing its saved identity.
Available alternatives remain selectable. **Refresh** is a separate compact
command beside the picker, not another choice or full-width bar. Apply this
treatment to other device pickers too; check expanded, selected and disconnected
states in the actual host. [Owner feedback: art-of-sim-rally RC16, 2026-09-23]

Persist whether the player follows Steering or chose an explicit FFB override.
Follow mode resolves the saved steering device by stable identity, not by
enumeration index, product-name substring, last activity, or the first available
FFB device. If Steering is unbound, disconnected, ambiguous, virtual, or lacks
FFB, show the reason and leave output inactive; let the player bind Steering or
choose a physical output device explicitly. A separately reported FFB interface
requires a verified identity association or an explicit choice, not a name guess.

A deliberate saved Steering rebind updates the target only in Use steering wheel
mode. An explicit FFB override survives steering recalibration/rebinding,
refresh, restart and USB reorder until the player changes it or chooses Use
steering wheel again. Missing saved devices remain missing. Existing explicit
choices are preserved on upgrade; unverifiable legacy index/name selections
require resolution rather than being silently accepted as the new default.

When the resolved target changes, zero/stop and release output on the old target
before opening the new one through strict identity selection. Selection and
refresh do not apply test torque, clear a saved Off preference, or bypass
driving gates. Input bindings on the old device retain their own lifecycle.

| State | Player-facing example | Behavior |
|---|---|---|
| Disabled | `FFB off` | No output |
| Device absent | `Wheel disconnected — reconnect it, then Refresh.` | Retain selection; no substitution |
| Available | `Ready — feedback starts while driving.` | No force in setup/menus |
| Normal suspension | `FFB inactive — paused` / `game controls the camera` | Zero/stop output; keep preference |
| Active | `FFB active` | Only when the drive/ownership gates and latest output delivery permit it |
| Output error | `FFB unavailable — wheel could not be opened. Retry connection.` | Stop output; show details on demand |
| Stopped by player | `FFB off — choose On when ready.` | F8/Stop saves Off; never auto-resume after panic |

**F8** and **Stop FFB** immediately stop feedback and save Off. Keep a
visible stop action on every page. Normal pause/focus/scene suspension may resume
with a ramp when driving returns; panic requires deliberately choosing **On**.
Reconnection to the same saved wheel follows the consumer's tested recovery
policy; it never transfers output to another wheel. Changing the FFB device
stops output before selection/opening.

Development builds use the same saved On/Off flow. Experimental status belongs
beside the tune and in verification records, not in a repeated session-start step.
Zero/stop transient output without tearing down and enumerating devices on each
pause, contact gap or camera transition. Device open failures must not create an
unbounded reconnect/stutter loop. Keep connection and suspension separately tested.
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
under the controls. In Advanced, adjust one clearly selected view at a time:
Height, Forward, Pitch and Field of view where supported, with units and
**Reset this view**.
Live numpad adjustment and **Cameras → Adjustment bindings** are required for
mounted views, using the [default mapping and rebind workflow](CONSUMER-CONTROLS-CAMERAS.md).
Keep Adjustment bindings available on demand in Simple. Sliders supplement
these shortcuts in Advanced; they do not replace them. Preserve saved
bindings, document concrete unsupported adjustments, and test visible direction,
reset scope and capture-key suppression. **Look behind** is a held action; release restores the previous view and
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
instruction. Simple offers a tested receiver preset with its actual destination;
editable connection fields, protocol layout and detailed channel diagnostics
belong in Advanced. Connection settings (Advanced) links directly to those fields.
One-way UDP can establish **Sending**, not **Dashboard connected**, without a
receiver acknowledgement. Distinguish sampled data, socket sends and a receiver
or physical shaker that was actually tested.

Recording can be managed by an agent/developer launch tool when the owner wants
diagnostics outside the menu. That route must request a bounded capture for one
launch, expire/consume the request, preserve ordinary settings and finalize on
normal exit. Agent launches default to no physical force; an attended diagnostic
run may explicitly allow the saved FFB preference. Do not make the player start
or stop recording in the menu for this workflow. [woden-rally-edge-wheel, 2026-09-14]

If player-operated recording is offered, place **Start recording**, **Stop recording**,
**Mark moment** and **Open folder** in an Advanced Recording section. Simple still
shows active recording status and Stop recording, as well as limit/write failures.
Show elapsed time, size/limit, dropped samples and final
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

Opening or closing a page must not change runtime settings. Switching
Simple/Advanced changes only presentation preferences, following UX-01-S.
Changing a slider must not
re-enumerate devices, reconnect FFB or write a file on every render event. Reset
FFB settings leaves controls/cameras/telemetry intact. Reset all settings names
what it affects, preserves a backup and requires a deliberate confirmation.
Neither updating the mod nor a failed calibration deletes personal bindings.

## Shared panel implementations by engine (STD-030, 2026-10-09)

Use the shared piece for the game's route instead of a new panel; each one draws the UX-1 frame (title, View
Simple/Advanced, Stop FFB when offered, Close, flat tabs in the fixed order, empty pages hidden, Help always) and the
STD-023 frame rate under Cameras (Help when there is no Cameras page).

| Route | Shared piece | Games (2026-10-09) |
| --- | --- | --- |
| Native DLL proxy (dinput8, D3D11 or D3D12, x64 or x86) | `native/panel` (Dear ImGui; DXGI present hook, D3D12 queue capture, F6/F8/Esc input hook, dev channel) and `native/dinput-proxy/dbce_ffb_layer` (FFB Off/On, Strength, effect-gain emulation, the force log) | Rush Rally 3, V-Rally 4, DiRT 4, DiRT Rally 2.0, GRID (2019) (D3D12), DiRT 3 (x86) |
| UE4SS (Unreal with the toolkit triple renderer) | `native/ue4ss-panel`: the same panel as one DLL a Lua mod loads with `package.loadlib`, a per-game `panel.ini`; edits the renderer's `config.ini` | ADRIFT (its own first copy), Sonic CrossWorlds (wiring pending) |
| Electron page | Slow Roads `src/app/dbce-panel.js` (HTML in the same shape, saves through a console line the loader validates) | Slow Roads; promote to the toolkit when a second Electron game appears |
| Unity / .NET mod loaders | each mod's own panel to UX-1 (art of rally, Woden, #DRIVE Rally, iRacing Arcade, Horizon Chase Turbo) | as listed |
| Others (CannonBall SDL/OpenGL, Slipstream FNA, OutRun 2006 D3D9) | none shared yet: Dear ImGui with the matching backend and the `native/panel` frame code is the intended route | |

For every route: the dev channel file (`%LOCALAPPDATA%\dbce\<game>\panel-dev.txt`, `open <Page> [simple|advanced]` or
`close`) is ignored when it was written before the game started, so a file left by an unattended run never opens the
panel for the player; settings are saved when a control is let go, not on every frame of a drag; a choice that applies
at the next start says so.

## Adoption and exceptions

Each consumer records `UX baseline: UX-1, revised 2026-09-16`, the guidance
commit/hash, plus a dated checklist, exact source
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

## Wheel button prompts (nice to have, 2026-10-05 owner)

When the player drives with a wheel, in-game button prompts should show the wheel's buttons,
not gamepad or keyboard glyphs. Games resolve prompts from the active controller's binding; a
wheel usually has no glyph art, so they fall back to raw element names ("Button 12") or a
"missing map" string. Where the game routes prompts through one lookup (DRIVE:
`InputGlyphs.GetJoystickGlyph`), a mod can return its own glyph for wheel-rig devices: a badge
drawn at runtime with the button's label. Labels default to the button number and can be named
by the player on the Controls page (rims differ, so no per-model art is assumed). Keep the
game's glyphs for gamepads and keyboards. Record per game whether prompts are covered.

## Steering weight and effect intensity (STD-027)

Use the shared [strength-control contract](FFB-STRENGTH-CONTROLS.md): steering
weight and crash intensity have separate labelled controls. Reuse an existing
independent steering gain; do not add a duplicate slider or call it overall force.
