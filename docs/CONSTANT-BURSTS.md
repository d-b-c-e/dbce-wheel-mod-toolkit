# Finite constant bursts - 0.15.0 candidate

Native 0.8.0 adds four optional exports (46 total). These effects occupy an
independent four-slot pool; ordinary steering and periodic slots remain separate.
This candidate has offline verification only. Physical equivalence to the
attended diagnostic's A effect has not been established.

| Managed API | Contract |
|---|---|
| `SupportsConstantBursts` | All four optional exports are bound to the loaded module. Older DLLs remain usable. |
| `CreateConstantBurst(int durationMs)` | Allocate an idle zero-magnitude slot, duration 1..500 ms; returns 0..3 or -1. |
| `PlayConstantBurst(int slot, float signedMagnitude)` | Finite value in [-1,+1]; zero stops, nonzero values round to at least one signed native unit. Returns success/failure. |
| `StopConstantBurst(int slot)` | Stop a valid slot; repeatable, preserves allocation. Returns success/failure. |
| `ReleaseConstantBursts()` | Stop/release the entire constant pool. |

Native names match; Play takes an integer in [-10000,+10000]. Encoding uses
signed magnitude and Cartesian direction `{abs(magnitude),0}`, matching the
device's established one- or two-axis effect layout. Effect gain is 10000;
device gain is not changed. Play stops the old event, writes direction, magnitude
and finite duration, requires exact `S_OK` and an exact duration readback, then
calls `Start(1,0)`. It neither uses SOLO nor reacquires/retries a failed event.
Failure releases the effect handle while retaining slot metadata for a later
new event. Repeated identical amplitudes restart the finite event.

ZeroForces, focus loss, hold timeout, panic, close and shutdown include this pool.
Ordinary steering updates do not stop it. A driver-finite duration is essential:
the effect must expire even if the game's next callback never arrives.

Consumers should explicitly opt in, expose crash strength separately from
landing strength, and persist both without silently resetting existing values.
Use one logical active impact: arbitrate competing events, stop the previous
family before replacing it, and expose failures rather than silently claiming
the new event played. Unsupported constant effects must not disable working
landing effects. A failed stop warrants invalidating impact output until reset.

The attended candidate offers default 50% and a manual 100% ceiling. These are
owner-requested command levels, not portable feel recommendations. A 100%
constant plus steering can mix or clip in the driver; it does not establish
physical headroom. Keep game event detection and tuning in consumer repositories.

The diagnostic A path creates at requested magnitude before Start; this runtime
creates at zero and updates with SetParameters before Start. Offline fakes verify
the latter sequence, not its physical equivalence on MOZA or any other device.
See [impact evidence](../knowledge/FFB-IMPACTS.md) for attended findings and limits.
