# Woden Grip v4: adapter design review

Owner priority: improve Woden's Strength50 response using Art of Rally as the
fixed reference. Claude owns the Woden force model, controller and replay work;
Codex owns this independent design review. Reviewed committed Woden `f1ee7af`
(new friction inputs) and `050c357` (ordered crash contacts), the proposed v4
formula, current toolkit AxleForceCurve@1 and the immutable Kenya recording.
The unfinished v4 working files are not a reviewed implementation or release.
No runtime edits, installs, game/Steam launches, device output or rig actions
accompany this review. The owner prohibited Steam launches before 02:00 CT.

## Direction

A separately versioned, selectable Grip v4 with unchanged Classic v3 is a useful
experiment. The Kenya saved Strength49.583332/Peak25 result spends 46.46% of valid
interval time at its cap; increasing only v3 strength cannot lift that ceiling.
This does not establish a correct new reference ratio. Art's whole-drive P95 near
0.76 comes from another route/workload; use matched event/speed intervals and
owner feel, not percentile equality alone, to judge the eventual Strength50 tune.

Art's reference is fixed in
[`art-of-rally-50-v1.json`](../profiles/normalization/art-of-rally-50-v1.json).
Adopting its curve does not by itself validate the game's input adapter, force
units, direction, timing or physical response.

## Resolve in the adapter

1. **Name the output an estimate.** Unity 6000.3, matching Woden's recorded engine
   family, documents WheelHit.force as contact-force magnitude, not signed lateral
   tyre force. Its friction API specifies a spline curve and separate sideways
   slip; it does not provide an exact solved-Fy reconstruction contract from the
   proposed product. Treat `contactForce * curve(abs(slip)) * stiffness` as an
   adapter estimate. Preserve original fields and the chosen evaluator version.
   A linear or chosen Hermite approximation must be identified as such, not
   described as the exact Unity solver. Sources: [WheelHit.force](https://docs.unity3d.com/6000.3/Documentation/ScriptReference/WheelHit-force.html),
   [WheelFrictionCurve](https://docs.unity3d.com/6000.3/Documentation/ScriptReference/WheelFrictionCurve.html),
   [WheelHit.sidewaysSlip](https://docs.unity3d.com/6000.3/Documentation/ScriptReference/WheelHit-sidewaysSlip.html).

2. **Retain signed cancellation between front wheels.** Summing their force
   magnitudes and then taking the sign from Classic creates a discontinuity when
   nearly opposed contributions swap dominance: a tiny signed imbalance can
   command a large force in either direction. Form the signed axle estimate before
   applying the deliberate device-direction convention. Preserve both the signed
   estimate and any Classic direction proxy in the trace. Test equal/opposed slip,
   near cancellation, mirrored turns, unequal loads/curves, one grounded wheel,
   both airborne and unavailable grounded-wheel data. Existing reverse eligibility
   must stay explicit until its direction is qualified.

3. **The proposed IdealSlip parameter cancels.** In
   `Trail(absSlip / extremumSlip * IdealSlip, IdealSlip)`, with IdealSlip >= 1,
   AxleForceCurve divides the first argument by twice the second. The result is
   determined by `absSlip / (2 * extremumSlip)`: peak slip gives trail0.8, twice
   peak gives0.6, regardless of IdealSlip. This is algebra from the actual shared
   curve. Use an explicitly normalized-slip mapping and fixed compatibility angle,
   or introduce a meaningful independent trail scale. Do not expose a no-effect
   IdealSlip tuning control or label the constructed angle as measured degrees.

4. **Keep sampling phase explicit.** Woden's existing MainCar.FixedUpdate postfix
   samples before the upcoming physics solve. Contact/slip may describe the prior
   solve while live friction properties already reflect a new surface. Read-only
   access does not prove those inputs are synchronized. Record the phase and curve
   changes; retain that limitation in claims about reconstructed force.

5. **Separate calibration from delivery.** Stationary valid grid samples must
   update reference state while Driving=false, FFB off or physical output muted.
   Analysis should consume the same production reference with independent enabled
   filter state. Record car, reference, model, tune and reset epochs; explicitly
   label provisional/unavailable intervals. Reuse the tested pure reference policy
   and vectors rather than quietly creating a different same-named algorithm.

6. **Version the whole behavior change.** Classic v3 applies Strength/100 and a
   ForceShaper with its own fade, ramp, slew, damping, saturation, smoothing and
   peak cap. Proposed v4 uses Strength/50, AxleForceCurve and EMA0.2 without the
   Classic25% ceiling. This is more than removing one cap. Preserve the original
   v3 law/configuration/replay behavior and saved model choice. Retain physical
   lifecycle stop/focus/stale/recovery guarantees independently of the new model.
   Steering and crash model versions/settings remain separate.

## Recording and replay checks

The sealed `woden-kenya-20261005/source.jsonl` has no `sideFriction` fields
(read-only scan of all 3,658 JSONL lines). It remains the v3 compatibility and
trajectory reference. It cannot supply an original v4 trial; inserting current
curves/defaults into that recording would invent missing historical inputs.

A new muted v4 capture needs valid stationary calibration followed by movement,
all original grounded-front-wheel friction inputs, explicit reference/state,
per-row model/configuration identity and the selected actual and analysis output.
Unknown versions and missing required inputs remain unavailable. Exercise the
real RecordedForceReplay path across v3/v4 model changes, car changes, resets,
surface/curve changes, dropouts and airborne states. Revalidate original v3 rows
without modifying their source or weakening identity checks.

The crash contract also needs the latest iRacing corrections:

- `050c357` records contacts in one immutable tick snapshot, but consumes
  strengthPercent at contact time without storing it. Capture the event-time
  setting/trigger request; the next tick's setting can differ.
- Unseeded prefixes must remain explicitly unqualified for complete crash replay,
  even if steering and recorded-cue arithmetic agree. Overflow must not pass as
  a complete event history.
- Preserve Woden's actual float steering-plus-cue composition and sink conversion
  order. iRacing's integer composition is a different contract.
- `crash-constant-fallback@2` remains separate from Art's independent finite push
  and rattle delivery. A steering P95 does not qualify crash feel.

See [the shared crash review](CRASH-PORT-REVIEW-2026-10-06.md) and iRacing's
`docs/reviews/2026-10-07-force-contract-ef8f1c8.md` for frozen reproductions.

## Ownership and next evidence

Claude continues implementation and sends a clean source identity for review
before an owner A/B. Codex makes no overlapping Woden runtime edits. The next
useful steps are pure adapter/replay checks, then a coordinated muted recording
after the Steam hold, followed by attended comparison when the owner returns.
No new runtime test is claimed by this design review.

The Woden ledger currently lacks STD-021..025 and retains unchecked016/017/020.
The v4 commit should add touched021/025 as partial/experimental with its evidence;
already-added frame-rate features and unrelated setup/UI items need separate
reconciliation. This is a documentation gap, not a reason to claim those features
absent or to overwrite owner preferences.
