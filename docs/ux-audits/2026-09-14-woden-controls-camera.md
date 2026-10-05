# Woden binding and camera UX reconciliation audit

2026-09-14, source/documentation review of the active 0.2.3 working tree at
`E:\Source\woden-rally-edge-wheel`. Git HEAD is `7fcbbbd`; most 0.2.3 code is
uncommitted. [Source hashes](2026-09-14-woden-source-manifest.json) identify the
files reviewed. The install receipt identifies 0.2.3, installed at 2026-09-15
03:20 UTC (September 14 local). This audit does not establish that every current
working-tree edit matches that binary. No consumer files or installation were
changed, and no game/input/force/visual test was performed.

**Concurrent work observed before publication:** the Woden session changed
Panel, WheelInput, MountedCamera and Settings while this audit was being written.
The newer source places Handbrake with the other axes, starts button capture
directly, and adds camera shortcut/rebinding code. WUX-02/03 below describe the
initial hashed snapshot and are acceptance targets for that new work, not claims
that it is still absent. The exclusive HandbrakeUsesAxis calculation and Next
wheel picker remain in the follow-up source inspected. No completed build,
deployment or visual acceptance of these concurrent changes was verified here.
The source manifest includes a separate follow-up observation rather than
overwriting the initial evidence.

Rules: UX-1, published baseline `341fc3a`, the pending local owner clarifications
for saved FFB On/Off and agent-managed recording, and the
[binding/camera interaction detail](../CONSUMER-CONTROLS-CAMERAS.md) added here.
Existing violations and newly clarified requirements are distinguished below.
Source paths/line numbers are relative to Woden and refer to the recorded snapshot.

## Conclusion

Both causes contribute. Woden has **partial adoption with acknowledged gaps**,
not a completed reconciliation. `docs/UX-ADOPTION.md:3–17` explicitly disclaims
visual/physical acceptance and leaves the FFB picker and first-use walkthrough
open. The reviewed notes do not audit the actual e-brake click sequence or camera
adjustment bindings. `docs/VERIFICATION.md:15` also says there is no live 0.2.3
UI acceptance. The recorded 19 suites / 441 assertions do not close those gaps.

The previous standard specified analog binding/calibration and additive button
support, but described camera shortcuts as optional and never defined numpad
defaults or their complete rebind flow. A feature checklist and common tab names
left too much room for differing interactions. The new interaction detail closes
that guidance gap; it does not retroactively claim these consumer fixes exist.

## Findings and required reconciliation

### WUX-01 — P1 — Switching input type commits before calibration succeeds

`src/WodenRallyEdge.Plugin/Panel.cs:262–265` sets and saves HandbrakeUsesAxis
immediately when Button/Axis is chosen. `src/WodenRallyEdge.Core/HandbrakeInput.cs:5`
returns the axis amount in axis mode and ignores the saved mod button; if the
axis is unavailable it returns zero unless a stock game button is held.

Starting with a working mod handbrake button, choose Axis then cancel the new
calibration: the old binding can still be stored but its contribution is no
longer active. This conflicts with UX-04-H's existing additive-binding and
cancel-preserves-working-input requirements. It is a source-established behavior,
not a newly reproduced drive failure.

**Fix:** independent Handbrake (axis)/(button) assignments without a global
exclusive mode. Commit new axis identity/calibration only on Save calibration;
combine the valid axis and held button as specified by UX-04. Retain current
data during migration. **Acceptance:** cancel/timeout/failure retains effective
button input; axis and button both work after a successful save and disconnect
of one input does not cancel another held binding.

### WUX-02 — P2 — Handbrake Bind is navigation and introduces another workflow

`Panel.cs:249` special-cases Handbrake's Bind to open a separate page. The player
then chooses Button/Axis and another `Bind button` or `Bind / calibrate axis`
at `:270` / `:279`. Other button rows immediately begin capture. The ordinary
axis page lists only Steer/Throttle/Brake (`:211`) and uses Calibrate as its entry
even when unbound; handbrake uses another label/placement. `Invert` is an action
without the explicit state required by UX-02 (`:222`, `:286`).

**Fix:** consistent binding rows and a direct capture action. Calibration uses
the same provisional preview, Save calibration/Cancel and normalized display as
the pedals. A grouping tab may remain, but a Bind button must bind the named row.
**Acceptance:** the walkthrough in the new interaction detail needs no input-type
choice or second Bind click and no interpretation of raw device ranges.

### WUX-03 — P1 — Live numpad tuning and camera adjustment bindings are absent

`Panel.cs:332–345` offers Bonnet/Bumper sliders and Reset this view. Camera
settings are consumed by `MountedCamera.cs`; `WheelInput.cs:8` lists camera cycle
and rear view but no adjustment actions. No numpad/keypad adjustment route or
camera adjustment binding table was found in the reviewed plugin/core source.

**Guidance gap:** the old UX-06 text called shortcuts optional. Their absence
cannot fairly be described as violating a previously mandatory numpad mapping.
The user's clarified expectation is now explicit in UX-06-K.

**Fix:** active-mount numpad tuning plus F6 → Cameras → Adjustment bindings,
using the same values as the sliders and independently saved Bonnet/Bumper
poses. Bind/Clear/Cancel/conflicts/defaults must work without a physical numpad.
**Acceptance:** default direction checks, rebinding, restart, reset scope,
capture-key release and focus/game-camera suppression pass on the exact build.

### WUX-04 — P2 — FFB selection still uses Next wheel

`Panel.cs:311–318` displays `Output` and cycles GUIDs through Next wheel. The
adoption notes already acknowledge that the steering-derived dropdown is not
implemented. This departs from an existing explicit UX-05-D requirement.

**Fix:** FFB device dropdown with Use steering wheel as default and a saved
explicit override. **Acceptance:** directly select a named device, rebind
Steering, reorder/unplug devices, and verify follow/override/missing behavior
without applying force. Do not regress the newer no-reconnect-loop fix.

### WUX-05 — P2 — Acceptance and exception tracking are incomplete

The adoption document is a useful remaining-work list, but it does not cover
the e-brake mode/cancel behavior or the missing camera shortcut/rebinding flow.
The actual renderer handles mouse-down buttons/sliders (`Panel.cs:103–133`);
keyboard navigation and 720p/high-DPI acceptance remain open in its own notes.
Neither an updated label nor a compiled feature closes an interaction finding.

**Fix:** use the full checklist with rule IDs, expected click/key sequence,
source fix, exact built/installed version, and separate fixture/visual/attended
results. Show specific game limitations as exceptions, and unfinished work as
gaps. **Acceptance:** the handbrake and camera walks each have recorded results
and screenshots tied to the candidate package. Do not claim full UX-1 adoption
while these expected workflows remain unimplemented or untested.

## Guidance/process corrections in this toolkit update

- Added explicit binding rows, cancel semantics, numpad defaults, camera rebind
  flow and required interaction walks. These are implementation requirements,
  not a shared renderer that changes the consumer automatically.
- Reconciled pending local owner guidance for persistent FFB On/Off and
  agent-managed recording with the published docs. Its existence only in a local
  checkout meant a session reading GitHub could read different instructions with
  the same revision date. Future audits must record a guidance commit/hash too.
- Retained those owner preferences. Removing player recording buttons is not
  treated as a regression for the requested agent-managed workflow, and the old
  per-session arming instruction must not be reintroduced.
- Noted the existing sibling 1/3 pitch-direction disagreement in the new camera
  defaults. Preserve saved mappings rather than silently remapping either game.

The consuming session should use WUX-01/02/03/04 as the acceptance targets for
its fixes, including the concurrent work above, then run WUX-05's complete
walkthrough. This audit is handed over through documentation;
it did not edit or send commands to the active Woden session.
