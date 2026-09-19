# Overnight consumer UX rollout, 2026-09-16–17

Owner-authorized coordination across consuming products. This is a work ledger,
not an acceptance certificate. The coordinator owns shared guidance, consistency
review and cross-product follow-up; each consumer owns its runtime and UI.

Guidance baseline: **a84bebab5ec2abdcd5140b9c63c139ccff86a7d3**, UX-1 revised
2026-09-16. Read [Simple/Advanced placement](../CONSUMER-SETTINGS-VIEWS.md),
[the complete UX standard](../CONSUMER-UX.md),
[binding/camera workflows](../CONSUMER-CONTROLS-CAMERAS.md) and
[acceptance checks](../CONSUMER-UX-CHECKLIST.md).

## Scope and ownership

Initial assignments, late evening September 16 (America/Chicago):

| Product | Implementer | Scope and current status | Verification boundary |
|---|---|---|---|
| Woden Rally Edge Wheel | Existing **Woden Rally Edge** task | Acknowledged: actual Simple/Advanced UI, additive e-brake, camera rebinds, steering-following FFB dropdown; timer assist in Advanced Driving | Preserve owned-window fix, panic Off, camera corrections and current owner data; offline work first |
| iRacing Arcade Wheel | Existing **iracing-arcade-wheel** task | Acknowledged: preserve current UX candidate, implement view split and saved-value migration, reconcile saved FFB On/Off and selection | Existing keyboard activation, 720p, pause ownership and attended force/recovery checks remain open |
| Art of Sim Rally | Existing **art of sim rally** task | Acknowledged and queued after the current crash-effect candidate; no concurrent edits by coordinator | Current diagnostic tune/runtime work remains with its owner; new physical checks require owner attendance |
| #DRIVE Rally Wheel | Existing **#Drive Rally** task | Acknowledged: turn the current diagnostics panel into a consistent Simple/Advanced surface with honest capability messages | Wheel input, replacement FFB and added camera functionality are not implemented or validated; do not display false readiness |
| OutRun2006Tweaks-FFB | Assigned OutRun UX worker | In progress: existing ImGui renderer can host F6 player settings; wheel/FFB/telemetry options were largely INI-only | Preserve startup input backend and toolkit pin; record calibration, device-selection and unverified force-signal gaps explicitly |
| Milestone Wheel Tools | Assigned Milestone UX worker | In progress: improve actual external PowerShell setup with Simple/Advanced, action-oriented individual calibration and cancellation | Gravel is the hardware-verified product; no F6 runtime panel or added cameras exist. Other game families require separate evidence |
| Cruis'n Collection | Existing **cruisn** task | Acknowledged; actual frontend candidate isolated from the source-based personal launcher after freezing its timing checkpoint | Preserve rendering/parity work, menus and personal/public installations; timing replay held while builds/render fixtures run |

The register's description of Cruis'n as not yet a consumer was stale. Its
`lib/toolkit/VERSION` reports v0.11.1 and its README describes pinned profiles
shipped with the product; it is included here. Its ongoing rendering/parity work
remains separately owned. Verify real pins and product surfaces rather than
excluding a repo based on historical prose.

The toolkit runtime task retains ownership of the finite constant-crash API and
its existing native branches. The coordinator does not change force models or
upgrade consumer pins merely to add a view selector. Woden's shared owned-HWND
defect remains a separate runtime follow-up, not an implied UX fix.

## Required implementation result

- Simple is the default when no view preference exists. Explicit Simple/Advanced
  choice persists; both views use the same saved values and runtime policy.
- Complete ordinary binding/calibration stays in Simple, including analog
  handbrake and independent button input. Camera adjustment rebinding remains
  accessible in Simple; live numpad adjustment and native camera rotation stay.
- FFB device selection follows the saved Steering identity by default and keeps
  explicit overrides. Preserve owner Off choices, stop/recovery behavior and all
  ownership/focus/lifecycle gates. A UI migration must not retune the wheel.
- Detailed force tuning, camera pose sliders, telemetry configuration, raw
  diagnostics and optional handling experiments belong in Advanced. Simple
  retains clear failures, recovery, active-recording status and stop actions.
- Preserve settings, stable device identities and existing bindings on upgrade.
  Show when hidden tuning is active. No required first-drive step should require
  Advanced, raw identifiers or editing a configuration file.

Each consumer writes `docs/UX-OVERNIGHT-2026-09-16.md` with a per-option inventory,
the guidance hash, exact changed source/commits, build and package identity,
focused checks and remaining live acceptance. A newly labeled toggle with all
old settings still in Simple is not completion. A missing feature must be called
a gap; only a concrete game or renderer limitation supports an exception.

## Coordinator review and overnight follow-up

1. Read the consumer's result and inspect the actual diff/placement inventory.
   Check Simple's initial density, shared labels, retained basic operations and
   hidden-value/migration behavior against the canonical requirements.
2. Verify build/check evidence and artifact identity. Ask the owning task to
   correct concrete findings; keep one editing owner per consumer.
3. Use a common HTML visual reference for layout and interaction comparisons.
   It is demo data, not a shared game renderer or proof of runtime acceptance.
4. Keep builds/offline fixtures separate from game/desktop sessions. No unattended
   torque. Coordinate any later no-output UI session serially; do not kill a
   running game to deploy. Respect each repo's public-release boundaries.
5. Record implemented, built, packaged, installed, visually checked and physically
   accepted as separate states. No public release is part of this overnight UX
   task. Preserve unrelated toolkit and consumer work.

The overnight coordination window ends at **08:00 America/Chicago on September
17**. A recovery follow-up checks progress every 15 minutes while work continues;
it is not a reason to stop useful work between checkpoints. At the end, prepare
a concise per-product handoff and pause the follow-up. Stop earlier if all useful
independent work is complete or only attended/owner decisions remain. Quiet
unchanged checks do not need a progress report.

## Results

Implementation is underway. No new consumer result has yet passed coordinator
review. Update this section with exact changes and evidence as they arrive;
earlier audits and worker acknowledgments are not adoption proof.

### Shared issue: input-to-FFB identity association

iRacing identified that its Unity HID Steering identity cannot be verified
against the current native DirectInput GUID/name inventory. The runtime owner
confirmed no verified path/serial mapping API exists in the public or current
local candidates. The approved interim is an explicit saved association with
strict identity/missing handling, separate from an override. It must remain
reachable in Simple and is recorded as an automatic-setup capability gap.
See [integration guidance](../CONSUMER-EXPERIENCE.md). No native pin change or
name/VID:PID-only inference is required or authorized by this resolution.

### Shared visual reference

[Interactive reference](../reference/wheel-settings.html) and
[preview/verification guide](../reference/README.md) now provide the common
layout and interaction example. All Simple/Advanced pages passed headless
Chromium layout checks at 720p; Setup was also checked at 4K, with explicit scale,
long names, binding/cancel/persistence/conflict flows and saved FFB Off checks.
The coordinator visually inspected Setup, FFB, handbrake calibration and camera
layout. A review correction replaced a confusing centred-Steering 50% bar with
the standard Left — Centre — Right marker; the corrected Setup was rechecked.

Frozen HTML SHA-256:
`cd64239d2a4f092a69d74d05fe8a85e915509fd8686d19a7df2cf18ac8323996`.
The reference uses example data only. Its timeout was not waited out in the
automated walk; real input/force, consumer screenshots and hardware acceptance
are not established by this artifact.

### Milestone first candidate and requested follow-up

The isolated first candidate is `4ac0f117265ddf4d846f0d699e5d4bf3d7add3db`
on `codex/simple-advanced-setup`. It implements a Simple/Advanced external setup
flow, individual cancellable capture, friendlier assignments, settings retention
and double-click entry points. The worker reports 50 fixture checks passing under
Windows PowerShell 5.1 and PowerShell 7; native code and pins are unchanged.
The original checkout/installation remains unchanged. Coordinator source review
is in progress, not integration or full UX acceptance.

A second pass is requested for actual telemetry Off/On and destination editing
inside that existing tool. The native sender accepts IPv4 only; the editor must
validate that constraint and explain when saved settings take effect. This closes
an avoidable configuration-file dependency without inventing a game UI renderer.

### OutRun strict device selection

The original v0.8 native API can fall back when a preferred GUID is missing;
consumer enumeration alone cannot close that selection race. The coordinator
verified the official v0.13.0 archive against its GitHub digest and local hash:
`2f73f5427465cd85e8d969ee7ecaf7d9c2cb08ed923da80c4ac7c7234af1b0cc`.
Its source is `dd0ef20ad0cdaccc7a67f10a707dbd2a27a6efe9`; prior full package gates
and x86 zero-output ABI smoke passed. It is not physical OutRun acceptance.

Only the official native x86 DLL and matching C-ABI header override are approved
for this UX need, with explicit per-file provenance and `NATIVE-VERSION` alongside
the existing v0.8 baseline. The older force model, profiles and encoder remain
unchanged; a whole-package bump would bring unrelated model behavior changes.
The worker is implementing the actual picker and offline validation. Consumer
status tooling is being reconciled to report this split accurately.

### Review checkpoint, 2026-09-17 shortly after midnight

- **Art of Sim Rally** finished its separate finite crash candidate first:
  consumer main `992ed83`, installed RC6 source `1cd7814`, exact toolkit
  `358add53af220eb95bf606e240b230e23a6ea197`. The existing owner task now implements
  UX in `art-of-sim-rally-ux` / `codex/ux-simple-advanced`, preserving main,
  installed RC6, the native pin and owner tunes. Its pending physical acceptance
  is separate from this UI work.
- **#DRIVE Rally** code candidate `10622c06c5da627f93d47ebcd11412a0111f9908`
  adds actual Simple/Advanced research settings with atomic migration and guarded
  connection edits. The coordinator reviewed transaction/Unity adapter source and
  three production-content HTML screenshots, including 150% save-failure Help.
  The worker reports 12 suites / 92 assertions and 60 layout renders passing.
  This is a diagnostics candidate: wheel bindings, mod FFB and added cameras
  remain unimplemented; Unity runtime and physical acceptance remain untested.
  No installation was changed. The consumer's overnight report records the
  exact package, hashes and remaining checks.
- **Milestone** now implements saved telemetry Off/On and an atomic connection
  draft in its existing setup tool. Source review confirms the native proxy
  loads configuration once; the UI correctly says changes apply next game
  launch, with delivery unverified. Review requested recovery from malformed
  view-preference JSON through explicit view selection, retaining an invalid-file
  backup. Both review fixes require their own offline fixtures.
- **OutRun** now builds with a strict device dropdown and native/header-only
  override. The coordinator reviewed selection, owned-window checks and output
  gates. Follow-up targets actual headless visual evidence and bounded basic
  calibration; source/text fixtures alone do not establish a readable game UI.
- **Cruis'n** implements and tests the actual frontend draw method in an isolated
  UX worktree. Its CPU-only packaging build is allowed; the timing replay remains
  held until a quiet serial window. The source-based personal launcher stays on
  the previous frozen checkpoint.

Woden and iRacing remain actively implementing. Every owner received the reviewed
visual reference published in toolkit `95cbd89`; receipt is not adoption evidence.

### Frozen first candidates and further work

| Product | Reviewable source checkpoint | Evidence / next step |
|---|---|---|
| #DRIVE Rally | Code `10622c0`, report `8b5e11e`, pushed `codex/ux-simple-advanced` | 0.1.1 development ZIP verified; source/layout review has no blocking finding in reviewed scope. Installed/runtime state unchanged. |
| Milestone | `4ac0f11` plus `05fd6dc`, pushed `codex/simple-advanced-setup` | 106 fixture checks pass on PowerShell 5.1 and 7.6.5; coordinator reviewed telemetry and preference repair. Next add a bounded, read-only device-input preview. |
| OutRun | Native override `7fd8be4`, UI `4c29490`, local `codex/ux-simple-settings-2026-09-16` | First build and fixtures frozen. Next export actual ImGui draw data for software rendering and add opt-in transactional axis endpoints/centre; legacy input behavior remains until explicit calibration. |
| Cruis'n | `dd4e7ff`, pushed `codex/ux-simple-advanced` in isolated worktree | 39 focused tests, 10 actual draw-method images and frozen frontend config checks pass. Coordinator inspected 720p Controls. Telemetry editing and separate saved FFB Off/On are next. |

These checkpoints are candidates, not merges, deployments or full UX acceptance.
Per-consumer `docs/UX-OVERNIGHT-2026-09-16.md` files hold the detailed inventories,
artifact hashes and gaps. Cruis'n's future in-game F8/device selection work must
be assessed separately from the frontend, preserving the owner's World menu-force
exception and the frozen native timing candidate.

Coordinator review of iRacing's in-progress association code identified a legacy
generic-binding risk: creating a saved device association did not itself replace
the generic Steering path. A later sole matching input could resolve a different
previously saved association. The owner task was asked to require an identity-bound
Steering path (or upgrade it atomically), with an A/B replacement regression before
freezing. [Shared integration guidance](../CONSUMER-EXPERIENCE.md) now spells out
that case. This finding is not yet a verified fix or evidence of observed torque.

### Consumer registry verification

The register and `Get-ConsumerStatus.ps1` now include all seven consumers and
their actual pin sources. OutRun's baseline and native/header override are two
separate rows; the override files are checked against their provenance hashes.
Drive is evaluated only against Telemetry sources, and Recording extensions keep
their own provenance. Missing/malformed pins and Art's local build require review
without suggesting a release downgrade. iRacing's explicit `toolkit.version`
precedes labeled legacy changelog inference.

The isolated status-tool suite passed **76 assertions under both PowerShell 5.1
and 7.6.5**, including output, exit status, source filtering and unchanged file
hashes/timestamps. A real read-only comparison against v0.13.0 produced the
expected eight component rows and exit 1 for review items. This is dependency
inventory evidence, not installed-game or UX adoption certification.

The toolkit reference push also created an automatic hosted CI run, but its jobs
were refused before starting. Hosted CI is not part of the passing evidence above;
subsequent coordination pushes use `[skip ci]` and retain local verification.

### Follow-up review, 2026-09-17

- **Milestone** completed its bounded external-setup pass at `fe87e78`, published
  on `codex/simple-advanced-setup`. Simple now includes a read-only preview using
  staged mappings, with one input-only helper, cancellation and bounded cleanup.
  The coordinator reviewed normalization and the actual helper heartbeat source.
  **140 checks pass in both PowerShell versions**; real device/console interaction
  remains untested. Original main and game installations are unchanged.
- **iRacing** fixed the identified generic-path association issue. The coordinator
  re-read the actual resolver, saved-input lookup and role policy: follow now
  requires the exact persisted role before association lookup, and association
  no longer creates an inferred mapping while leaving the action generic. The
  owner reports A/B replacement/reorder regressions passing. Runtime acceptance
  and final package checks are still separate.
- **OutRun** now exports actual ImGui draw data/font atlas to a software renderer.
  The coordinator inspected 720p and scaled 4K FFB frames from the 26-frame fixture;
  layout has no blocking finding in that scope. These are renderer-level images,
  not game screenshots. Transactional calibration is still being checked.
- **#DRIVE Rally** received a second independent review. It reproduced a save
  recovery bug in the frozen Core DLL: fail Apply for ports 9000/9001, edit the
  visible draft to 9100/9101, then Retry writes the older pair. The owner is fixing
  the pending-transaction/edit relationship and reviewing Cancel recovery before
  a replacement candidate is accepted. No additional concrete keyboard/focus
  defect was established; Unity interaction remains untested by HTML fixtures.
- **Cruis'n** added atomic telemetry settings at `360d9ed` and a saved FFB switch
  retaining Strength at `3229661`. Both are launcher preferences applying next
  game launch. In-game F8 needs a new latched gate across all native effect types;
  strict stable identity needs a binding/native contract, not a display-name
  inference. The native timing checkpoint remains separately frozen.
