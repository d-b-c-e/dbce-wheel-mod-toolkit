# Steering and effect strength controls

STD-027 makes preferred steering weight adjustable independently of crash intensity.
The existing shared model and effect inputs already support this split; a second
multiplier is unnecessary in games whose steering gain is already independent.

## Player controls

| Control | Meaning | Behavior |
|---|---|---|
| Steering strength (%) | Tyre-based cornering resistance and return-to-centre feel | 0–100; 50 is the shared reference level. Preserve saved values. |
| Crash strength (%) | Intensity of collision kicks | Independent of steering gain; retain the qualified game/effect default. |
| Landing / Shift / Road strength | The supported named effect | Separate gain and clear unit/range; omit unsupported effects. |
| FFB Off / Stop | Stop all physical wheel effects | Existing lifecycle, focus, device and output gates still apply. |

Suggested steering help: "Cornering resistance and return-to-centre force from
the tyres. Crash strength is separate; 50 is the reference level, not torque in Nm."
Suggested crash help: "Intensity of collision kicks, independent of Steering
strength. Use FFB Off or Stop FFB to stop all wheel effects."

A strength slider changes magnitude. It does not by itself correct rounded force
buildup or delivery delay. Do not quietly add a fixed spring: tyre self-aligning
force varies with load, slip and speed, and should retain its existing low-speed
fade and grip information. Games without tyre physics must describe the actual
arcade steering/centering law instead of claiming tyre forces.

Use plain labels on the FFB page. Keep steering in Simple, with independent effect
controls there or in a clearly named Effects group. Model constants and response
shaping belong in Advanced. An old Strength config key may stay unchanged while
the visible label improves. Do not show two sliders for the same multiplier.

## Adapter and recording contract

`AxleForceCurve.Normalised(..., gain, ...)` already accepts the steering gain.
For qualified STD-025 adapters it is `SteeringStrength / 50`. CrashCue accepts an
independent event magnitude; contacts use the crash strength at arrival. Landings,
shifts and texture follow their own recorded amplitude rules. These are separate
inputs before composition, not claims that physical effects never interact.

A real master gain may be retained, labelled **Overall strength**, with help saying
which effects it scales. Add an independent gain at the steering stage if needed,
preserving the old overall value and behavior until the new control is adjusted.
Do not rename a master gain and advertise false independence. Damping/friction or
road texture must not be silently swept into a "centering only" adjustment.

Keep the existing ordering of shaping, clamp, smoothing, effect composition and
output gating unless a versioned model change explicitly requires otherwise.
More steering can leave less headroom for impacts at the combined clamp, even
when the generated crash cue is identical. Do not promise unchanged combined feel.

When composition changes, record effective steering/effect strengths, model and
calibration identities, separate pre-composition requests and final requested /
delivered command. Contact strengths must be captured at event time; tuning
changes split comparison epochs. Muted playback still computes and records the
signals. Legacy captures retain their original composition, with no missing
parameters silently filled in a way that changes replay. A UI-only rename changes
none of these contracts or the force-source fingerprint.

## Checks and rollout

Calibrate the game's reference rather than adding another steering multiplier.
For example, the iRacing owner requested old80 become new50 on October8: with
the same `Strength / 50` gain, changing the load reference ratio from .80 to .50
gives the same pre-clamp steering (`1.6 / .8 = 1 / .5`). Record that reference per
row, preserve old captures and saved tunes, and apply an owner-requested local
setting change explicitly. The original4197-row tape gives identical integer
steering commands for those paired trials; that does not prove matching physical
torque across games. Crash amplitude remains independent; combined headroom may
still change. Full evidence: `knowledge/IRACING-CENTERING-CALIBRATION-2026-10-07.md`.

For a newly separated consumer, exercise zero/partial/full steering with identical
contacts, and vary crash intensity with identical steering inputs. Check that the
unselected component is unchanged before composition. Include final saturation,
FFB Off, invalid input, release/reset and recording/replay; do not rely on a pure
helper test that bypasses the actual adapter and recorder. These tests never need
a real wheel. Physical feel remains an attended acceptance step.

| Consumer | Inspected behavior | STD-027 status at introduction |
|---|---|---|
| iRacing Arcade | Strength scales steering only; CrashStage uses independent event-time strength | Source labels/help corrected; 403 tests and plugin build pass. Five production-adapter/recorder cases vary steering/crashes independently, including zero steering with a live cue; strict replay passes. Packaging/rendered labels pending. |
| Art of Rally | Strength drives AxleForceCurve; crash/landing/shift have independent amplitudes | Suitable existing split; label/discoverability rollout recorded in consumer ledger. No reference tune change. |
| Woden Rally Edge | Crash is separate, but steering strength includes damping; Classic and Grip scales differ | Partial: audit/isolate the steering-only gain before claiming centering-only independence. Keep the owner-selected model and legacy captures. |
| DRIVE Rally | Grip Strength also scales damping and texture; Classic scales its combined response | Adapter/composition work needed, not just a rename. Preserve model 2/3 replay and installed owner tune. |
| F-Zero / other consumers | Not qualified by this survey | Audit the component split; use arcade terminology where applicable. |

Source pointers: iRacing `ForceFeedback.cs`, `ForceStages.cs`, `Panel.cs`,
`ForceContractTests.cs`; Art `SettingsPanel.cs`, `Settings.cs`, `ImpactController.cs`;
Woden Core `ForceSignal.cs`, `GripSignal.cs` and plugin `ForceController.cs`;
DRIVE `DriveGripModel.cs`, `DriveForcePipeline.cs`.
No portfolio-wide adoption, rendered UI or physical normalization is implied by
this source survey. Consult STANDARDS.md and each consumer's TOOLKIT-ADOPTION.md.
