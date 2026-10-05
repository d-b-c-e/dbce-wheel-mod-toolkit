# Current source checkpoint

## Recording/playback operator handoff — 2026-10-04

[Runbook](RECORDING-PLAYBACK-RUNBOOK.md) gives exact commands and recovery;
[postmortem](../knowledge/RECORDING-PLAYBACK-LESSONS.md) records failed approaches
and their evidence. Art's full original recording completed 13,455 poses and
received owner visual acceptance. Woden/DRIVE/iRacing trajectory candidates still
require live qualification at this checkpoint. These documentation additions
do not change native output, consumer pins or default-branch release status.
Shared playback code is separately retained on `codex/session-playback-core`
(runtime 0.2 source `50b6b67`, 54 offline assertions); Art intentionally uses 0.1.

## Source and organization checkpoint, 2026-10-02

This record supersedes the source/build/publication statements in the dated
handoffs below; it does not upgrade their game, display or physical acceptance.
Shared wheel master is `80ab49031b1968f930fdea1887ae07eed86beaab`, following
`af938a9055782cc9f8e592cf17b055e7e4544fa0`. Native and managed hosted checks
passed on master (run [36985039001](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/actions/runs/36985039001))
and the reviewed branch (run 36984995628). Its three checkpoint-copy additions
are tests/documentation only: x86 and x64 each passed 264 continuations and
462 rejected transactional restores across 11 profiles, plus the existing
conformance/events/contracts/recovery checks. These are trusted single-threaded
same-process copies, not a public API, external codec or runtime adoption.
[Assessment and limits](CHECKPOINT-COPY-ASSESSMENT.md).

The published `v0.15.0` source remains
`358add53af220eb95bf606e240b230e23a6ea197` (native 0.8.0/46 exports).
Current master still has native 0.6.0/41 exports and managed defaults 0.1.0.
Default-branch promotion neither rebuilt that release nor repinned consumers.

The [shared repository checklist](SHARED-REPOSITORY-CHECKLIST.md) now records
the qualified triple-toolkit source, promoted VR/TSO portable-builder source,
local unreleased preflight archives and exact consumer source checkpoints.
The [consumer register](CONSUMERS.md) retains separate component ownership.
Historical statements that Sonic source is unpublished, replay approval is
awaited, or app packaging profiles are absent must not be used as current gates.
The prior replay approval is resolved; F-Zero candidate `14ea520` publication
remains blocked and GPU validation remains pending, as reported by its coordinator.
No additional runtime, ROM, device or release permission follows.

Historical wheel/optimizer owner changes, the triple-toolkit test edit and VR
local artifacts remain outside these isolated promotions. Source publication
does not prove a live checkout was synchronized or its old owner handed off.
Existing weekly coordination is retained.

---

# External-session experimental candidate - 2026-09-28

Local candidate **0.16.0-external.1**, native **0.10.0 / 55 exports**, branches
from preserved POV source `397a5f22af38b2459f18ffa2e1d685ee838cce8f` on
`codex/external-session-ffb`. It adds an optional dedicated-owner finite constant
API with pinned game/client/producer identity, source/client/input leases,
explicit rearm, independently revoked policy, serialized driver cleanup, and
sticky legacy force blocking. Ordinary consumers retain their existing guards.
Read [the complete contract](EXTERNAL-FFB.md) before binding it.

The coordinator reviewed the ABI and lifecycle, including same-thread driver
reentry, baseline autocentre restoration, copied caller frames, finite scheduling
readback, and retained cleanup errors. Source and candidate verification are
recorded separately: `dist/EXTERNAL-HANDOFF.json` identifies the exact clean
source commit, all payload hashes and final gate results; `dist/external-evidence`
retains the gate logs. Preserved `0.16.0-pov.1` artifacts/receipt are unchanged.

**OFFLINE ONLY. Hardware smoke compiled, not run.** No controller enumeration,
acquisition, shown window, game, virtual pad, input injection or real torque is
authorized for this assignment. Package gates use `-Offline` to skip discovery.
Native cancellation results do not prove physical stopping; a driver may block
or ignore finite duration. OEM autocentre is restored to its captured baseline
on orderly Close and may itself exert torque. A real game-state producer and
attended hardware/consumer acceptance remain outstanding. No official release,
consumer deployment or completed CrossWorlds FFB claim is made.

---
# POV input candidate - 2026-09-28

Local candidate **0.16.0-pov.1**, native **0.9.0 / 47 exports**, from exact
`v0.15.0` source `358add53af220eb95bf606e240b230e23a6ea197`. This isolated
branch adds coherent axes/buttons/four-POV snapshots and optional managed
binding. It is not an official release, consumer deployment or hardware claim.
Read [POV contract](POV-INPUT.md) before integration: output must be gated on
read validity, and both enabling and disabling FFB require deliberate reader
ownership transitions. The old read export and force lifecycle are unchanged.

Offline package gates passed: 121 managed tests; current and legacy native
binding fixtures on x64 CoreCLR and x86 .NET Framework; production POV fake
COM device tests on x64/x86; existing burst/logging/constant fake effects,
24 sync assertions, packet/model/proxy conformance and PowerShell helpers.
Both DLLs expose all 47 undecorated exports and match the C header. The package
helper enumerated controller identities only; it did not read/acquire devices.
Final local package identity and gate logs are recorded in ignored
`dist/POV-HANDOFF.json` and `dist/pov-evidence/` in this worktree.

**Hardware smoke compiled only on x64/x86; NOT RUN.** The standing desktop
pause remains in force. No game launch, input injection or torque output was
performed. Next: parent review and explicit consumer adoption; then coordinate
an attended zero-output smoke/hat-direction/disconnect/FFB-toggle check before
claiming physical wheel support. Current default-branch UX guidance is separate
from this runtime branch; older handoff/candidate/adoption entries below remain
historical, including the original pre-publication wording for v0.15.0.

---

Offline candidate gates passed: 121 managed tests; 104+9+8+8 binding assertions
per runtime; sync and native conformance/fake-effect suites on x64/x86.
46 exports verified. Smoke harnesses compile only; hardware execution not run.

# Release handoff, 2026-09-27

Official **v0.15.0** now publishes the original frozen archive from
`358add53af220eb95bf606e240b230e23a6ea197` (native 0.8.0, 46 exports).
No binaries were rebuilt. Art's actual sync script passed against draft and
published assets in an isolated fixture; all five DLL/XML files matched its
local pin byte-for-byte. The Art task owns its real repin and final release.
See [artifact hashes, source identity and validation](RELEASE-0.15.0.md).

The runtime source is on `codex/finite-constant-bursts`, separate from this
documentation branch and the main workspace's independent runtime/Recording work.
Do not infer a source merge or other consumers' adoption. Fresh 121 managed
tests, native fake-effect checks and 46-export/version checks passed on both
architectures. No device was opened; this candidate's hardware smoke remains
compiled-only. The candidate snapshot immediately below is historical.

# 0.15.0 local candidate - 2026-09-16

Independent finite constant impacts, native 0.8.0 / 46 exports.
See [contract](CONSTANT-BURSTS.md) and [attended evidence](../knowledge/FFB-IMPACTS.md).
The diagnostic constant was preferred but weak at 40%; production SetParameters
path remains physically unverified. No public release or consumer adoption claim.
Earlier entries below are historical.

Offline shaped candidate gates passed:121 managed tests;80+9+8 binding
assertions per runtime; native shaping/logging tests x64/x86;42exports.
Smoke harnesses compiled only. No physical shape validation.

# 0.14.0 local candidate - 2026-09-16

Additive shaped finite sine API (native0.7.0,42exports) based on frozen
logging candidate82c7891. Strict parameter echo, phase and finite fade, with
explicit legacy envelope clearing. No public release or hardware launch.
See [SHAPED-BURSTS.md](SHAPED-BURSTS.md). Earlier entries below are historical.

# 0.13.1 local candidate — 2026-09-16

Native 0.6.1 makes per-update force tracing opt-in with
`DBCE_FFB_TRACE_FORCE=1`, retaining default lifecycle/error logs and all 41
exports. No force arithmetic or lifecycle changes. See
[force logging](FORCE-LOGGING.md). Unpublished; reported game hitch causality
is not established. Earlier handoffs below are historical.

Offline package gates pass, including 121 managed tests and both native fake
logging suites. Hardware smoke loads all exports on x64/x86 but cannot proceed:
no FFB device is attached. This candidate has no new hardware verification.
# Historical 0.13.0 candidate snapshot, 2026-09-12

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

# Historical coordination handoffs

## Shared recorded playback and Woden readiness, 2026-09-28

The owner authorized coordination with F-Zero Deluxe followed by Woden capture
readiness. Source-only [v1 tooling](RECORDED-PLAYBACK.md) adds case/artifact identity,
bounded no-device force observations and comparison. ROM-free tests: 16 passed,
one symlink privilege skip. This does not change native/Recording package pins.
Woden **0.2.12 / `bf58042` is installed** for the next seated recording. Source
review and 37 suites / 965 assertions cover immutable tuning trials, strict
comparison and controller reset/gate transitions. The coordinator verified the
package, all nine deployed payloads, backup and four unchanged owner files.
No expiring request exists; the owner command creates it only when invoked.
The shared synthetic trial reports 77 changed magnitudes on the same 100-request
timeline. Real driving capture and physical acceptance remain pending.
See [owner command, identities and limits](research/2026-09-28-recorded-playback-coordination.md).

## Sonic CrossWorlds project kickoff, 2026-09-28

The owner authorized a dedicated Sonic task and ongoing toolkit coordination
for wheel input, FFB, meaningful telemetry and right-stick-to-hat bindings.
[Task identity, installed-build evidence and acceptance scope](research/2026-09-28-sonic-crossworlds-kickoff.md).
The implementation task owns `E:\Source\sonic-crossworlds-wheel`; an hourly
heartbeat reviews milestones. This is research, not verified consumer adoption.
Consumer repair source `a41a9d1a801487f30af45d0e0360916579ff291a` passes all
83 offline tests in a coordinator rerun. Source review closes target-exit input
gating, old-heartbeat-before-renewal and immutable settings/device snapshot gaps.
Adapter repair `d142721` passes 14 optional and 83 standard tests in coordinator
reruns. Retained authenticated client-pipe liveness closes the reproduced exit
race; pre-client samples are consumed and fake expiry now observes old deadlines.
The owner resolved the source-access prerequisite. Local consumer `122336a`
co-builds UE4SS and its field-free C++ smoke mod with recorded compatibility
fixes. Coordinator checks confirm source/submodule pins, paired DLL and patch
hashes, x64 imports and lifecycle exports; no DLL was loaded or rebuilt here.
This source checkpoint is local and unpushed, distinct from package `7347442`.
Lifecycle diagnostic source `afd6b53` and receipt `f18432f` preserve a separate
loader/mod/proxy stage. Coordinator checks pass all 15 frozen file hashes and
source identity; no ProcessEvent hook is used because callback detachment before
mod unload is unsafe in this pin. The game-only staging/rollback plan is reviewed.
Helper repair `29d1f7c` passes independent preflight and exact output-path/hash
checks for both build trees with archived copies present. Frozen DLLs were not
rebuilt. An owner-authorized game-only slot is now recorded at consumer `c047dd7`.
The mod emitted one startup QPC/thread record, but UE4SS failed its default
EngineVersion/FText scan after 34 attempts, before Unreal initialization.
The coordinator verified the preserved log hash, staged-file receipt consistency,
unchanged game executable, absence of owned paths and no remaining related process.
Compatibility review `6710503` supports UE 5.4 and upstream candidate `6c26f038`,
whose pinned source no longer requires the failed FText constructor scan.
The coordinator verified the interview, local manifest, source pins, lifecycle
interface and changed proxy layout. Candidate source `60b500b` / receipt `d52d08c`
now co-builds the newer loader/proxy/field-free mod. Independent checks pass the
stage preflight, 149 proxy exports, four project inputs, five payloads and six
log/patch/settings hashes. Handoff repair `7e0599e` / receipt `e933045` now passes
independent Stock/UE54 checks, all 12 frozen evidence files, four repair inputs,
both five-file variants and altered-manifest/loader-override rejection. Original
DLLs and receipt are unchanged. The accepted second-slot plan selects UE54 and
requires actual module/config/mod paths plus effective arguments before runtime
acceptance. Offline review is complete; the heartbeat is paused for an owner-
authorized game-only slot. The newer candidate is not game-verified.
Consumer source remains local/unpushed. Game-thread delivery, real driving fields,
usable input and physical FFB remain unverified. The limited loader start does not
establish UE4SS compatibility or grant device-testing authorization.
Native external-session source
`06340b38ec5549ee3fa477668da097e559e61c6e` and its local candidate receipt are
reviewed for offline integration; all 66 ZIP payload hashes were independently
verified. No consumer repin, game input, real telemetry or physical FFB acceptance.
Read-only inspection/offline work can proceed; the shared-desktop pause persists.

F-Zero adapter `d84c60d` produces shared observations from its reported matching
11,364-frame replay. Independent artifact/comparison checks confirm 45,461 aligned
requests and 29,083 differences between two tunes. Snapshot-path binding is fixed;
four ROM-free adapter tests pass independently. Capture provenance is documented
separately from the observation runner. Shared playback/Woden coordination is
complete and its heartbeat paused; Woden awaits the owner's actual recording.
Its separate actuator repairs through `02fb715` and component receipt `ea25ea8`
pass targeted [source, fake-test and identity review](research/2026-09-29-actuator-rearm-review.md).
Physical acceptance remains separate; no frozen candidate/release is changed.

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

## Recording/playback update — 2026-10-04

Read [the operational runbook](RECORDING-PLAYBACK-RUNBOOK.md) and
[failure lessons](../knowledge/RECORDING-PLAYBACK-LESSONS.md). Art's original
session-5 trajectory was accepted by the owner after the full 13,455-pose run.
Original force/telemetry data is retained separately from muted output. Shared
playback 0.2 at `50b6b67` has 54 offline assertions; Woden/DRIVE/iRacing adapters
still need their own live qualification at this checkpoint. Core details:
[PLAYBACK-CORE](PLAYBACK-CORE.md). The native/toolkit handoff below is historical;
it does not describe the separate playback component's version or acceptance.

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
