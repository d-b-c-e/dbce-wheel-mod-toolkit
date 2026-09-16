# 0.13.1 local candidate — 2026-09-16

Native 0.6.1 makes per-update force tracing opt-in with
`DBCE_FFB_TRACE_FORCE=1`, retaining default lifecycle/error logs and all 41
exports. No force arithmetic or lifecycle changes. See
[force logging](FORCE-LOGGING.md). Unpublished; reported game hitch causality
is not established. Earlier handoffs below are historical.

Offline package gates pass, including 121 managed tests and both native fake
logging suites. Hardware smoke loads all exports on x64/x86 but cannot proceed:
no FFB device is attached. This candidate has no new hardware verification.

# 0.13.0 local candidate, 2026-09-12

This isolated branch adds finite periodic bursts (native 0.6.0, 41 exports),
all-family focus/watchdog stopping and first-low-magnitude periodic delivery.
See [PERIODIC-BURSTS.md](PERIODIC-BURSTS.md) for exact contracts and limits.
AxleForceCurve@1, ImpactMixer and profile data are unchanged. This candidate is
not published; the official 0.12.0 release and consumer installed RC remain separate.
No GitHub workflows are dispatched. Consumer landing detection/settings/testing
are owned by art-of-sim-rally's task, not this checkout.

Validated locally on 2026-09-12: full package gates passed with 121 managed
tests, 24 sync assertions, 63 binding assertions on each of x64 CoreCLR and
x86 .NET Framework plus 8 legacy-component checks on each. Native fake-effect
tests passed on x64/x86, alongside existing packet/model/proxy/helper suites.
Both native DLLs built with 41 exports; zero-output C ABI smoke passed every
stage on the MOZA R12 in both architectures, including idle burst creation,
zero play and stop. No nonzero burst was applied; physical expiry/feel and
Unity Mono in-game delivery remain unverified. No Actions, push or publication.

The previous release handoff below remains historical context.

# Current handoff

Updated 2026-09-07. Source/package version **0.12.0**, native device component
**0.5.0** (38 additive C exports). Consumer pins remain separate: inspect
`tools/Get-ConsumerStatus.ps1` and `docs/CONSUMERS.md`; never infer adoption from
a toolkit release. [Prior handoff](history/STATE-before-0.12.md) is historical
and includes stale version, directory and deployment-path claims.

## Implemented and verified offline

- Exact-module Cdecl managed binding, including renamed plugins and original
  three-argument signatures. Four-argument initialisation selects an explicit
  nullable GUID. A staged PreferGuid applies to the next initialisation only.
- Strict saved-GUID opening refuses fallback if absent or if the selected
  candidate cannot open. Existing native consumers opt in through
  SetStrictDeviceSelection; legacy native fallback remains available.
- Read slots sharing the FFB handle retain identity. Panic removes managed
  readiness; a new lifecycle restores actuators/panic state. Shutdown releases
  FFB while ShutdownAll also closes readers and enumeration-only DirectInput.
- Transactional sync preflights parts/conflicts, stages all files, preserves
  unselected manifest entries and nested tool paths, and rolls back failed
  commit renames. Force replacement preserves local edits as backups.
- AxleForceCurve compatibility version 1 preserves art of rally's arithmetic
  order. Existing profile data is untouched. Dynamic consumer regression is
  separately owned by art-of-sim-rally.
- Managed/native hysteresis passes direction reversals and honours lowered
  limits. Invalid model/shaper inputs/settings clear state and expose rejection;
  valid input recovers without restarting.
- Consumer setup diagnostics explain selection failures. Delivery diagnostics
  distinguish calculated forces from accepted output API calls.

Full package command passed on 2026-09-07: 121 managed tests (80 FFB, 41 telemetry),
24 real-filesystem transaction assertions, 40 fake-binding assertions each on
x64 CoreCLR and x86 .NET Framework 4.8; MSVC native profile conformance
(10 profiles x 200 samples), event/signal/recovery tests, Forza packet tests,
x86/x64 proxy scaffold tests, PowerShell helpers, both device DLL builds and
x64 export/header checks. GCC recovery invariants also passed locally.

The native implementation was separately exercised by the adoption task on the
MOZA R12 using the corrected x86 zero-output smoke: strict missing-GUID refusal,
constant/periodic/condition zero updates, panic and reinitialisation passed.
Its damper update is now zero unless --force is requested. See
[adoption evidence](ART-OF-RALLY-ADOPTION.md). No nonzero test force was requested.

## Limits that still matter

- .NET Framework is not Mono. Unity 2019 Mono loading and in-game integration
  still need consumer testing. Fake DLL tests do not acquire hardware.
- Native acceptance and zero-output device tests do not prove physical feel.
  OutRun's migrated/profile path and steering signal, iRacing's powered-base
  sign/feel, and Milestone's packets on the wire retain their previous
  verification gaps. Do not call them gameplay-validated.
- An abrupt process termination between sync directory renames can require
  recovery from .toolkit-transaction-*/previous. Caught rollback failures retain
  and report that recovery directory. This is not crash-proof storage.
- The consumer UI improvements described below are toolkit capabilities, not
  automatic UI changes in every consuming product.

## Next work

1. Finish art-of-sim-rally's official release pin and offline dynamic comparison
   in its own task, then attended consumer testing before gameplay claims.
2. Adopt [consumer UX guidance](CONSUMER-EXPERIENCE.md): picker-local status,
   stable identity for input roles, no-force calibration, active tune visibility,
   and reviewable support evidence. Calibration/profile-preview UI remains future work.
3. Resolve OutRun steering by its diagnostic drive before tuning the profile.
   Verify the live install path from the consumer repo; historical handoff paths
   must not be used as deployment authority.
4. Continue the shared input kit and SDL consolidation. native/dinput-proxy
   already contains the tested header scaffold; input/templates remain planned.
