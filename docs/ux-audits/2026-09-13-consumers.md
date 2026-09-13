# UX-1 audit: iRacing Arcade Wheel and Art of Sim Rally

Reviewed 2026-09-13 against [UX-1](../CONSUMER-UX.md),
[setup guidance](../CONSUMER-SETUP.md) and the [checklist](../CONSUMER-UX-CHECKLIST.md).
Both repositories have active working-tree changes. This is a **source and
documentation audit**, not a new visual inspection, package certification or
attended wheel test. Neither sibling's source or installed game was changed.

The main departure is structural: a player has to relearn where setup lives,
which input route to choose, what the same controls are called, and how to stop
or diagnose output. Matching colors alone will not resolve that.

## Scope and evidence

- **iRacing Arcade Wheel:** HEAD `e2c823a86bc766938fb95566b20f2aa4473bc613`,
  plus current uncommitted 0.3.8 source and player/installer documentation.
  `docs/STATUS.md` records actual wheel input/recording and finish handoff;
  attended Alt+Tab recovery remains open. Those are previous consumer evidence,
  not tests performed by this audit.
- **Art of Sim Rally:** HEAD `5a3c369f90209990e77c38e0ee95455484eda3dd`,
  plus current documentation/installer changes. AGENTS records published and
  installed 0.2.5, release source `c6242a0a163315f7a390d3860c6204b4ca215619`.
  Its new setup/installer documentation is ahead of the immutable published ZIP.
  Owner RC8 acceptance does not close its remaining visual/full hardware matrix.
- [Source manifest](2026-09-13-source-manifest.json) records SHA-256 values of
  the relevant local files. References below are **consumer-relative file:line**
  locations in that snapshot, not claims about the corresponding release tag.

## Comparison

| Area | UX-1 target | iRacing Arcade | Art of Sim Rally |
|---|---|---|---|
| Entry | F6 → Wheel settings | F6; standard entrance mostly present | Ctrl+F10 → Mod Manager → mod settings |
| Navigation | Setup, Controls, FFB, Cameras, Telemetry, Help | Eight pages; Recording/Devices separate; transmission prominent in setup | Seven sequential foldouts; Steering/Shifter/Direct input separate |
| On/Off | Label + explicit Off/On | Checkbox primitive; `Enabled`, `Send telemetry` | Checkbox; `Enabled`, `Read the wheel directly` |
| Axis setup | Guided calibration + final-input bars | Live bars, Rebind/Invert/deadzone; raw range processor exposed | Assign/Flip, numeric values; capture followed by automatic range extension |
| Identity | Stable saved device; missing remains missing | FFB index/name matching and first-device fallback; shifter index/name | Strict identities already implemented for direct input/shifter/explicit FFB choices |
| Stop action | Visible Stop FFB and F8, latched resume | No player-facing panic action found; F8 remains a legacy camera-bind setting | No dedicated player-facing panic action found |
| Cameras | Native cycle, independent Bonnet/Bumper, handoff stops FFB | Native cycle and finish handoff; one combined mounted-views toggle | Native cycle, separate toggles, camera-conflict handback; broad matrix still open |
| Telemetry/support | Sending facts + recording + local support | Rich bounded recording; no support-file/Open folder action in panel | Sending status and support file; recorder explicitly developer-only |
| Install | One install/update route and folder fallback | Bundled offline runtime, drag-folder fallback, rollback/ownership logic | Separate UMM setup; custom path requires command-line argument |

## iRacing Arcade findings

### IAW-01 — P0 — Saved FFB selection is an enumeration position (UX-04/05)

**Observed in source:** `FfbDeviceSelector.cs:25–60` returns `DeviceIndex` directly,
matches the steering device by product-name substring otherwise, and falls back
to the first FFB device when unresolved. `FfbNative.cs:155–157` clears the GUID
preference and sets the index. `Panel.cs:568–586` persists picker positions.

**Impact:** enumeration changes or duplicate names can route output to a different
physical device. This is a concrete selection-policy defect; this audit did not
reproduce wrong-device torque. The similar-looking automatic picker hides a
material difference from Art's strict saved-identity route.

**Correction:** migrate an explicitly chosen wheel to a verified instance GUID;
show the saved wheel when absent and require explicit replacement. Use the
toolkit's selection result and strict open. An automatic suggestion can follow
steering, but must not silently become another saved output wheel.

**Acceptance:** offline reorder/disconnect/duplicate-name fixtures retain or
refuse the intended identity; an attended zero-output connection check confirms
the picker and actual opened device agree before any force test.

### IAW-02 — P1 — Global binding reset commits immediately (UX-08)

`Panel.cs:395` calls `BindingStore.ResetAll()` from one click. In
`BindingStore.cs:289–306`, that removes every override, clears entries and saves.
There is no confirmation or reset-specific backup in this path. Migration backups
elsewhere do not undo an arbitrary later reset.

Add a scoped reset first, and a deliberate global confirmation with a recoverable
snapshot. **Acceptance:** Cancel leaves all bindings intact; confirmed reset can
restore the exact prior set, including added wheel/menu slots.

### IAW-03 — P1 — Stop/recovery is not a player action (UX-05)

The reviewed panel has FFB Enabled and connection/output prose, but no permanent
Stop FFB button or latched Resume FFB state. Native `PanicStop` is used internally;
that is not a discoverable player control. `Settings.cs:220` still reserves F8
as `BindViewKey` in the legacy binding configuration.

Add the visible stop action and standard hotkey, resolve the legacy shortcut
collision, and show the reason for normal suspension versus a user stop.
**Acceptance:** F8 works on every page and while driving; output stays stopped
until deliberate resume. Preserve automatic ramped recovery for ordinary pause.

### IAW-04 — P2 — Setup and navigation foreground implementation choices (UX-01/11)

`Panel.cs:49` declares Wheel setup, All controls, Force feedback, Gearbox, Camera,
Recording, Telemetry, Devices. `DrawQuickWheel` calls `DrawTransmissionMode`
before the axes (`Panel.cs:266–271`). `DrawControls` exposes Driving/Ui/Campus
maps and pages through two actions at a time (`Panel.cs:365–395`).

Use the standard six pages with optional Driving, merge quick/all controls into
one curated Controls view with advanced raw maps, and put recording under
Telemetry. Setup should show the next unbound axis before optional gearbox work.
**Acceptance:** a player coming from another mod can find axes, Pause, FFB,
camera and recording without consulting this game's guide.

### IAW-05 — P2 — Calibration asks the player to interpret input processing (UX-02/04)

`Panel.cs:304–319` exposes `Invert`, `Map -1..1 to pedal`, a decimal deadzone and
raw ±1 values. Rebinding and inherited steering-deadzone correction are valuable,
but there is no explicit centre/endpoints/released/full calibration transaction.
Action rows also vary between Rebind, Bind, set, clear and Default
(`Panel.cs:654–746`, `:795–806`).

Keep the tested input path; wrap it in Bind/Calibrate with 0–100% pedal display,
explicit inversion and Save calibration/Cancel. Normalize action labels and
button capitalization. **Acceptance:** reversed pedals and partial travel can be
set up without explaining processor ranges; cancel preserves the old calibration.

### IAW-06 — P2 — Common FFB settings mix player tuning with model constants (UX-02/05)

`Panel.cs:534–564` uses Enabled, unitless Strength, Reference torque and prose
instructing the player to read log peaks. The status says “Wheel open. Output now”
based on readiness and a value, rather than exposing the full inactive/error/
delivery distinction. `Widgets.cs:143–150` supplies a checkbox-style primitive.

Present `FFB: Off/On`, Strength with a clear percentage meaning, selected wheel
and reasoned status. Move reference torque/model details to Advanced and retain
the current owner's effective tune through any presentation migration.
**Acceptance:** an unopened, paused, stopped or failed wheel has a distinct
actionable state; a computed value alone cannot read as successful delivery.

### IAW-07 — P2 — Shifter selection is still index/name based (UX-04)

`ShifterDevice.cs:72–88` resolves an index or first substring match; Settings
stores index/name. Move the role to stable identity independently of the FFB
wheel. **Acceptance:** USB reorder and unplug/reconnect keep the same shifter
gates; missing hardware cannot fall through to another button device.

### IAW-08 — P2 — Cameras and support need the remaining common affordances (UX-06/07)

`Panel.cs:815–826` has one `Bonnet and bumper views` toggle, with both sets of
settings on the same page. `DrawRecording` (`:339–352`) shows the path as text,
but has no Open folder or consolidated support-file action.

Give Bonnet and Bumper independent cycle inclusion, one selected view's controls,
Open folder and a local Create support file action. Preserve the recorded native
camera cycle and finish handoff. **Acceptance:** disable either added view without
losing the other; collect support without hunting multiple logs. Retest the
documented open Alt+Tab case rather than treating this audit as its result.

### IAW-09 — P2 — Save/connection feedback and keyboard navigation are incomplete (UX-03/08)

`Panel.cs:95–128` defers configuration persistence until panel close and only logs
save failures. Telemetry host/port fields are edited directly (`:844–868`).
`Widgets.SliderRect` uses passive focus and mouse events (`Widgets.cs:168–202`).
The opaque, screen-scaled panel is a good existing foundation; it does not by
itself establish keyboard operability or a visible save outcome.

Add Saved/error feedback, Apply connection for a complete endpoint edit, and
keyboard focus/activation for rebuilt widgets. **Acceptance:** navigate/edit
without a mouse, simulate a failed save, and verify the old endpoint survives an
invalid edit. Visual scaling and long-device-name behavior remain Not tested here.

## Art of Sim Rally findings

### AOSR-01 — P1 — Common settings require the loader's navigation (UX-01/11)

`Main.cs:67–68,131–134` registers the Mod Manager GUI/save callbacks;
`Settings.cs:7` and the current setup/installer guide instruct Ctrl+F10. The
settings renderer is a sequence of foldouts (`SettingsPanel.cs:54–60`), not the
shared Setup/Controls/FFB/Cameras/Telemetry/Help route.

Add F6 Wheel settings using the existing renderer/controller logic as appropriate;
retain UMM as a secondary entrance. Do not replace the loader to achieve this.
**Acceptance:** the same shortcut and page names work after switching from Woden
or iRacing; Esc/Close restore the game's pause and cursor correctly.

### AOSR-02 — P1 — Input setup asks users to choose a backend and another screen (UX-04/11)

`SettingsPanel.cs:69–80` exposes steering fixes and the game's bind-any-device
route; `:164–175` separately exposes “Wheel input (direct)” / “Read the wheel
directly,” instructing players to keep working game bindings and use direct input
only where needed. Menu input stays with keyboard/pad. The setup guide repeats
this decision. These routes are technically justified, but the user owns the
routing decision instead of a single task-oriented setup.

Offer common axes/buttons in Controls and choose the tested route internally,
showing a short compatibility explanation only when it changes the user's next
action. Retain current GUID bindings and the no-runtime-Rewired-switch rule.
**Acceptance:** a new wheel user binds axes and Pause without knowing Rewired or
DirectInput; document the menu-binding gap until supported.

### AOSR-03 — P1 — No prominent latched Stop FFB control (UX-05)

`SettingsPanel.cs:98–105` provides Enabled and resets output when changed, but the
reviewed mod has no F8/Stop FFB/Resume FFB UI route. Existing shutdown, focus and
watchdog protections are separate and should be retained.

Add the shared stop action in the new panel shell and keep it visible on every
page. **Acceptance:** an attended stop test leaves steering input usable and
force stopped until explicit resume, including separate landing effects.

### AOSR-04 — P2 — Assignment and calibration are implicit (UX-02/04)

`SettingsPanel.cs:178–195` shows numerical live values, Assign, Flip and Clear.
`WheelInput.cs:330–348,410–428` captures a moved control, commits it and tells
the player to exercise full travel once. Range extension (`:250–279`) then
updates calibration during use. Timeout/cancel and identity retention are already
present; there is no explicit full calibration preview/Save step or left/right
and 0–100% visual check.

Keep compatible persisted calibration but add an explicit guided candidate with
Save calibration/Cancel, bars, `Invert steering/pedal: Off/On`, and readable axis
names. **Acceptance:** a cancelled or ambiguous capture cannot replace a working
binding; verified endpoints remain predictable on later drives.

### AOSR-05 — P2 — Wording and numeric meanings differ across the family (UX-02)

`SettingsPanel.cs:100,110–136,272,374–389` uses a bare Enabled checkbox, Strength
with “50 is the baseline,” fractional Smoothing, Send telemetry, and the shared
checkbox/slider helpers. Foldout headings are forced uppercase (`:351`).

Use the common labels, sentence case, explicit states and units. Keep existing
force arithmetic/tunes intact; do not convert a smoothing coefficient to “ms”
without a correct conversion. Explain its actual meaning in Advanced if a common
time-based control is not feasible. **Acceptance:** the common string review
passes and old saved settings produce the same force output after a UI-only change.

### AOSR-06 — P2 — Install still depends on a separate manager workflow (UX-09/11)

Current `tools/installer/README.txt` and `docs/SETUP.md` require installing UMM
separately, then running the mod installer or dragging its ZIP into UMM. The
custom-folder recovery requires running `Install.bat -GameDir ...` in PowerShell.
Updates/settings preservation and scoped uninstall are existing strengths.

Provide a guided, supported UMM prerequisite step and folder picker/drag-folder
fallback through one install entry point. If UMM cannot be bundled, name this as
a loader exception and make the handoff explicit. Do not advertise unreleased
installer improvements as part of published 0.2.5.
**Acceptance:** a clean machine and a non-default Steam path can install/update
without script editing; a second mod and owner settings survive uninstall.

### AOSR-07 — P2 — Support is available, recording remains a developer workflow (UX-07)

`SettingsPanel.cs:304–325` already offers a local support file and result text;
`:286–299` reports the active telemetry destination and packet count. Keep these.
AGENTS and `docs/DEVELOPMENT-CAPTURE.md` explicitly place recording in a separate
non-shipping probe controlled by external commands.

Record this as a **product-scope exception**, not a broken recorder or permission
to ship the probe. If user session recording is adopted, build a supported,
bounded Start/Stop/Mark/Open folder flow under Telemetry with production shutdown
handling. **Acceptance:** the product either states recording is unavailable or
provides a supported saved artifact without developer tools. No automatic uploads.

### AOSR-08 — P2 — Save and endpoint changes depend on implicit lifecycle events (UX-08)

`Main.cs:131–134` saves through UMM callbacks; bindings use their own deferred
save, and `SettingsPersistence` already writes via atomic replacement.
`SettingsPanel.cs:280–299` edits host/port directly and waits for idle to change
the active endpoint. There is no common Saved indicator/Apply connection action.

Preserve the existing atomic writer and safe idle gate, add a visible save/error
state and commit a validated endpoint together. **Acceptance:** typing an invalid
port or encountering a write failure leaves the last working connection/settings
available and tells the player what to fix.

### AOSR-09 — P2 — Legacy handling control sits among ordinary steering setup (UX-01/11)

`SettingsPanel.cs:86–89` exposes “Disable steering limiter on car spawn (legacy)”
and explicitly warns that unticking does not restore the current car. This is
distinct from linearizing wheel input and makes first setup harder to reason about.

Keep it off and move it out of the normal setup flow; review removal/migration
under the consumer's physics policy. If retained, put it under an explicitly
restricted Driving/Advanced section and explain the respawn boundary.
**Acceptance:** ordinary calibration cannot change this option or imply that
its effects revert immediately. No physics change is proposed by this audit.

## Preserve these foundations

- iRacing's real input-value bars, additive wheel bindings, stock menu actions,
  held Look behind, detailed recordings and packaged ownership/rollback design.
- Art's strict identities, disconnected-binding retention, local support file,
  protected settings writer, separate Bonnet/Bumper controls and inherited UMM
  text scaling. KI-32's fixed tiny-font defect is already corrected in 0.2.5 source;
  visual confirmation is still pending, so it is not listed as an unfixed defect.
- Both consumers' tested game-specific force models and prior attended evidence.
  UX convergence does not authorize arbitrary retuning, loader replacement or
  broad shared widget extraction.

## Recommended adoption order

1. Correct iRacing's strict output identity and recoverable global reset; add the
   common stop/resume action to both consumers with lifecycle checks.
2. Adopt one panel shell, page order and common vocabulary; keep established
   game-specific renderer implementations and preserve config keys/data.
3. Consolidate controls and guided calibration, then surface useful status/errors
   beside the affected control. Keep raw maps/model constants under Advanced.
4. Unify install/first-use instructions and fill the folder/prerequisite gaps.
5. Perform the same cross-game walkthrough from the shared checklist, including
   720p/high-DPI visuals and attended camera/FFB transitions. Attach results to
   exact packages. No audit item is closed merely by updating a README.

Woden is the third implementation to align: its 0.2.2 input/camera fixes are
development work, not an already accepted UX reference. Use UX-1 for its next
panel iteration and explicitly retain the experimental session-start step until
physical output is validated.
