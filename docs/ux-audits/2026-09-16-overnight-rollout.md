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
| Cruis'n Collection | Existing **cruisn** task | Dispatched after verifying its real v0.11.1 toolkit pin and shipped-profile use; scope is the existing player frontend/settings | Preserve rendering/parity work, menus and personal/public installations; no new ROM/GPU replay or deployment for this UI split |

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
