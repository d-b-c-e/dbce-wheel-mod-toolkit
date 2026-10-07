# Follow a new force model through the whole recording workflow

Woden's October 7 Grip v4 Core replay tests passed, but the stage CLI still
required force-config v1 / Classic3. `ffb-export` called that CLI validator first,
so every new Grip stage was rejected before the new export branch. Consumer
`ab7f1fa` fixes that integration gap. This was caught offline, before treating a
new capture as normalization evidence.

For any consumer adding a force model:

1. Follow config writer, channel producer, manifest identity, strict reader,
   original-model replay, stage validator and actual export CLI. Passing one
   layer does not qualify the rest. Keep older model/config pairs accepted and
   unknown or mismatched versions refused.
2. A reference load may have been learned before recording began. Restore its
   recorded value and exact qualification state on each analysis row; relearning
   from the shortened tape produces a different request. Reject missing,
   nonfinite, fractional or unsupported state. Known zero is distinct from
   unavailable, provisional and qualified reference.
3. Independent muted analysis needs its own matching initial/reset history.
   Do not substitute the physically gated preview, which may remain zero.
   Preserve model, tune, car and curve identity; split stage cases on a changed
   car/tune rather than silently bridging histories.
4. Test a complete sealed fixture through the real writers, validator and CLI,
   including pre-capture reference, reset/discontinuity, a curve change, invalid
   state, output-attempt refusal and exclusive output. Recheck an untouched
   original legacy reference too. Synthetic evidence must keep its provenance
   in reports instead of being described as original live physics.
5. Map normalization to `eligible`, not merely model/car validity. Missing and
   provisional references must not count as quiet-force samples. A steering
   pass does not certify ordered crash replay, effect delivery or wheel torque.
   Kinematic playback is route/presentation evidence, not a new live tyre-force
   baseline.

Woden evidence: full source51/37957, UI669, stage3267, geometry22 and
envelope10836; original Kenya3601 rows with zero model error and six corrupted
copy refusals; synthetic Grip160 rows,159 model-valid,139 eligible, zero error.
The separate frozen force-controller review passes59/37973. No installed
settings, force model arithmetic, native pin or physical device changed in the
stage fix. Real-curve capture and attended Art-at-50 comparison remain pending.
