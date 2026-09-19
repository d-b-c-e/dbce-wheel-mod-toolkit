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
| #DRIVE Rally | Source `1b8acea`, 0.1.2; save-retry regression fixed | Research UI/loader deployed and 28 file hashes independently verified; self-contained PS5.1 player installer follows in 0.1.3 | Pending; wheel/FFB/added cameras remain future game capabilities |
| OutRun2006Tweaks-FFB | Stage 1 source `06b94f5`, including calibration `2d20e1f` | First UI/calibration package deployed; DLLs and four retained owner files independently verified. Separate USB pedal/camera work continues | Pending |
| Milestone Wheel Tools | Clean `fe87e78`; 140 checks in PS5.1 and PS7 | Agent preparing setup-tool integration and installed supported-game deployments | Pending; external setup is a documented renderer exception |
| Cruis'n Collection | Frontend source `79a192b`, integrated into master `eb78f4f` | Source launcher deployed; seven source files and 22 owner files independently verified; native UX work continues from accepted renderer baseline | Pending; preserve World menu-force exception |

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
- Art had the same batch-default conflict with the rebound Settings key, plus
  binding/calibration/Clear changes taking effect before successful persistence.
  Both findings were sent to its owner before final candidate verification.
- Woden could save historical calibration endpoints after the candidate device
  disconnected. Its owner added current exact-device validation before commit;
  source-linked regression verification is part of the new candidate.
- Milestone's new installer initially ignored its generated package hashes, and
  uninstall lacked a rollback path for a locked later file. Both findings were
  sent to its owner before deployment, with corruption/partial-removal fixtures.
- Drive's update rollback could overwrite an externally modified existing target.
  Its successor must distinguish original bytes, its own installed bytes and
  unknown concurrent edits, preserving the latter with a recovery-required report.

### Verified first deployment

iRacing 0.4.1 was installed through its packaged installer to
`D:/Program Files (x86)/Steam/steamapps/common/iRacing Arcade`. ZIP SHA-256:
`2D36DE1A06631374E07251BC9E0BA58D0C3015AFF16DB87A8CABDA92728DD6B5`.
The coordinator independently checked all eight installed payload hashes and
the backup directory. Owner receipt verifies unchanged configuration and fourteen
bindings. Backup: `BepInEx/backup/IRacingArcadeWheel/20260919-135007-0a2dfb33`.
Receipt: `E:/Source/iracing-arcade-wheel/dist/deployment-0.4.1-20260919-134956/deployment-receipt.json`.
The later camera-default finding above keeps this an intermediate deployment.
An actual no-output 3440×1440 menu check inspected Simple Setup/Controls,
Advanced and capture. The game exited normally, logs showed FFB never opened,
and the coordinator independently verified all five owner config hashes after
restoration. Screenshots live in `E:/Source/iracing-arcade-wheel/dist/smoke-20260919-1857`.
Cancel interaction was inconclusive and is assigned for a timed successor check;
these screenshots do not establish 720p behavior or physical input/force acceptance.

Cruis'n's actual Stream Deck route uses
`E:/Source/launchbox/Launchbox-Racing/scripts/Launch-Cruisn.bat`, launching
`E:/Source/cruisn-collection/harness/collection.py`. The coordinator independently
verified seven deployed source hashes, 22 retained owner configuration files,
the unchanged wrapper and accepted native executable (`87d04de4…`). Its source
deployment receipt and backups are in
`E:/Source/cruisn-collection/results/diagnostics/ux-20260919-deployment/`.
The frozen frontend executable SHA-256 is
`0ed809c7adf0a65950c20c695c9ef132df5a24ab3f883236ad70bd695a252993`.
This stage updates the actual launcher UI; native F8/device/calibration completion
remains a separate tracked stage, not a reason to substitute the graphics candidate.

OutRun's first package (`06b94f55e62edf110f450ab9862a2cbd91068ed4`) was deployed to
the verified LaunchBox target under
`E:/Source/launchbox/Launchbox-Racing/Games/Windows/Outrun 2006 Coast to Coast/OutRun2006 Coast 2 Coast (Tweaks)`.
The coordinator independently checked both runtime DLLs and all four retained
INI/layout/manifest files against `.wheel-settings-backups/20260919-185854-286-07569bf0/receipt.json`.
The previous recognized Tweaks 0.6.1.0 proxy is backed up there. Stage 1 ZIP:
`E:/Source/OutRun2006Tweaks-FFB/build/packages/wheel-settings-20260919-stage1-verified.zip`,
SHA-256 `e448ee211c06e18fd6bb005705ada4b7cceb39453388a14506c17c3ed757c6c2`.
Separate USB pedal binding and native camera-cycle work are not included in this
stage and remain assigned; no game or physical-force acceptance is claimed.

#DRIVE Rally 0.1.2 (`1b8acea1b73469202aa2859e56bdba26fc859c67`) was installed to
`D:/Program Files (x86)/Steam/steamapps/common/DRIVE Rally`. The coordinator
independently verified all 28 payload/loader/ownership hashes and the original
absence of a mod configuration file after installation; no owner defaults were
seeded. ZIP SHA-256:
`3D8A221F0403DD29F940E5D8FDCF722F2EEBF62F35811BA5748A0FF315CB6E32`.
Receipt/staging/backup directory:
`E:/Source/drive-rally-wheel/artifacts/deploy/20260919-135855-e2014e03`.
This closes the first local deployment and reviewed retry defect, but its
repository-only PS7 installer is an intermediate route. The owner is preparing
a separate 0.1.3 player package with stock Windows PowerShell support and fixtures.

The expired September 17 build hold was explicitly released. Offline builds and
deployment to closed targets may proceed. Live games share one desktop; request a
bounded serialized slot with output policy and normal-exit handling. Do not kill
a running game for deployment. No unattended nonzero torque is authorized.

The unrelated Cruis'n timing replay remains queued without a granted slot. The
old overnight automation remains paused; this is an active coordinated rollout.
