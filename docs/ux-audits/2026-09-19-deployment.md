# Consumer UX completion and deployment — 2026-09-19

The owner authorized completing the UX work, updating documentation, building and
deploying all seven consuming products. This supersedes the earlier candidate-only
scope. The [September 16–17 ledger](2026-09-16-overnight-rollout.md) records earlier
reviews; its candidate statuses are historical.

The baseline is [UX-1](../CONSUMER-UX.md), the
[Simple/Advanced contract](../CONSUMER-SETTINGS-VIEWS.md),
[controls/camera workflows](../CONSUMER-CONTROLS-CAMERAS.md),
[setup guidance](../CONSUMER-SETUP.md) and
[interactive reference](../reference/README.md). Guidance revision `a84beba`,
reference `95cbd89`, and identity/inventory clarification `e1f0e3f` are the
published starting points. Source, build, deployment and live acceptance are
tracked separately; a file copy does not establish input, camera or force behavior.

## Completion record

| Consumer | Starting point | Current rollout status | Live acceptance |
|---|---|---|---|
| Woden Rally Edge | 0.2.8 candidate; 35 suites/942 assertions and 377 source-linked UI checks; prior dirty work protected | Owner finishing documentation/integration and deployment | Pending |
| iRacing Arcade | Clean `d5371dd`, packaged 0.4.1; 248 consumer, 151 overlapping core, 4 config, 56 installer checks | 0.4.1 deployed and eight payload hashes independently verified; subsequent review found batch camera-default conflict, correction required | No-output menu smoke reserved; physical acceptance pending |
| Art of Sim Rally | Isolated UX implementation from `992ed83`; installed RC6 crash candidate retained | Owner completing UX/tests/package before deployment | Pending |
| #DRIVE Rally | 0.1.1 research UI; local save-retry fix and regressions | Owner verifying correction and preparing 0.1.2 deployment | Pending; wheel/FFB/added cameras remain future game capabilities |
| OutRun2006Tweaks-FFB | Committed first UI `4c29490`; calibration/render follow-up local | Agent resumed final checks, packaging and deployment | Pending |
| Milestone Wheel Tools | Clean `fe87e78`; 140 checks in PS5.1 and PS7 | Agent preparing setup-tool integration and installed supported-game deployments | Pending; external setup is a documented renderer exception |
| Cruis'n Collection | Isolated `724d8e0`; packaged frontend `3229661` | Owner completing frontend/native UX gaps and source-launcher integration | Pending; preserve World menu-force exception |

## Deployment evidence required per product

Record these in the consumer's adoption/deployment report and link the report here:

1. Source revision and any separately identified dependency overrides; package
   version, absolute local artifact path and SHA-256.
2. Verified game/launcher destination and closed-process check immediately before
   mutation. Use the supported installer or record the specific development route.
3. Backup/rollback location, installed payload hashes, and before/after evidence
   that owner bindings, calibration, tunes, view choice and saved Off survive.
4. Relevant build, migration, save-failure, installer and UI evidence. Label
   source fixtures, software-rendered images and actual game interaction accurately.
5. Remaining capability exceptions and live checks. No invented disabled controls
   or unsupported game actions solely to make the six pages look identical.

Deployment does not require changing an unrelated toolkit component, force tune,
graphics candidate or game asset. Preserve custom dependency pins. Documentation
must describe the installed player's path, with developer details kept separately.

## Coordination

### Review findings during completion

- Woden's calibration/button save changed effective bindings before persistence
  succeeded. Owner is implementing copy/write/swap transactions and actual-source
  failure/retry fixtures, including Clear/default paths.
- iRacing's batch Restore numpad defaults omitted the game-action conflict check
  used by individual Default. A previously reassigned numpad key could become both
  a game action and camera adjustment. Owner must preflight the entire batch and
  preserve assignments on conflict before a successor candidate is accepted.

### Verified first deployment

iRacing 0.4.1 was installed through its packaged installer to
`D:/Program Files (x86)/Steam/steamapps/common/iRacing Arcade`. ZIP SHA-256:
`2D36DE1A06631374E07251BC9E0BA58D0C3015AFF16DB87A8CABDA92728DD6B5`.
The coordinator independently checked all eight installed payload hashes and
the backup directory. Owner receipt verifies unchanged configuration and fourteen
bindings. Backup: `BepInEx/backup/IRacingArcadeWheel/20260919-135007-0a2dfb33`.
Receipt: `E:/Source/iracing-arcade-wheel/dist/deployment-0.4.1-20260919-134956/deployment-receipt.json`.
The later camera-default finding above keeps this an intermediate deployment.

The expired September 17 build hold was explicitly released. Offline builds and
deployment to closed targets may proceed. Live games share one desktop; request a
bounded serialized slot with output policy and normal-exit handling. Do not kill
a running game for deployment. No unattended nonzero torque is authorized.

The unrelated Cruis'n timing replay remains queued without a granted slot. The
old overnight automation remains paused; this is an active coordinated rollout.
