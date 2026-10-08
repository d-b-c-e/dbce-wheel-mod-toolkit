# Centering calibration from owner feedback

[dbce-mods-iracing-arcade a2a8c39, 2026-10-07]

The owner reports Formula 4 steering at Strength 50 substantially lighter than
Art, with satisfactory crashes. The preserved October 7 evening log's last
cumulative summary has 19 seconds at 20–60 km/h and 16 seconds at 60+ km/h.
Its P50/P95/RMS are 0.11/0.42/0.18 and 0.12/0.40/0.21, compared with Art Norway's
0.19/0.43/0.24 and 0.37/0.76/0.44. These support the subjective finding but do
not match manoeuvres or measure delivered torque. The original drive was not
recorded as a force tape; repeated cumulative summaries are not separate runs.

First comparison candidate: keep Strength 50 and change the existing adapter
LoadReferenceRatio from 1.15 to 0.80. Production AxleForceCurve then receives
1.4375 times the target before its clamp/smoothing. No shared-model change,
synthetic spring, speed boost or crash gain is added. Ideal slip, smoothing,
direction and the owner's crash strength 70 stay fixed. This is a prepared
candidate, not an applied tune or accepted calibration; no new global default.

Production-law synthetic checks cover 672 combinations plus the actual crash
stage: 31,918 assertions, no native/device path. Saturation increases from
32 to 80 grid cases; these are not owner-drive clipping rates. At 100 km/h,
6-degree slip and side force 0.5 times resting load, magnitude changes from
0.3913 to 0.5625. At a ratio of 1.0, the candidate clips at 1.0 instead of 0.7826.

Reusable lessons:

- Full-scale reference is the existing adapter calibration point. Try a bounded
  attended calibration there before introducing another force model or spring.
- Preserve accepted event effects independently. Their calculated cue can stay
  identical while a stronger steering signal changes headroom in a shared clamp.
- P95 is not peak force. A drive histogram does not identify centering events,
  nor justify scaling an already clipped/smoothed series into a candidate.
- Peak-window force/slip logs may be paired with current speed/output, not values
  at that peak. They cannot reconstruct the original force stream.
- The resting reference changed from about 15,409 to 10,384 in this drive; a
  later qualified stop can replace it. Preserve per-row reference and lifecycle
  in the next recording instead of treating the printed ratio as the only gain.
- An installed slider can be ineffective for the selected model. Current iRacing
  Centre boost and Full force controls affect Classic only; Claude was asked to
  hide them from the single Grip panel. No new Grip boost was silently added.

The next attended comparison uses the same Formula 4/car/stage and unchanged
base profile, with separate baseline/candidate recordings from the countdown.
Keep steering release, clipping and separately labelled impacts in the check.
Art's pinned reference remains unchanged. Consumer details and private repeat
evidence are in `docs/reviews/2026-10-07-centering-calibration.md`.

## Owner trial and a bounded speed-reference experiment

[iRacing repack.142 / 7a5e6ad, owner report and source review, 2026-10-07]

The owner tried .80 at50 and found it stronger but still weaker than expected,
with rounded rather than sharp response. Setting smoothing to0 did not noticeably
change that. The closed smoothing-.09 epoch logged22 seconds at20–60 with
.18/.51/.26, and31 seconds at60+ with .25/.62/.36 (P50/P95/RMS). The zero-smoothing
epoch is separate. No F11 tape was saved; the preserved log and video cannot
reconstruct the force waveform or identify a cause of rounded response.

The first **offline-only** speed experiment keeps ratio .80 at60 km/h and below,
blending with zero endpoint slopes to gain1.25 / effective ratio .64 at100+.
At80 it is gain1.125 / ratio .7111. These transition points and the25% ceiling
are deliberate test bounds, not a fit inferred from percentiles. Original Art
data/defaults, crash strength and the shared steering law are not changed.
`tools/calibration/SpeedReferenceCalibration.cs` is vendored into the consumer
diagnostics, not the shipping plugin/library. The profile is immutable data at
`profiles/normalization/iracing-speed-reference-candidate-v1.json`.

Consumer377 tests include invalid profiles/speeds, unavailable-source eligibility,
production clamp-before-filter, refusal of synthetic/changed-identity captures
as original evidence, and no overwrite. The explicit synthetic CLI passes6333
production checks over1666 surface cases plus transitions and actual crash-stage
composition. Grid saturation rises260 to290; these are not time-weighted owner
rates. Thirteen unchanged cue samples include nine where additional steering
leaves less incremental crash headroom. Cue identity is not physical equivalence.

The existing original EuroTruck3001-row capture passes strict selected-force
and crash replay, then produces identical .80 and speed-candidate steering
on every row (maximum40.62 km/h; zero eligible high-speed rows). This validates
the input/export path and lower-speed preservation, not the proposed high-speed
feel. Frozen analyzer/profile/evidence is private under
`SessionEvidence/iracing-speed-reference-candidate-20261007`.

Sharpness findings:

- The normal active path at smoothing0 uses the current target immediately.
  At .09080882 a step already reaches90.9% on one update and99.2% on two; it
  is inaccurate to call that a whole-tick delay.
- Trail has no kink or state transition at ideal slip12 degrees. At12.5–15.2
  degrees it changes about.792 to.747, reaching its .6 floor at24 degrees.
  It shapes instantaneous magnitude; it does not itself store a delayed force.
- Conditional unavailable-input release and the .5-second output-resume ramp
  are separate. Do not remove them to chase feel without recorded evidence
  that they are repeatedly active. The log alone does not show such a cause.
- P95 .62 neither measures the peak nor proves that clipping is rare enough
  to ignore. Source tyre-force dynamics, sampling/update cadence and physical
  delivery remain possible contributors; none is diagnosed from the summaries.

Next: a verified original owner tape from before driving, strict model replay,
then paired reference trials with comparable corners, rise/release/reversal,
clipping and delivery cadence. The offline candidate is not installed or promoted;
runtime adoption would also need per-row calibration identity and legacy replay.


## First owner F11 tape and the marked sharp turn

The October 7 22:54 owner recording contains 4,197 samples, zero drops, a complete
footer and an F5 marker at 42.629 seconds. The original selected steering and
crash stage replay exactly; all 3,122 recorded driving sends were accepted.
Settings were Strength 50, reference .80, smoothing 0, ideal slip 12, crash 70.

A reusable analyzer lesson: a remembered camera selection is not activation.
iRacing's old check rejected 132 countdown rows although no force/input effect
was active. The owner-requested camera now remains selected across hiding and
is allowed on the grid. The consumer now labels legacy activation unobservable,
keeps force handover errors strict and checks optional explicit activation
against the focused-grid policy. Original bytes and both reports are preserved;
398 tests cover the correction. Never make a tape pass by changing its channels
or suppressing all errors just because the force arithmetic matches.

Across the marked +/-5 seconds: 500 rows, valid active Grip, smoothing 0,
delivery scale 1, no steering clipping, native=requested every row. Recorded
physics steps are 20 ms; driving wall intervals P95 27.76 ms, max 44.14 ms.
At the marker, 73.27 km/h yields 23.11% requested force. No added filter,
unavailable-input release or recovery ramp explains the reported spongy turn;
end-to-end input/motor latency and subjective feel remain unmeasured.

The paired .80-base speed experiment raises that sample only to 24.60%.
Above 60 km/h, duration-weighted RMS rises 33.35% to 37.12%, versus 44.19% for
Art Norway and 23.13% for Art's other owner capture. The lower band is unchanged.
These different workloads cannot establish a universal match. Both steering
trials avoid clipping, but combined crash headroom still needs review.

Keep the current tune and the speed candidate offline while gathering feedback;
do not sell gain as a sharpness correction. Claude approved the offline helper;
any runtime adoption must retain source/tuning identity and replay, handle invalid
speed without throwing, label/update comparison shadows and add no player knob.
Detailed consumer record: docs/reviews/2026-10-07-owner-ffb-marker.md. Frozen private
evidence: SessionEvidence/iracing-owner-080-20261007-225407, source SHA-256
`e56ff9a80b1dd5b97a82e469255f3bf454a5ac62b2030e98eaf3ac46d38b9bf6`.
