# Offline axle-reference calibration

`SpeedReferenceCalibration.cs` is a small vendorable **diagnostic** helper. It
does not enter the shipping FFB library, open a device, modify a saved tune, or
change `AxleForceCurve@1`. A consumer supplies original tyre inputs and calls
its actual production force law with the helper's reference ratio. Preserve
the source, profile and consumer law hashes in each experiment.

The curve keeps its base ratio through StartKmh, then uses a cubic blend to
BaseRatio / HighSpeedGain at FullKmh. It is continuous with zero endpoint
slopes and adds no temporal state. Apply it before the production clamp/filter;
never multiply a previously clipped or smoothed recording. Signed speed is
treated as magnitude; non-finite speed and invalid profiles are refused.
Missing-input safety handling remains the consumer's responsibility.

The exploratory gain bound is 1..1.5, reference ratio .01..10 and transition
speeds 12..1000 km/h. These validation bounds do not certify physical safety.
The concrete iRacing v1 profile uses .80 / 60 / 100 / 1.25. At 80 km/h it gives
gain 1.125 and ratio about .7111; at 100+ it gives ratio .64. It preserves zero
input, sign and the law's parking fade. Additional steering can reduce crash
headroom even when the independent crash-strength setting is untouched.

The iRacing developer analyzer vendors this source and provides
`ffb-calibration-probe` for synthetic production-law surfaces/transitions, and
`ffb-calibrate` for strict original-capture trials. Neither command changes a
game runtime. Its documentation records the exact calls and evidence limits.

Do not fit force response to whole-drive medians. Use comparable corner/event
inputs, time coverage, rise/release/reversal, saturation and delivery cadence.
The October 7 owner report says strength improved at .80 but force still felt
rounded with smoothing zero; this curve experiments with strength, not a
claimed sharpness fix. There was no original force tape from that owner drive.
