# Finite periodic bursts (0.13.0 local candidate)

Native component 0.6.0 adds three exports (41 total). This candidate has not been
published. Existing continuous effects, AxleForceCurve@1, ImpactMixer envelopes,
and named profile data retain their contracts. Landing detection and event
settings belong to the consumer.

## API

| Managed call | Return and contract |
|---|---|
| `CreatePeriodicBurst(int freqHz, int durationMs)` | Slot or -1. Creates an idle, zero-magnitude sine; never starts during creation. Frequency 1..100 Hz, duration 1..500 ms. |
| `PlayPeriodicBurst(int slot, float magnitude01, float freqHz)` | True only if accepted. Explicitly stops/reconfigures/starts one iteration, restarting the finite duration even for an identical event. Magnitude finite 0..1, frequency finite 1..100 Hz. Zero means stop. |
| `StopPeriodicBurst(int slot)` | True for a successfully stopped valid burst slot, including one already idle. Retains slot. |
| `SupportsPeriodicBursts` | All three optional native exports exist. Capability only, not proof the wheel supports sine effects. |

Native C names match the managed names, except Play takes integer magnitude
0..10000 and frequency in millihertz (1000..100000). All returns are 4-byte ints;
all functions use Cdecl. Create/play require an initialized device. The slots
share the existing four-slot periodic pool; ReleasePeriodicEffects/managed
ReleasePeriodics releases all continuous and burst slots. Continuous Update
rejects burst slots; Burst Play/Stop reject continuous slots.

Every positive managed magnitude maps to at least one device unit. Burst play
has no envelope-change gate; 5%, 0.2% and one-unit output reach the effect API.
Continuous effects retain their existing change gate except that their first
positive value (or first value after stopping) now always reaches the driver.

## Lifetime and focus

The DIEFFECT duration is finite microseconds at creation and each play. Start
uses one iteration. The DirectInput effect expires without a subsequent game
callback, including when the managed game loop stalls. There is no pending event
queue or software countdown responsible for stopping it.

Play attempts once. Failed stop/parameter/start operations release the handle
and discard that event; there is no acquire/retry that can replay a landing
later. A genuinely new event may recreate a silent handle before its own play.
Consumers must not retry rejected landing events. Unsupported devices and invalid
arguments report failure/LastError instead of falling back to constant force.

Nonzero Play requires this process to own the foreground window. Install the
existing exit guards after FFB initialization: WM_ACTIVATEAPP loss now stops all
periodic and condition effects as well as zeroing/stopping constant force.
Regaining focus never plays a burst. ZeroForces stops burst handles without
releasing their slots. The hold watchdog now sees active periodics/conditions
when constant force is zero and stops all families on timeout. Periodic and
condition updates count as heartbeat calls; finite duration remains independent
of that watchdog or frame traffic.

## Consumer example

```csharp
// After Initialize succeeds; install guards on the window thread.
WheelFfbNative.ExitGuards();
int slot = WheelFfbNative.SupportsPeriodicBursts
    ? WheelFfbNative.CreatePeriodicBurst(25, 120) : -1;
// On ONE validated landing transition, when opted in by the player:
bool accepted = slot >= 0 && WheelFfbNative.PlayPeriodicBurst(slot, .05f, 25f);
// Record the result. Do not enqueue/retry this event after a failure.
// On pause/menu/focus reset, stop this event and clear game-side detection state:
if (slot >= 0) WheelFfbNative.StopPeriodicBurst(slot);
```

The example duration spans three sine cycles at 25 Hz. This is an integration
example, not a physically validated tune or a profile change. Keep magnitude,
opt-in state, landing threshold/cooldown, pause and teleport detection in the
consumer. A rejected burst should leave steering and input usable.

## Local validation and remaining limits

- The native fake-effect harness includes the production adapter, calls its
  actual focus-loss and watchdog paths with fake periodic/condition effects,
  and tests a zero constant force with active other effects.
- Fake driver deadlines at 1/50/500 ms expire without toolkit callbacks. Tests
  cover restart, zero/idempotent stop, unsupported creation, invalid bounds,
  six HRESULT failure classes, and low-magnitude initial updates.
- The binding fixture checks magnitude/frequency/duration marshalling and rejects
  invalid calls before the native boundary. A separate old-component fixture
  proves missing optional exports do not break legacy loading.
- Fake tests prove requested parameters and control flow; they cannot prove a
  physical wheel's finite-duration compliance, waveform, force feel or strength.
  Unity Mono in-game behavior remains a consumer test, not an implication of
  passing .NET Framework fixture tests.
- No GitHub workflows or publication are required to run the local package gates.
  No nonzero physical-force test is part of these gates.

Local execution on 2026-09-12: full package gates passed (121 managed tests;
24 sync assertions; 63 new-component and 8 legacy-component binding assertions
per runtime; native fake effects x64/x86 plus all existing conformance suites).
MOZA R12 zero-output ABI smoke passed every stage on x86 and x64, including idle
burst creation, zero play, stop and panic recovery. No nonzero burst was played.
