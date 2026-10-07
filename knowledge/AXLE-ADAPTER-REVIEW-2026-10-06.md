# Shared axle adapter requirements after the iRacing review

STD-025 puts tyre steering arithmetic in AxleForceCurve, with engine sampling
and calibrated conversions in the consumer. That is the correct boundary.
Sharing the curve does not establish adapter units, calibration, delivery or
force replay. Preserve Art's pinned gold reference unchanged (STD-021).

Independent review of iRacing aac28a0 executed frozen production bodies and the
actual vendored managed curve with fake settings/logger/output, never Unity or
a hardware sink. Six scenarios and nine checks reproduce three defects while
all 329 existing consumer tests pass:

| Boundary | Reproduction | Reusable requirement |
|---|---|---|
| Selected model to recording | Classic command 400 is recorded while Grip sends 2428; all 24 fake-delivered samples fail native.command. Muted samples pass even after corrupting every grip.Output. | Record/replay the selected production model, complete settings and state, validity, source identity, request and separate delivery. Preserve old contracts; reject unsupported models rather than validate an unused model. |
| Unavailable inputs | Grip request 150 jumps to Classic 10000 for one missing sample. A zero-force timeout latches Classic even after valid inputs return and Reset. | Missing data must not silently become a much stronger model. Prefer bounded release with an explicit status; retain legacy models as deliberate choices. Any automatic fallback needs independent fault evidence, bounded transition/recovery and recorded state. |
| Rest calibration | Countdown load is skipped; a first moving load seeds the reference. Identical later samples produce 4856 versus 485 with proper rest calibration. | Separate observation/calibration from output eligibility; qualify stationary grounded load or use an explicit reference. Record calibration identity and transitions. |

Values are synthetic model requests before delivery scaling, not measured
wheel torque or a diagnosis of the owner's drive. The frozen harness asserts
known defects, not success of a future fix. Consumer review:
[2026-10-06-grip-adapter.md](https://github.com/d-b-c-e/dbce-mods-iracing-arcade/blob/main/docs/reviews/2026-10-06-grip-adapter.md).
Private standalone evidence is retained outside disposable checkouts at
%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/
iracing-grip-review-aac28a0-20261006, with source, managed dependencies,
synthetic recordings, full analyzer reports and a SHA-256 manifest.

Before copying an adapter to another game:

1. Establish signed force and slip units from engine/capture evidence, plus
   the current wheel identity and completed physics sample. A native field
   name or correct array element type is insufficient. Preserve unavailable
   values explicitly rather than substituting ordinary zeros.
2. Pass standard inputs to the shared curve. Keep conversion/calibration in
   the consumer. Any reference relative to axle load is provisional until
   comparable events at Strength 50 and owner feel establish acceptance.
3. Keep model calculation independent of the physical-output gate. Record
   model identity/version, raw inputs, adapter conversions, settings, reset/
   calibration/filter/fallback state, intended request, scaled command and
   delivery result separately. A property named delivered output must not
   merely contain a pre-scale request. No software log measures torque.
4. Exercise the real adapter through recorder and analyzer, not just the pure
   curve. Include model switches, muted/absent devices, accepted/rejected/
   scaled commands, recording starting mid-drive, pauses, car changes and
   corrupted/unsupported model data. Offline trial exporters must dispatch
   to that model and pin both adapter and shared-curve identities.
5. Preserve effect families. Sharing steering arithmetic does not implement
   Art's finite crash push/rattle, landing or texture in another game.

No new standard or runtime API was introduced by this review. STD-025 remains
the owner requirement; iRacing's initial adapter is partial until corrected.
No install, launch, input, focus, display, device or physical force action was
part of this review. Claude owns the coordinated consumer implementation.

## Follow-up: comparison logging in iRacing cd9c017

The owner's sharper-but-weaker report led to a provisional ideal-slip/load-ratio
tune. The first three findings remain open. Two more fake-sink cases show why
live comparison must run the actual candidate model: dividing clipped output by
gain labels a 0.90 Strength-50 request as 0.50 when observed at Strength 100;
FFB-disabled driving is also counted as zero force. Use an independently reset/
seeded gain-1 analysis processor on original inputs or explicitly collect only
eligible actual Strength-50 samples. Separate car/tune epochs. Never derive a
counterfactual by scaling a clipped/smoothed output.

The hardcoded Art P50/P95/RMS 0.36/0.76/0.43 is the whole Norway capture, with
140.76 of149.55 seconds above60km/h. Its20–60 band instead has
P50/P95/RMS0.1856/0.4334/0.2353;60+ gives0.3712/0.7637/0.4419. The second
Art capture differs again at identical tuning. Keep source identity, matching
bands/event types, duration weighting and clipping/release measurements with
every comparison. No single drive distribution is a universal calibration target.
See the consumer review's repack.121 addendum for source and reproduction paths.
