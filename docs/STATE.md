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

## Art RC16 device-picker guidance, 2026-09-23

Art's task reported that expanded FFB device choices and Refresh looked like
full-width command bars. UX-05-D now requires bounded/flat choices, visible
selection, retained but disabled saved-disconnected choices, and compact Refresh
beside the picker. Simple/Advanced guidance and the checklist match. Art's
deployment record identifies installed RC16 source `e854b994125494fb38442a1c41476c42befad934`;
this task read that record, but did not independently test its renderer or devices.
This is guidance only: no toolkit binary change, package bump or consumer repin.

## Art RC14 commands and native-menu handoff, 2026-09-22

The Art task forwarded two further owner-test findings. UX-03-C now covers
compact commands across all pages, and UX-03-H defines a programmatic handoff
that waits only for the actual initiating gesture's release plus neutral frames
before explicitly releasing overlay ownership. It must not wait for all mapped
axes/buttons: resting pedals and held gear buttons can keep them active forever.
Focus/cancellation and ordinary-close gesture suppression remain required.
[Provenance and acceptance](ux-audits/2026-09-22-art-rc14-commands-handoff.md).
This update is documentation only; consumer runtime/interaction fixes stay with
Art's task. No toolkit binary, game or device changes are implied.

## Rendered Art feedback clarification, 2026-09-22

Art's task forwarded owner feedback from the rendered RC13 panel: weak card
contrast, button-like navigation/disclosures, full-width command noise and a
duplicated Setup page. UX-1/UX-03-G now require perceptible boundaries/spacing,
distinct flat compact navigation and compact grouped commands. Setup remains
only for a distinct guided workflow; saved page meanings must survive removal.
See the [audit and provenance](ux-audits/2026-09-22-art-rendered-hierarchy.md).
This is guidance only. Art implements the consumer change in its own task;
the HTML demo remains historical and no toolkit runtime/binary is changed.

## Art visual hierarchy follow-up, 2026-09-21

Owner feedback and an independent review of Art RC12 source/retained screenshots
found ambiguous binding ownership and too little actionable body space in Simple.
[UX-03-G](CONSUMER-UX.md#binding-ownership-and-useful-space--ux-03-g) now specifies
bounded groups, local instructions, clear disclosure controls and useful initial
viewport checks. See the [review](ux-audits/2026-09-21-art-visual-hierarchy.md).
Art's implementation/deployment is recorded in its own review and deployment log;
new live-game acceptance is pending. Other consumers are not newly certified.

## Consumer deployment coordination, 2026-09-19

The owner authorized completing and deploying the seven consumer UX updates.
All seven now have built artifacts and verified local deployments. Consumer
documentation and the Simple/Advanced/readiness/install guidance are updated.
Desktop testing is paused after the user's Escape stop; all games are closed
and temporary test configurations restored. Woden's latest startup guards,
iRacing's final stationary click and attended hardware acceptance remain unverified.
See the [deployment ledger](ux-audits/2026-09-19-deployment.md) for exact package,
installation and verification status. The September 17 build hold has expired;
no live-game slot follows from it. Preserve this checkout's independent runtime
and Recording work. Consumer deployment does not imply a toolkit package bump
or completed attended input/force acceptance.

Updated 2026-09-07. Source/package version **0.12.0**, native device component
**0.5.0** (38 additive C exports). Consumer pins remain separate: inspect
`tools/Get-ConsumerStatus.ps1` and `docs/CONSUMERS.md`; never infer adoption from
a toolkit release. [Prior handoff](history/STATE-before-0.12.md) is historical
and includes stale version, directory and deployment-path claims.

## Consumer UX guidance, 2026-09-16

[UX-01-S](CONSUMER-SETTINGS-VIEWS.md) now requires Simple by default and Advanced
for detailed tuning. The page-placement inventory, first-use flow, migration and
interaction checks are documented in the linked contract and UX checklist.
This is documentation only: no shared renderer, consumer implementation, game
deployment or live validation is implied. Consumers must record their own adoption
evidence; previous audits do not certify this revision.

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
