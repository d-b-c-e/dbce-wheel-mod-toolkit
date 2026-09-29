# F-Zero actuator recovery review — 2026-09-29

F-Zero Deluxe owns `E:\Source\dbce-wheel-mod-toolkit-actuator-fix`, branch
`codex/fix-actuator-rearm`. This review does not merge runtime changes, repin a
consumer or alter the frozen official, POV or external-session candidates.

Reviewed commits:

| Source | Change |
|---|---|
| `43c5e3cda87ce8b4d72f1ee0ceeda2bd3a6b8ebd` | Check Acquire failure; enable actuators on every open |
| `6b6e29bea4386352a65fa905e6d5e2fda1174fdb` | Clear persistent-effect started caches after constant-force recovery; lower condition update threshold |
| `ca24f1e638ecc5b4575457e472b92f85acf19ac3` | Remove preemptive Stop from nonzero periodic burst playback |

The consumer reports an actual impact request at frame 6139 and an old-DLL
`DIERR_NOTDOWNLOADED` response. It attributes the discarded collision to stopping
an idle undownloaded effect. The new DLL is only in its experimental preview;
physical feel remains unverified. The coordinator independently built and ran
the current fake burst/production focus-watchdog suite on x64 and x86: both
passed without opening hardware. The tests do not close the findings below.

## Required corrections before shared acceptance

1. **Preserve shape verification before playback.** Removing Stop for every
   retrigger also changes a still-playing shaped effect. DirectInput may update
   its parameters immediately or restart it during SetParameters; GetParameters
   verification then happens after the new shape can act. Handle the idle
   not-downloaded case without changing a playing effect before verification.
   Add a fake that models on-the-fly updates and implicit restart, including
   mismatched/unsupported shape rejection. Preserve finite expiry, discard on
   failure, sine-only compatibility and no stale retry.
2. **Cover every shared-device reacquire route.** The new cache invalidation is
   called only from SetDeviceForcesXY. Public Aquire and periodic/condition
   creation/recovery also acquire the shared device and can stop sibling effects.
   Audit those paths and test production calls with persistent sibling slots;
   directly calling the cache helper is insufficient. Finite events must not be
   replayed after recovery.
3. **Keep lifecycle fixes separate from update-rate tuning.** The coefficient
   delta threshold changed from 500 to 50 in the recovery commit. Remove that
   unrelated change or give it separate rationale and tests.
4. **Test open/reopen failures through production code.** Verify Acquire and
   actuator-on failures, successful reopens after prior actuator disable, and
   cleanup. A header helper test does not exercise OpenCandidate.

Focused feedback was sent to the owning task, which acknowledged and began the
repairs. Preserve its source ownership and wait for a clean repaired checkpoint
and exact DLL receipt. No new official release or hardware claim is authorized
by this review.

Microsoft documents both [SetParameters playback behavior](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417952(v=vs.85))
and [Start downloading/restarting an effect](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417955(v=vs.85)).
The first finding is an inference from those contracts and the inspected call
order; no physical reproduction was attempted.

[F-Zero Deluxe / dbce-wheel-mod-toolkit coordination, 2026-09-29]
