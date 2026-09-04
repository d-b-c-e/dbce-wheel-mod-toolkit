# Designing the force signal

The device layer can be perfect and the wheel still feel wrong: the signal
decides. Provenance in brackets.

## Choose the source signal before the model

- **Aligning torque from a Pacejka tyre model reverses sign past the slip
  peak.** art of rally's `Wheel.Mz` (1989 Pacejka `CalcAligningForce`) peaks at
  a few degrees and goes negative past ~8°; that game's front tyres run 12–29°
  in ordinary corners. Fed straight to the wheel it centred at small slip and
  pushed *outward* mid-corner — "there is no centre", and at 60 km/h+ it was
  centring only 42% of the time. It was also tiny (|Mz| ≤ 15). Nothing built on
  top of that signal could have felt right. `[art-of-sim-rally, 2026-09-02]`
- **Lateral tyre force × pneumatic trail** is what sims use and what shipped:
  `(FyL + FyR) × trail / reference`, trail falling from 1.0 at zero slip to 0.6
  at twice the ideal slip angle. Large, follows the steering 98–100% of the
  time above 30 km/h, saturates without reversing, keeps the "lightening" cue.
- **Slip × load** is a cheap proxy for lateral force when the tyre model does
  not expose forces. It has the same shape below saturation. Check it for
  reversal before trusting it. `[iracing-arcade-wheel, unverified at the base]`
- **The game's own motor output**, when the game had FFB hardware (arcade
  boards): decode the byte, do not model. Cruis'n USA's is a *damper* — a kick
  opposite to and proportional to wheel velocity decaying in ~100 ms, no static
  force at any parked position; Cruis'n World adds sustained ±126 holds;
  Exotica is a centring spring. Measured, not assumed. `[cruisn-collection, 2026-08]`
- **Reuse the game's own tables** where they exist: OutRun's Xbox build carries
  a surface vibration LUT (asphalt 0, sand 0.25, grass 0.7, rough 0.85–0.9,
  water 0.7–0.8) — better than invented coefficients. `[OutRun2006Tweaks-FFB]`

## Validate the inputs, every session

Signals lie silently. Two cases: the reversal above, and OutRun's steer input
(`field_1D0`) reading zero through corners, which left the spring and damper
terms of a whole redesign producing nothing while the lateral term did all the
work — recorded as a telemetry bug, its FFB consequence unnoticed.
`[OutRun2006Tweaks-FFB commit 2d2a828]`

The toolkit's `ForceModel` therefore carries **input validators**:
- liveness — an input that never leaves zero while the car is turning;
- range — an input pinned at its clamp;
- sign by speed band — `sign(force)` vs `sign(steer)` counted per speed band
  (the 5 Hz `FFB trace` line that found the Pacejka reversal).

## A model that carries over between games

Every synthetic-FFB implementation converges on the same shape; OutRun's
"center-out" model is the fullest instance in the family:

```
F_struct = (F_spring + F_lat) * loadMod + F_damper
F_spring = -steer * SpringStrength * speedCurve(speed)        # virtual centring
F_damper = -steerRate * DamperStrength * (0.4 + 0.6 * speedNorm)
F_lat    = latNorm * speedNorm * SteeringWeight * gripFactor  # lateral load
gripFactor = 1 - GripLoss * driftAmount                        # LIGHTENS in a drift
loadMod  = 1 + clamp(-longAccel * WeightTransfer, -0.20, +0.30)
+ events: crash impulse, gear-shift double pulse, surface texture (periodics)
```
`[OutRun2006Tweaks-FFB, outrun2006-redux docs/FFB-REDESIGN-FABLE.md]`

What is game-specific: where each input comes from, the normalisers (OutRun's
24.0 lateral, 90 m/s speed scale) and the tuning constants. What carries: the
terms, the parameter vocabulary, the presets as starting points (`Arcade` =
OutRun's constants, `SimLite` = lateral force × trail), and everything below.

## Conditioning chain (order matters)

1. **Deadzone** — subtractive (1.5 of 10000), not a hard clip.
2. **Dual-rate EMA** — attack 0.25 / decay 0.20; single-rate either lags the
   build-up or lets the release snap.
3. **Soft saturation** — `tanh`, not a hard clamp; the clamp is where the
   "wall" feel comes from.
4. **Slew limiter** — structural terms only; **events bypass it** or a crash
   becomes a shove.
5. **Output deadband** — ~15 LSB; stops the base buzzing at rest.
6. **Warm-up ramp** — 30 frames on start; **ramp from zero over ~250 ms after
   any effect recreation** — stepping back to the requested magnitude is the
   felt jerk, and a slew limiter cannot help because its state never saw zero.
7. **Low-speed fade** — 0 at 3 km/h, full at 12 km/h. Aligning torque is
   meaningless at parking speed (slip angle = atan(lateral/forward) with a tiny
   denominator); real cars have none either. `[art-of-sim-rally]`
8. **Strength is the only user dial.** Two dials for one sensation — a
   reference that lowers to strengthen and a gain that raises — confused
   everyone who met it. Keep the reference internal, tuned from a measured
   peak (art of rally: 6,000–7,000 N at a hard corner; 6,000 and 8,000 were both
   too strong at Strength 50 on a MOZA R12 → 11,500 reference, 2026-09-03).
   `[art-of-sim-rally]`

## Texture and events

- **Synthesised vibration through 60 Hz constant-force updates dies**: ~26%
  zero-order-hold loss at 25 Hz plus driver smoothing plus 30–60% under `tanh`
  when riding on steering load. Use hardware periodics (`GUID_Sine`) for road
  texture and slip; if you must synthesise, **cap at 15 Hz** and inject after
  the compressor. `[OutRun2006Tweaks-FFB]`
- **Gear-shift jolt is a symmetric ±K double pulse**; a one-sided kick reads
  as the game yanking the wheel. Real shift jolt is longitudinal.
- **Crash direction from pre-crash lateral history** (~8 frames back); the
  collision response corrupts the instantaneous signal. Reset the lateral EMA
  after a crash or the wheel stays pinned. Wall hits need a ~100 ms window on
  deceleration, not a per-frame delta.
- **Wheel lightens as grip is lost.** Lateral slide as the whole force
  inverts the real relationship (heaviest exactly when grip went).
- Near-linear speed scaling reads arcade; speed-squared reads sim.
- **Impact intensity from an audio engine is often binary** (Gravel: median
  1.000 over 368 impacts) — the collision *speed* is the discriminator.
  `[milestone-wheel-tools]`

## Strong bases and unstable loops

A damper-type force on a low-friction 8–12 Nm base is a control loop with
the human in it, and it oscillates. The ladder that works: strength 30–40%,
then a **slew limit** on the game's output, then a **peak clamp** — small road
forces untouched, big ones capped. Cruis'n's `ffb_trace_report` counts
direction reversals in a 10 s window and prints a verdict with that
prescription. `[cruisn-collection, 2026-08]`

## Measurement discipline

- Measure the sign on the real base and write the measurement in the code
  comment next to the sign.
- Step tests: ±32/64/112 counts should give proportional peaks; log them.
- "Every wrong turn in this project came from inferring a value that could
  have been read from a log." `[milestone-wheel-tools CLAUDE.md]`
- Four decimal places on any "bit-exact" or "matches" claim. `[cruisn-collection]`
