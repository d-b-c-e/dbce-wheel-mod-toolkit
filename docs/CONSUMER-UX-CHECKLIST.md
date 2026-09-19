# Consumer UX audit and acceptance checklist

Baseline **UX-1**, revised 2026-09-16. Copy the report template below into the consuming
repo when adopting [the standard](CONSUMER-UX.md) and
[setup contract](CONSUMER-SETUP.md). Keep source review, fixture checks, visual
inspection and attended hardware evidence separate.

Use **Pass**, **Partial**, **Gap**, **Not tested** or **Not applicable** per row.
“Not applicable” requires the actual game limitation and the user-visible
explanation. A documented limitation is still a gap where the feature is expected.
Do not treat an empty checkbox as proof of failure or a source method as proof of
successful runtime behavior.

## Report header

```text
Consumer / game:
Audit date / UX baseline revision: YYYY-MM-DD / UX-1 revised 2026-09-16
Guidance commit/hash and any local owner clarifications:
Source commit and branch:
Working-tree changes included / source hashes where needed:
Built version / installed version / game build:
Evidence inspected: source / package fixture / screenshot / attended drive
Display resolution, scaling and input devices actually tested:
Settings placement inventory and Simple/Advanced migration evidence:
No-touch scope (other active work or installations):
```

## Review matrix

| ID | Acceptance check | Required evidence |
|---|---|---|
| UX-01 | F6, Settings button, Close and Esc share one panel; six core pages have standard names/order; no duplicate binding stores | Source + live open/close/navigation |
| UX-01-S | Simple by default; persistent explicit View choice; complete basic setup and recovery in Simple; Advanced adds tuning; one settings store, preserved effective hidden values/custom summary, no runtime side effects from switching | Per-setting inventory + fresh/legacy configuration checks + all [view walkthroughs](CONSUMER-SETTINGS-VIEWS.md), including Simple first use, errors, capture/edit guards and both views at 720p/high DPI |
| UX-02 | Explicit label/value Off/On controls, common action names, units, readable device labels, scoped reset wording | String/source review + visual inspection |
| UX-03 | Opaque readable layout, stable navigation, unified scale, long names, keyboard focus, no stock pointer/submit/navigation dispatch through the panel; held keyboard/wheel/pad input releases before handoff; clean cursor/pause restoration | 720p and high-DPI/4K views + actual input aggregation and stock-menu open/close interaction |
| UX-04 | Independent wheel/pedal/shifter/handbrake identities; bind/cancel/timeout; centre/endpoints; conflicts; live final-input bars; saved bindings survive disconnect and shared-primary roles become neutral on failed reads | Source + calibration/persistence/shared-device loss tests + attended input |
| UX-04-H | Bind starts capture directly; provisional analog handbrake calibration with visible inversion/deadzone/0–100%; cancel preserves effective old input, independent additive button/axis rows; progressive game route or labeled digital fallback | Exact binding walkthrough + calibration/persistence fixtures + final-input evidence; no exclusive mode saved before successful calibration |
| UX-05 | Clear Off/Ready/Inactive/Active/Error/Stopped states; visible Stop FFB + F8; strict selection, zero during setup, latched panic and ramped normal recovery | Policy/lifecycle tests + attended low-force check |
| UX-05-D | FFB device defaults to the saved Steering device; dropdown or documented directly selectable list; persistent follow mode/explicit override, strict identity, missing/non-FFB handling; no Next-device-only picker | No-output selection/reorder/disconnect fixtures + visual interaction; switching preserves saved Off and normal driving gates |
| UX-06 | Native camera action cycles stock/Bonnet/Bumper; disabled views skip; held Look behind releases; game takeover releases camera and stops FFB | Cycle tests + intro/pause/replay/finish/focus/camera-conflict drive |
| UX-06-K | Default live numpad movement/tilt/FOV/reset of the active mount; F6 camera adjustment Bind/Clear/Cancel/defaults/conflicts and keyboard-without-numpad support; saved mappings/pose scope, capture/focus/camera suppression | Full [camera interaction walk](CONSUMER-CONTROLS-CAMERAS.md) on the actual package, with screenshots and saved/reloaded bindings; record concrete unavailable adjustments |
| UX-07 | Telemetry Sending is truthful, missing/estimated signals labeled; opt-in bounded recording and local support file; no automatic sharing | Source + receiver/capture checks + saved artifact inspection |
| UX-08 | Autosave/status, explicit calibration save, atomic destination apply, failed save visible, reset scope and retained backups | Persistence/failure tests + UI flow |
| UX-09 | One player ZIP/install/update path, discovery/folder fallback, closed-game/preflight checks, pinned dependencies, rollback, retained owner settings | Actual packaged installer fixtures + first launch |
| UX-10 | Uninstall owns only its files, preserves other mods/loader and user data by default, reports retained/modified files | Packaged uninstall fixtures |
| UX-11 | New user completes Connect → axes → buttons → FFB → drive entirely in Simple, without config editing or developer knowledge | Attended first-use walkthrough; optional hardware/dashboard configuration does not block readiness |

## Findings format

For each actionable finding, record:

- **ID / severity / rule:** stable identifier, priority and the relevant UX rule.
- **Observed behavior:** concrete trigger and what the player sees or must do.
- **Evidence:** source file/line or artifact and exact version; distinguish source
  facts, documented runtime evidence and untested inference.
- **Impact:** the extra decision, failed action or misleading state.
- **Proposed correction:** specific resulting behavior, preserving existing data.
- **Acceptance:** a short repeatable check that would close this finding.

Priorities: **P0** risks wrong-device force, lost data or broken shutdown;
**P1** blocks normal install/setup/control or makes an essential action unavailable;
**P2** causes avoidable confusion/inconsistency; **P3** improves visual fit/wording.
Assign priority to the concrete impact, not a hypothetical risk. An untested
hardware transition is an evidence gap, not automatically a confirmed P0 defect.

## Cross-game walkthrough

UX-01-S was added on 2026-09-16. Earlier audits do not establish Simple/Advanced
adoption. Add the complete [view walkthroughs](CONSUMER-SETTINGS-VIEWS.md) and
per-setting placement inventory to each consumer review. Keep the initial
connect/bind/check/drive sequence in Simple; exercise Advanced separately for
customization, diagnostics and preservation of existing tunes.

The additional UX-04-H/UX-05-D checks were made explicit on 2026-09-14. Earlier
audits do not establish compliance with them; retain their original evidence
dates and add a consumer review of these requirements.
The same day's [interaction detail](CONSUMER-CONTROLS-CAMERAS.md) adds UX-06-K
and makes the handbrake click/cancel flow explicit. Capture the guidance hash
so reviews can distinguish these changes from earlier text with the same date.

With each game in turn, perform the same sequence and record where instructions
or actions change:

1. Install from its player ZIP into a clean fixture, then a supported real install.
2. Find Wheel settings without opening developer docs; verify Simple Setup is the
   fresh-config default. Stay in Simple to bind three axes, cancel a
   capture, save calibration, bind Pause and Change camera. Check left/right and
   released/full pedal values against effective game input.
   If the game has a handbrake, bind a separate analog device and a button;
   verify released/partial/full input, reversed travel, cancellation and both
   bindings held/released together. For a digital-only game route, verify the
   visible threshold/release behavior and its documented limitation.
3. Close/reopen, restart the game, and unplug/reconnect the selected wheel. Confirm
   saved assignments, inactive status and the explicit replacement route.
   Check handbrake release on disconnect/focus loss without cancelling another
   held binding. In FFB, confirm the initial steering-wheel default, select a
   device directly, and test follow versus explicit override after a saved
   Steering rebind, USB reorder and restart. Exercise missing/non-FFB Steering
   and duplicate names with no output; never select the first wheel silently.
4. Start an attended low-strength drive using the saved On/Off preference, with
   no session-start step. Check F8/Stop saves Off and only On resumes. Confirm
   transient gates zero output without repeatedly reopening/enumerating devices.
   Open/close settings, pause, lose/regain focus, reset the car and finish normally.
   Confirm no stale held inputs, force or mounted camera outside player control.
5. Cycle through stock views, Bonnet and Bumper using the game's camera button;
   disable one added view; hold/release Look behind; inspect handoff to replay/photo
   or other available game-controlled camera states.
   Use the default numpad adjustments on each mount, then rebind movement, tilt
   and FOV in F6 without a numpad. Verify visible directions, cancel/conflicts,
   Clear/defaults, reset scope, persisted assignments and release after capture.
   Sliders and a list of source methods do not substitute for this interaction.
6. Enable a verified telemetry preset in Simple and verify it at the receiver.
   Check custom destination editing in Advanced and the actual saved destination
   back in Simple. Exercise either the offered player recording controls or an
   agent-prepared one-launch
   capture. Verify bounds, request expiry/consumption, normal-exit finalization,
   saved settings, output suppression for unattended launches and the saved file.
7. Update with changed user bindings/tunes and another mod installed. Confirm both
   survive. Exercise uninstall in a fixture and verify exactly what remains.

No unattended torque is needed to review source, render controls, validate a
package or inspect recordings. Schedule the attended steps separately and leave
them marked Not tested until someone at the wheel performs them.

## Keeping projects aligned

Link this baseline and the settings-view contract from each consumer's
contributor/agent notes. Classify every new UI setting in its placement inventory.
Include affected rule IDs in UI/installer change reviews and update the audit
when a finding closes.
Record legitimate game restrictions beside the relevant option and in an exception
table. Share labels, layout conventions and interaction policies; do not fork a
new native output layer just to adopt the UX. Before extracting shared widgets,
prove they work in both Mono and IL2CPP; the standard does not authorize replacing
working consumer rendering with an untested abstraction.
