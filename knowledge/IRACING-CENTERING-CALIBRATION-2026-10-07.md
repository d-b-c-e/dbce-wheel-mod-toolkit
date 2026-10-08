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
