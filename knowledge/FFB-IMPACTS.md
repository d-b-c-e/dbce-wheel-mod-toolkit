# Short impact effects: evidence and consumer guidance

## MOZA R12 attended comparison

[art-of-sim-rally + dbce-wheel-mod-toolkit, 2026-09-16] The owner compared
120 ms effects in an explicitly operated standalone tester: A, finite constant;
B, shaped 6.25 Hz sine with phase 9000 and 120 ms fade; C, B with ordinary zero
steering updates approximately every 20 ms. Calls returned S_OK, finite duration
readbacks matched, and observed status included PLAYING. The owner preferred A,
but reported 20% as slight and 40% as weak. This does not establish a satisfactory
crash tune or measured torque. The later tester default 50% and manual ceiling
100% were requested for further attended comparison, not validated recommendations.

[art-of-sim-rally + dbce-wheel-mod-toolkit, 2026-09-16] C retained PLAYING across
the observed zero updates (three per 40% C trial). This weakens the hypothesis
that each zero steering update cancels the periodic effect; status alone cannot
prove actual motor output. GetForceFeedbackState returned E_FAIL, so its flags
cannot support any hardware-state conclusion. Device FFGAIN and effect gain
read as 10000; these do not describe the complete Pit House configuration.

[art-of-sim-rally + dbce-wheel-mod-toolkit, 2026-09-16] Frozen consumer evidence
uses local date September 16 (September 17 UTC). Under art-of-sim-rally/results:
`attended-effects-first-comparison-20260917/owner-comparison.log`, SHA256
`d123e4114e346572aac9367ee63afe433390797ff0d247300e5191bc9fd65eba`;
`attended-effects-40-percent-20260917/owner-comparison.log`, SHA256
`2f2edd103a359f6d33928f49d2f8bef08f4afd28a917c67d3e954ed79b638d0a`.
These are frozen copies, not mutable AppData logs. The consumer receipts retain
owner interaction evidence. Later close-time lines in a live log do not change
these snapshot hashes.

## Applying the findings elsewhere

[dbce-wheel-mod-toolkit, 2026-09-16] Distinguish driver acceptance, PLAYING status,
and owner-perceived output in diagnostics. Record exact waveform, phase, fade,
duration, magnitude, axis count, gain, HRESULTs and call latency. Equal nominal
amplitudes with different waveforms and fades need not have equal impulse or feel.
Neither a successful API call nor a comparison with another waveform proves the
cause of a weak effect. Preserve failed diagnostic queries as failures.

[dbce-wheel-mod-toolkit, 2026-09-16] Use finite driver effects for impacts, with
explicit idle creation, stop-before-replacement and lifecycle cleanup. Keep
steering independent and do not use SOLO for layered impacts. Arbitration belongs
at the game-event boundary: one active logical impact avoids uncontrolled overlap.
Separate crash and landing controls, retain existing values, and disable only the
unsupported family when creation fails. Never treat a failed stop as success.

[dbce-wheel-mod-toolkit, 2026-09-16] The new constant API caches a zero effect and
uses SetParameters before Start; tester A created its requested magnitude directly.
Production hardware equivalence remains unverified. Before adoption claims,
record the actual consumer pin, device and driver settings, precise runtime path,
foreground/focus/exit behavior, concurrent steering, and attended strength results.
Do not copy R12 strength values to other bases or change existing profiles based
on this comparison. Full nominal command plus steering is not physical headroom.
