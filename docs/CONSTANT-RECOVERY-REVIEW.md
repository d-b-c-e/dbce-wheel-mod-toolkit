# Constant-force recovery investigation

[dbce-wheel-mod-toolkit, 2026-10-02] Isolated source candidate from canonical
`d41fe55f98da92488cafab13c59eb1937dc394b7`. Held for independent review.
No game, real DirectInput device, force output, tuning, settings mutation,
deployment, hosted CI or publication occurred.

## Reproduced defects

The existing burst fixture covers finite-event drop/no-replay, focus/watchdog
fake effects, lock publication and worker-drain shutdown. It does not drive
the production constant-force access-loss/recreation path with a fake COM device.
The new fixture compiles that production source, substitutes bounded fake device
and effect interfaces and injects the Win32 tick functions in the test translation
unit only. It initializes no DirectInput instance and opens no device.

On canonical source, 100 constant requests at one tick under persistent INPUTLOST
produced 101 Acquire calls: one in the throttled block and 100 in the later
unthrottled branch. At tick zero, the timestamp sentinel also allowed the main
block on every request (200 Acquire calls total). After both two-axis and
single-axis recreation attempts failed, later requests returned at the null
effect check; the now-accepting fake could never recover (two creation calls).

The final identical test source fails `loss`, `recreate` and `boundaries` on
canonical x64 and x86, and passes all modes on the candidate. Cancellation checks
also pass before/after; they protect deliberate release against the new pending
retry behavior. Before/after evidence is saved outside the repository candidate.

## Bounded source change

Remove the later unthrottled Acquire. Keep access-loss reacquisition in one block
with the existing strict greater-than-250-ms interval. Track attempted state
separately from timestamp so a real tick zero is not an uninitialized sentinel.
Both functional recovery throttles now use GetTickCount64/ULONGLONG, matching the
existing hold/ramp clock. Diagnostic logging counters retain their old clocks.

If self-heal creation fails, mark only that missing effect retryable. Later
fresh caller requests can retry after the existing strict greater-than-1000-ms
heal interval. Failed retries stay bounded. Successful pending retry uses the
existing axis fallback and starts the ramp at zero before applying the current
caller's request. It never replays the earlier rejected magnitude automatically.
Clear pending recovery, throttle state and ramp when the effect is deliberately
released by panic, candidate close or shutdown. No export/version/profile changes.

## Checks and limits

```text
native\wheelffb\test\build_constant_recovery_test.bat x64
native\wheelffb\build\constant_recovery_test.exe loss
native\wheelffb\build\constant_recovery_test.exe recreate
native\wheelffb\build\constant_recovery_test.exe boundaries
native\wheelffb\build\constant_recovery_test.exe cancel
```

Use `x86` and `build\x86\constant_recovery_test.exe` for the 32-bit cases.
The builder only compiles; each explicit mode reports nonzero on assertion failure.
Boundaries cover INPUTLOST, NOTACQUIRED and NOTEXCLUSIVEACQUIRED with both failed
and successful acquisition, 100 persistent requests, 250/251-ms boundaries,
tick zero, crossing the old DWORD boundary, uptime above 2^32 and 2^40 ms, and
a full DWORD-period jump that must not alias the elapsed 64-bit throttle.
Recreation checks both-axis rejection, 1000/1001-ms boundaries, persistent
creation rejection, later single-axis acceptance, zero at pending-retry ramp
start, fresh signed request after ramp and cancellation by panic/shutdown.

Existing burst/no-replay, concurrent publication, delayed-worker drain and
pre-device watchdog cases pass on x64/x86. Candidate DLL builds pass for both
architectures; x64 export/header verification retains 41 exports. The existing
actual-DLL lifecycle fixture passes both architectures (41-export resolution,
empty cleanup, worker restart and 32 unload cycles), without enumeration,
InitDirectInput or force calls. Its existing unused-static-header compiler
warnings remain. New constant fake builds report no compiler warnings.

These results establish bounded software control flow with accepting/rejecting
fakes. They do not establish any driver's behavior, physical force recovery,
game acceptance, timing under hardware contention or installed adoption.
Existing worker-drain and burst test source is unchanged. The offline summary
branches remain separate and held. Review must precede any publication.
