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

## Findings at the original checkpoint

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

## Repaired source review

Clean `02fb715531f8c857f5d66c89382da3c724fde7d8`, tree
`d224cba3e59f87c0a8680394cf8f7cb2d02900ec`, follows burst repair `44ca79d`.
Stop again precedes parameter changes, with idle `DIERR_NOTDOWNLOADED` tolerated;
the test models implicit restart and rejects a mismatched shape before playback.
All inspected FFB acquisition/recovery paths use the shared cache invalidation
helper; finite events are not restarted. The threshold is restored to 500.
Production fake-COM tests cover public Acquire, constant/periodic/condition
recovery, creation Start retry and open failures/success.

The coordinator independently built and ran burst and production POV/recovery/open
suites on x64 and x86: all four passed. This closes the targeted source findings.
The following existing DLL hashes were independently checked without rebuilding
the DLLs:

- x64: `F2D9DD3059A75ED381446C30488F5B75670642AF8972F2A2D593F983B9BE7972`.
- x86: `DB9BAC4EE11A8DCCA0C6F43185C1060292F84C8E4448E699DC14F2FE61FDEE5A`.

F-Zero committed its component receipt at `ea25ea8`,
`docs/WHEELFFB_EXPERIMENTAL_RECEIPT.md`. It records the clean source/tree above,
native 0.9.0 (version integer 900), 47 exports on both architectures, fake-suite
builds and smoke compilation only. The coordinator rechecked the clean native
source/tree and both DLL hashes, including the exact x64 rig-preview copy.
The prior preview DLL hash is retained in the receipt; that old binary was not
saved separately. Normal installed Stream Deck/LaunchBox files were not replaced.
This closes the requested experimental component receipt review. No device was
opened in coordinator validation. Shared release/adoption and physical feel are
not established by this source review.

Microsoft documents both [SetParameters playback behavior](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417952(v=vs.85))
and [Start downloading/restarting an effect](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417955(v=vs.85)).
The first finding is an inference from those contracts and the inspected call
order; no physical reproduction was attempted.

[F-Zero Deluxe / dbce-wheel-mod-toolkit coordination, 2026-09-29]
