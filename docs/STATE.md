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
compiled-only. Older dated source and candidate notes below are historical.

# Current handoff

## Shared recorded playback and Woden readiness, 2026-09-28

The owner authorized F-Zero Deluxe playback coordination followed by Woden's
next-capture readiness. Shared source-only v1 case/force-observation/comparison
tools are on `codex/recorded-playback`, clean source `87b47d53121c0b3fc8479e622ae8a79c8ca3fa58`,
in the attached recorded-playback worktree. Tests:16 passed, one symlink privilege
skip; no game/device execution. [Contract](RECORDED-PLAYBACK.md) and
[consumer assignments](research/2026-09-28-recorded-playback-coordination.md).
Woden **0.2.12 / `bf58042` is installed** for the next seated recording. Source
review and 37 suites / 965 assertions cover immutable tuning trials, strict
comparison and controller reset/gate transitions. The coordinator verified the
package, all nine deployed payloads, backup and four unchanged owner files.
No expiring request exists; the owner command creates it only when invoked.
The shared synthetic trial reports 77 changed magnitudes on the same 100-request
timeline. First real driving capture remains separate. No native/Recording repin.

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

## Woden window-selection defect, 2026-09-16

The Woden 0.2.6 run confirmed that native 0.5 overwrites its owned HWND with an unverified foreground window when called with zero, causing exit-guard failure and no FFB. [Case and shared follow-up](../knowledge/games/woden-rally-edge.md). Woden 0.2.7 passes a verified explicit HWND; shared native correction remains outstanding. No toolkit binaries or pins changed.

## Implemented and verified offline

September 12 addition, unreleased: `tools/forza/forza_capture.py` is a bounded,
receive-only 232/311/324-byte decoder/recorder with raw datagrams, strict length
and finite checks, all corner fields and summary ranges. Six tests passed:
five decoder checks and an optional subprocess/localhost UDP integration test
using bytes produced by iracing-arcade-wheel's pinned v0.12.0 Telemetry DLL.
No game or FFB device was opened. See [FORZA-CAPTURE.md](FORZA-CAPTURE.md).
Native and telemetry release binaries were not changed for this addition.

Unreleased extension (2026-09-12): `Dbce.Wheel.Recording` adds portable bounded
JSONL session capture and a validating streaming reader. It is not in published
v0.12.0. See [SESSION-RECORDING.md](SESSION-RECORDING.md) for exact API, limits,
offline verification commands and separate consumer pinning. Game sampling,
replay force comparison and live-runtime verification stay with the consumer.
Final extension checks passed on 2026-09-12: 170 managed tests (49 Recording,
80 FFB, 41 telemetry), the 24 existing sync transaction assertions, and 19 new
managed package/local-sync/release-compatibility assertions. The latter passed
with both a pre-Recording fixture and the cached published v0.12.0 archive;
`-Version v0.12.0 -Parts native,dotnet` requires no Recording DLL. No devices
were accessed, and the full native release packager was not run for this work.

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
