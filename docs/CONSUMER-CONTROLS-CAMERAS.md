# Binding and camera adjustment workflows

Normative UX-1 interaction detail, revised 2026-09-16 (initially 2026-09-14). Use with
[the standard](CONSUMER-UX.md) and [acceptance checklist](CONSUMER-UX-CHECKLIST.md).
These workflows specify what the player does and sees. Having the feature in
code, or documenting a gap, does not establish that the workflow is complete.

Under [Simple/Advanced placement (UX-01-S)](CONSUMER-SETTINGS-VIEWS.md), ordinary
binding and full axis calibration stay available in Simple. Camera **Adjustment
bindings** is also available on demand in Simple, including on keyboards without
a numpad. Detailed pose sliders, response shaping and threshold tuning belong
in Advanced. View selection never changes input/camera behavior or saved values.

## One binding pattern, including the e-brake — UX-04-H

Use the same row pattern for Steering, Throttle, Brake and Handbrake (axis):

| Row content | Action |
|---|---|
| Action name; friendly device/axis or Not bound; live normalized value | **Bind**, **Calibrate**, **Clear** in consistent positions |
| Invert handbrake: Off/On; Deadzone: percent | Explicit state/value controls |

An unbound row shows Bind; Calibrate is available for an existing assignment.
The player does not first choose a global Button/Axis input mode. The Handbrake
axis and Handbrake button are independent, additive bindings. Both remain usable.
A section/tab can organize the rows, but **Bind starts capture for that row**;
it does not navigate to another page with another Bind button.

The handbrake axis flow is:

1. Open **F6 → Controls**, find **Handbrake (axis)** and choose **Bind**.
2. Show `Release the handbrake, then pull it fully and release.` Capture the
   device/axis and endpoints provisionally. Show the detected friendly device
   and a 0–100% preview, with **Save calibration** and **Cancel** together.
3. Save calibration commits identity and calibration together. Before that,
   the previous saved binding remains intact; after Cancel/timeout, it still
   works when gameplay resumes. Entering a setup page or choosing a hardware
   type must not disable the old button/axis assignment or change its mode.
4. Show 0% released, intermediate pull, and 100% fully pulled. Recalibrate uses
   the same operation. **Invert handbrake: Off/On** displays its state; an Invert
   action with no visible resulting state is insufficient.
5. If wanted, bind **Handbrake (button)** using the ordinary button capture.
   That row goes directly to `Press a button`, not to a handbrake mode selector.

Use the UX-04 analog/digital game-route rules. A digital-only game still accepts
the analog hardware axis, with labeled activation/release points and separate
device/game state. Preserve stock and mod button contributions. Do not make a
saved mod button inactive just because an axis exists. Partial input, simultaneous
bindings, disconnect, focus loss, failed/cancelled calibration and persistence
are acceptance cases, not just feature-list entries.

## Live camera adjustment on the numpad — UX-06-K

Mounted Bonnet and Bumper views provide **live numpad adjustment by default**.
F6 sliders supplement the shortcuts; sliders alone do not fulfill this workflow.
The same keys adjust whichever supported mounted view is currently active.
They do not switch the game's camera or change the inactive mount's settings.

The family defaults are:

| Action | Default |
|---|---|
| Move up / Move down | Numpad 8 / Numpad 2 |
| Move forward / Move back | Numpad 9 / Numpad 7 |
| Move left / Move right | Numpad 4 / Numpad 6 |
| Tilt down / Tilt up | Numpad 1 / Numpad 3 |
| Widen field of view / Narrow field of view | Numpad + / Numpad − |
| Reset this view | Numpad 0 |

Directions describe the visible result, independent of a game's numeric pitch
sign. Use small predictable tap increments; holding an adjustment repeats at a
bounded rate independent of frame rate. Reset acts once per press. Show the
active mount and changed setting/value briefly, and save with the usual deferred
Saved/error behavior. Avoid file writes every rendered frame.

This mapping follows Art of Sim Rally's existing defaults. The inspected iRacing
source uses the same movement/FOV/reset defaults but the opposite 1/3 tilt pair.
That difference is a migration item; **preserve existing saved bindings**, show
the actual current assignments, and change them only through the player's
deliberate rebind or Restore numpad defaults. Do not silently reinterpret saved
key names or pretend the projects already agree. [art-of-sim-rally and
iracing-arcade-wheel source inspection, 2026-09-14]

An unavailable adjustment needs a concrete game limitation and visible
explanation. Not having written the hook yet is an implementation gap. Never
offer an active shortcut that does nothing. Preserve the native Change camera
cycle, mounted-view ownership and camera-takeover FFB rules in UX-06.

## Rebind camera adjustments inside F6

**F6 → Cameras → Adjustment bindings** lists every adjustment above, its actual
current assignment, **Bind** and **Clear**. Include **Restore numpad defaults**,
scoped to these shortcut bindings. It does not reset camera positions, steering,
pedals, FFB or the game's Change camera binding. If Controls also links to camera
adjustments, both entrances edit the same stored bindings.

The required flow is: choose an adjustment's Bind, press a new key, see the
updated readable assignment, close F6, and use that key on the active mounted
view. A keyboard without a numpad must support the full workflow without config
editing. Support physical wheel buttons through the normal button-binding route
where that route exists; record keyboard-only limitations explicitly.

Capture supports Cancel/Esc, Clear, timeout and context-aware conflict feedback.
Keep the prior assignment on cancellation or rejected input. Protect Settings,
Stop FFB and other reserved actions. Conflicting camera shortcuts must not run
together silently. Reject unsupported chords with a clear message rather than
quietly storing only one key. Help shows saved assignments, never a hardcoded
numpad legend after a rebind.

Apply those checks to **Default** and **Restore numpad defaults** as well as
Bind. Preflight the entire proposed reset against the current game, mod and
reserved assignments before changing any key. A conflicting batch leaves every
existing assignment intact and explains which action needs resolving. For example,
after moving Camera Up off numpad8 and assigning Settings or Handbrake to numpad8,
restoring camera defaults must not silently create a duplicate. Cover both the
individual and batch paths in consumer interaction fixtures.

Persist a binding/calibration proposal before replacing its effective assignment.
If the save fails, keep the last working assignment and a visible Retry/Cancel
route to the proposal. The same transaction rule applies to Clear and reset;
an error label alone does not make a discarded proposal recoverable.

Do not tune/reset while settings, capture or text input is active, after focus
loss, or while the game/another camera writer owns the view. Wait for captured
or held keys/buttons to be released before re-enabling shortcuts. Normal camera
switching retains each mount's saved adjustment; Reset this view affects only
the active mount. Keyboard shortcuts must not force the game out of a scripted
camera or resume stopped FFB.

## Required reconciliation evidence

A consumer review includes these exact walks on the packaged build:

- Bind and cancel an e-brake axis starting with a working button; confirm the
  button still works. Save axis calibration, then use both bindings independently
  and together. Verify rest/partial/full pull, inversion and unplug/reconnect.
- Enter Bonnet, use each default numpad adjustment, then Bumper; prove each view
  retains its own values. Verify directions by their visible results and reset
  scope, including the 1/3 tilt pair.
- Rebind at least one movement, tilt and FOV adjustment on a keyboard without a
  numpad. Exercise cancel, conflict, Clear, defaults and game restart. Ensure the
  capture key never immediately moves/resets a view on panel close.
- Inspect the entire binding/adjustment UI at 720p and high DPI, then test panel,
  focus, stock-view and game-camera takeover suppression. Associate screenshots
  and results with the exact package. Code/fixture tests are complementary.

Mark each step Pass, Gap, Partial or Not tested. A reconciliation is complete
only when the expected workflows pass or a specific game limitation has been
recorded. Matching tab names, updated prose and passing non-UI tests are not
enough. The [Woden source audit](ux-audits/2026-09-14-woden-controls-camera.md)
illustrates the distinction without changing that active consumer project.
