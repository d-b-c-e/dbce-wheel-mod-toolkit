# Cadence-aware bounded impact contract (unpublished source candidate)

This additive shared C#/C++ implementation is based on official v0.15.0 source 358add53af220eb95bf606e240b230e23a6ea197. It is not a release, installed consumer pin or accepted physical effect. Existing ImpactMixer.Trigger/Mix/Envelope, ForceShaper, AxleForceCurve@1, native device APIs and profiles remain unchanged. The new CadenceImpactMixer is opt-in; Woden's 710002af stage and Art's installed behavior are preserved.

## Demonstrated defect and correction

The legacy signed 100 ms envelope has a positive lobe ending at 20 ms and a peak at 5 ms. Point sampling at 20 or 30 ms from exact onset misses the positive lobe. Existing source output remains the baseline, including this behavior.

ImpactMixer.EnvelopeIntegral is the exact piecewise signed antiderivative of that unchanged envelope. CadenceImpactMixer averages its integral difference over each completed interval instead of taking one point. For a single normalized event, the first interval crossing onset retains positive demand at 20-1000 Hz across tested phases. Integrated signed area remains zero when the complete isolated event is covered. Intervals up to 50 ms are accepted; coarser, nonadjacent, reversed, duplicate and nonfinite intervals fail closed to zero and clear history. This avoids cancellation of the whole bipolar event into one 100 ms interval.

The demand is retrospective and has up to one accepted interval of calculation latency. The returned command is applied by the consumer at callback time; it is not a reconstruction of the missing 5 ms hardware peak. Interval averaging can reduce absolute area when a window spans both lobes; final slew/clipping also changes delivered command shape. Preserve the raw interval pulse, mixed target and final command separately. No torque, feel or collision equivalence is claimed.

All interval and event timestamps must use the same monotonic output clock. Simulation catch-up time is not an output clock. A consumer must verify any callback timestamp correlation into this clock before enabling event input; preserve the original timestamp for diagnosis. The saved trace records sample elapsed time, not separate native-call timestamps, so the replay does not measure physical device cadence.

## Explicit final budget

Immutable constructor inputs: peakLimit (0..1 nominal command fraction), slewPerSecond (>0), headroom (0..0.5). The tested Woden contract uses the existing .25 peak and 1.5/s slew with .25 reserve. No gain is introduced or changed.

An already normalized structural request is clamped to the selected final peak. While a verified event overlaps an interval, target = structure*(1-reserve) + peak*reserve*clamp(intervalPulse,-1,1). With no verified events, there is no reserve attenuation: target = structure. The target is then capped and slew-limited against the previous final command. This is a force budget in command units, not measured aggregate wheel torque. Independently played native conditions/bursts must not bypass or be counted as part of this budget.

Safety zero on inactive context, invalid input or stale/nonadjacent interval bypasses slew intentionally. Reset clears all event/history state. Event targets end at 100 ms; a slewed final command may have a bounded settling tail (at most peak/slew under zero structure after the event target ends). Continuing structural force can remain, as expected. Do not call this Art's driver-finite 120 ms constant crash effect.

Trigger requires explicit inputVerified=true, finite amplitude in (0,1], explicit direction +/-1, monotonic nonnegative timestamp and an eight-event capacity. Default/unverified input is rejected and clears pending events. Mix defaults impactInputsVerified=false and therefore produces structural-only output. A queued event cannot precede the first processed interval or lie in its future. Eight overlapping events remain bounded; clipped overlap does not promise preservation of unclipped impulse.

## Consumer safety and collision seam

This class makes no device call. Submit its sole final command through the consumer's existing ownership/focus/stock-owner/model/panel/stale/respawn/exit gates and guarded SetForce route. Configure the existing native hold timeout to 150 ms, retain zero-before-window-destruction, and use the existing Zero/Stop path on safety gates. Do not apply a second shaper/gain after the final limiter or independently start another impact family. The new class rejects >50 ms intervals; it does not replace the native watchdog if callbacks stop.

Woden still has no source-validated body-collision hook or normalized event channel. It therefore must keep inputVerified=false and impactInputsVerified=false. Do not infer collision from alignment, wheel-ground-hit, acceleration, load spikes, button input or envelope fixtures. A reviewed adapter must prove selected-player body contact, exact build/source/profile/session/car/reset identity, event identity, callback phase, world normal, relative velocity in verified m/s, nonroad/nonlanding classification, timestamp freshness and native callback ABI before emitting a normalized severity and explicit cue direction. The existing Art normal-speed threshold/severity mapping is a comparison reference, not adopted Woden collision tuning. No Woden collision mapping or output is activated by this candidate.

Art's AxleForceCurve@1 baseline and separate 120 ms positive-X finite crash route are retained. Do not substitute this 100 ms bipolar route into the approved Art effect or compare UI percentages as torque. Consumer integration and any shared toolkit HAT port remain under owner coordination; this v0.15-base candidate does not include the separate v0.12 input override.

## Evidence and baseline deltas

- Managed suite: 93 FFB and 41 telemetry tests, no skips. The eight legacy impact-contract rows and existing force/profile conformance remain unchanged.
- 672 rate/phase/sign/structural-bias scenarios (20/30/50/60/120/250/1000 Hz, 16 phases, both signs, 3 biases), with 60,096 managed/native parity rows on x64 and x86. Cap, final slew, isolated signed area, positive-lobe retention, queue bounds and safety reset are checked.
- Production native WheelFfb.cpp is compiled only into fake-COM regression harnesses. Mixed normalized/quantized commands are checked against +/-2500 units and the quantized slew ceiling. Actual ApplyHoldTimeout boundary is checked at 150 ms and the next millisecond, plus focus loss/all-family shutdown. No native device is opened.
- Source-linked actual Woden ForceSignal re-evaluates all 8,251 model rows with zero preview difference; original source/config/profile hashes are checked. The proposed stricter no-event final limiter using recorded monotonic sample time changes 190 rows: maximum delta .0087415241 (sequence 10801). Simulation-clock comparison changes only sequences 10437 and 10438, maximum .0055755768; this comparison is not the output-clock contract. These are disclosed adoption deltas, not changed installed behavior. No stale-interval safety zeros or collision events occur in either replay.
- Art's 2,947 immutable steering rows still match the actual shared AxleForceCurve@1 within 7.22175598e-8. No separate impact output or physical feel is compared.

The original 710002af package stage remains byte/hash verified. No consumer payload, settings, native DLL, version/profile, display or hardware is updated. Publication will be sequenced by the parent after independent review; no push, binary release or deployment is performed here.

## Reproduction

Run dotnet test Dbce.Wheel.sln -c Release with an optional offline RestoreConfigFile. Build tools/cadence-review; optionally pass WodenSourceRoot pointing to the pure Woden source feature for the source-linked comparison. Run with a NEW evidence directory, immutable Woden case directory and verified Art forces.csv. It generates synthetic managed-native.csv plus evidence.json and refuses overwriting the directory. Private recordings/configs stay outside Git and releases.

Run native/forcemodel/build_cadence.bat x64 <generated CSV> and x86 <generated CSV>, existing build_conformance.bat, and native/wheelffb/test/build_constant_burst_test.bat x64/x86. These fake/pure tests are device-free. Hardware smoke is not run because native DLL source/ABI is unchanged and device operations remain prohibited.
