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
| Woden Rally Edge | Source `df0e4dc`, 0.2.8; prior work preserved separately in `9d28b83` | Deployed; 35 suites/942 assertions, 459 UI assertions and 59 installer checks; nine payloads and four owner files independently verified. Installer concurrency correction follows separately | No-output run exited normally; F6 panel did not open, diagnosis/retest required |
| iRacing Arcade | Source `4a06481`, packaged 0.4.2; 248 consumer, 151 overlapping core, 10 config, 56 installer checks | Successor deployed; conflict/save-retry checks passed in game. 0.4.3 addresses stock menu dispatch behind panel | 0.4.2 normal exit and six restored owner/receipt hashes verified; physical acceptance pending |
| Art of Sim Rally | Source `44e11dd`, 0.2.6-rc.9; all sixteen RC gates passed | Deployed; six payloads, six retained owner/game files and ZIP independently verified; RC9 corrects installer completion wording | No-output UMM menu check queued |
| #DRIVE Rally | Source `ebb9e39`, 0.1.3; save-retry and rollback corrections included | Self-contained PS5.1 player package installed; all 28 installed files and package hash independently verified | Pending; wheel/FFB/added cameras remain future game capabilities |
| OutRun2006Tweaks-FFB | Stage 3 source `8ac4d9f`; missing-section and disconnected-primary-pedal fixes included | Stage 3 deployed; two runtime DLLs, five owner files and ZIP independently verified | Fixed launch/F6 and stock camera identification queued |
| Milestone Wheel Tools | Source `3545810`, 0.2.0; 140 UX + 81 installer checks in PS5.1 and PS7 | Deployed to Gravel; fourteen installed files and three owner files independently verified | External setup/native calibration exception; physical/game acceptance pending |
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
- Woden's installer rollback could write while a game started concurrently and
  could replace externally changed files. Its installer-only successor must retain
  unknown bytes and backups, report recovery required, and test both races.
- iRacing's actual 0.4.2 panel check showed stock menu actions still dispatching
  behind F6. Its successor blocks Unity EventSystem dispatch while modal; review
  also caught missing held wheel/pad navigation in the release aggregation.
- Cruis'n's native selector and name-only launcher mappings can choose the first
  ambiguous device; unresolved MAME mappings can retain numeric device slots.
  Its axis/sign wizard lacks endpoint calibration. Audit `1dfdc2ae` proposes strict
  identities, neutral unresolved input and staged calibration applied by the actual
  native input path. Those are assigned work, not accepted capability exceptions.

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

**Superseding live finding:** the first launch failed with `0xc0000142` before a
window or FFB initialization. Its new migration called `ini.Keys("FFB")` when the
actual older upstream INI had no FFB section; the parser throws for an absent
section. The worker exited the error dialog normally and restored the recognized
upstream proxy through the tested restore route, removing the temporary Off override
and verifying original settings. The coordinator independently verified the restored
proxy hash `116bc609d2af4b366f7f02e3594f7d3be1e57c2f1fd937d9b8f36b9df1cf5cf9`.
The earlier stage is no longer the active installation. Actual older-INI regression,
fixed successor deployment and another no-output launch are required.

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

Woden 0.2.8 was deployed from `df0e4dc323cf39266ebc553d637638cc416d650c` to
`D:/Program Files (x86)/Steam/steamapps/common/Super Woden Rally Edge`.
The coordinator independently verified all nine payloads against
`BepInEx/WodenWheel-install.json` and all four owner files against the backup.
Backup: `WodenWheelBackups/before-install-0.2.8-20260919-141503-904bcc76`.
Original ZIP SHA-256 `e254425c213890bdde7d8e3530dccde39206badb78ec7884a271fc49fd9036b9`.
The owner corrected packaged uninstall documentation without changing the nine
runtime files; the revised ZIP hash is
`914657af4dfeb080782724853d2936fdee486388b58078f7a1620af3c8784ac1`.
Future package corrections must use a distinct frozen artifact identity.
The save-failure/disconnect corrections are included. Source fixtures are not
live Unity input evidence. The force-disabled run exited normally, reported
physical output suppressed, zero writes/failures, and finalized 3,206 recording
samples with zero drops/errors. Its owner verified all four original config files
after restoration and archived the consumed one-launch diagnostic request.
F6 injection did not open the panel during stock attract mode; Simple/Advanced
and binding interaction remain unverified and assigned for diagnosis/retest.

Milestone 0.2.0 was deployed from `3545810f4fd33647ff19ab87ab02a9aec0afa47a` to
Gravel's `gravel/Binaries/Win64`, with `Wheel settings.bat` at the game root.
Package SHA-256 `c561eaec8ec1f1cf54af0af78a92e5d8e31a39f6d9f8daf3d2fa24d8c614b055`.
The coordinator independently verified all fourteen receipt entries plus the
original, backup and current INI, WheelConfig and settings.sav hashes. Receipt:
`E:/Source/milestone-wheel-tools-ux-20260916/artifacts/deployment-2026-09-19.json`.
Its installer backup is `DBCE-Wheel-Backups/20260919-141450-2f6780`; independent
owner backup is `owner-state-20260919-141449-e51d8e` beneath the same directory.
Both reviewed installer findings are closed in the deployed source. Gravel is
the only installed supported target; calibration/FFB/cameras remain game-owned.

### Verified successor deployments and actual interaction

iRacing 0.4.2 (`4a0648194a69f46d66d91343f83de0bcef451c5b`) ZIP SHA-256 is
`554214A3442F9D4C2B9F6DCDBB6F301E5AC5684558BE0F0048C72A8AD01C00B7`.
All eight payload hashes were independently verified. Its actual menu check
passed focused Cancel, whole-batch game-action conflict rejection, locked-save
draft retention, successful Retry and subsequent Cancel preserving the old file.
The game exited normally at 19:21:24 UTC; no FFB device opened. The coordinator
verified all six restored owner/config receipt hashes afterward. Evidence:
`E:/Source/iracing-arcade-wheel/dist/smoke-0.4.2-20260919/smoke-receipt.json`.
The source game screen was 2560×1440 and captured images 1280×720. Stock Free Play
opened behind the panel during this run; 0.4.3 must close and recheck that observed
input propagation defect before claiming complete menu isolation.

Art's RC8 source is `2517162d71fc12e030ecda64d8b46d31f0d56b20`; ZIP:
`E:/Source/art-of-sim-rally-ux/dist/ArtOfSimRally-0.2.6-rc.8.zip`, SHA-256
`0FFB6CEAE47E7268AD2A4815B69F69984D8CEA56C7028A5377E1594B337352C3`.
The coordinator independently verified six installed mod payloads, six retained
owner/game/recorder files and the native plugin duplicate. Receipt/full prior
installation backup:
`E:/Source/art-of-sim-rally-ux/results/ux-rc8-install-8a331d2029c14571892bb2e2cb0f5b4f/`.
The separately pinned finite-force native candidate remains unchanged. Native
keyboard-map conflict detection, batch defaults and transactional binding saves
are included. The installer completion wording is receiving a separate RC9 package.
RC9 subsequently passed all sixteen gates and was installed from
`44e11dd7a7c0e6dd99cef7f2c8be0620e9ce717a`; ZIP SHA-256:
`8D92D7758AD25F8F62A870A4275B72BE685C533D79FF3976A98FE17EDA3E1199`.
The coordinator independently verified its six payloads, six retained owner files
and ZIP against `results/ux-0.2.6-rc.9-install-3570eb7cbaf247c4b2c74ee9824ef17a/receipt.json`
in `E:/Source/art-of-sim-rally-ux`. The old saved FFB name/index has no strict GUID;
the player must explicitly choose Use steering wheel or reselect the FFB device.
No tune or binding was silently migrated to a guessed device.

Drive 0.1.3 source `ebb9e3992709e86f00f63948d363afe7169a4499` installed through
its packaged Windows PowerShell 5.1 route with Steam discovery. Player ZIP SHA-256:
`5171A488D7C9CA85D83602ADE1E77D798C94872EC68DDDEE779FE205CDBB1189`.
The coordinator independently verified all 28 installed entries and the ZIP.
Receipt and rollback originals:
`C:/Users/antho/AppData/Local/Dbce/DriveRallyWheel/Backups/20260919-142242-4f9d1d8e/`.
No mod configuration existed before or after deployment. Fourteen packaged PS5.1
cases and thirteen developer installer cases include preservation of external
changes during rollback. This is a research/telemetry UI deployment, not completion
of the game's future wheel/FFB/camera features.

OutRun stage 3 source `8ac4d9f026644264852d93f8a7435e4d5177ee24` replaces the
failed first stage and unlaunched stage 2. ZIP:
`E:/Source/OutRun2006Tweaks-FFB/build/packages/wheel-settings-20260919-stage3.zip`,
SHA-256 `8383b567c66cb359ad6f35a0ce30d0df692c9934a58e8202998e5699965ee0f4`.
Installed proxy SHA-256:
`b36a3cc1e55ff7fbd3a3c28c0981fd6d174ea7ca122f34e8da28d8feed5be67c`.
The coordinator independently verified both runtime DLLs and five retained owner
files against `.wheel-settings-backups/20260919-192938-828-68521837/receipt.json`.
The v0.13 native component override remains distinct from the package pin.
Independent source review found calibrated pedals on the primary device could
reuse stale samples after disconnect; stage 3 gates current/previous/telemetry
paths and includes a lost-read regression. Physical reconnect remains unverified.

The expired September 17 build hold was explicitly released. Offline builds and
deployment to closed targets may proceed. Live games share one desktop; request a
bounded serialized slot with output policy and normal-exit handling. Do not kill
a running game for deployment. No unattended nonzero torque is authorized.

The unrelated Cruis'n timing replay remains queued without a granted slot. The
old overnight automation remains paused; this is an active coordinated rollout.
