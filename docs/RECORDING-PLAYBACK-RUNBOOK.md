# Start and verify recording/playback

Updated 2026-10-05. Read [the failure lessons](../knowledge/RECORDING-PLAYBACK-LESSONS.md)
before changing an adapter. This runbook is intended to work without recovering
conversation context. Source/layout migrations on other branches do not change
the identity of a tested installed artifact.

## Preserve a qualified reference before worktree cleanup

For the shared core's sealed trajectories (Woden, iRacing and DRIVE), use the
toolkit's PowerShell 7 helper to copy **all sealed streams** to a new private
directory. It verifies source and destination hashes, rejects linked/escaping
paths and overwrites, and publishes the copy only after verification:

```powershell
./tools/playback/Copy-StageReference.ps1 -Source 'E:/path/to/recording' -Destination "$env:LOCALAPPDATA/Dbce/StagePlayback/references/game-reference-date"
```

Save its JSON receipt beside the new reference, then run the game's original-data
review command against that copy. This preserves bytes; it does not qualify a
recording or copy unsealed logs, screenshots or owner-state backups. Archive those
separately and verify the archive before retiring a worktree. Art session 5 uses
an older format without this seal; follow its dedicated instructions below.
`tools/playback/Test-StageReference.ps1` exercises complete/nested copy, overwrite,
changed-stream, nested-destination, escaping-path and duplicate-name refusals.

## Art of Rally: known-good reference playback

At the October 4 acceptance checkpoint the game is installed at
`D:\Program Files (x86)\Steam\steamapps\common\artofrally` and the tested source
checkout is `E:\Source\games\dbce-mods-art-of-rally`, branch
`codex/session-playback`. **0.4.0 is published and installed**, including the
matching optional probe. Tag `v0.4.0` is source
`30d421b26eccebc564cd50bf0733c1ed6a82d849`. Both archives and checksum files were
downloaded from GitHub and matched to the tested files. Verify the current
install receipt before assuming a later session has kept this installation.
The local reference is **2026-10-04-owner-session-5** (Japan6/Kei/Afternoon).
Never overwrite, regenerate or substitute it with `synthetic-roundtrip-*`.

Release: https://github.com/d-b-c-e/dbce-mods-art-of-rally/releases/tag/v0.4.0

- Normal archive SHA-256: `C498F8139FD64D13DD395CC036DE71C50614087C216280A17AA3A4724A96B3CA`.
- Optional SessionTools SHA-256: `08DA5536F641E7ADAF7B8414DD3C45205A40A90F6F3A43629B14F2B5C8476C94`.
- Local final run: `replay-20261004-210753`, same 13,455 poses and zero error,
  launcher exit 0. Saved files and all 58 game preferences restored.

1. Arrange exclusive rig use; all other racing games and other agents' display
   tests must be stopped. Keep physical outputs muted. Do not change resolution,
   triple geometry, wheel gain or the owner's bindings for a playback check.
2. The game must be closed. Confirm the normal mod and matching optional recorder
   are installed. The optional release ZIP has its own `build.json`, seal and
   install script; it does not contain a reference recording or game assemblies.
3. Run these commands from PowerShell:

```powershell
Set-Location 'E:\Source\games\dbce-mods-art-of-rally'
git branch --show-current
Get-Process artofrally,'Super Woden Rally Edge',DRIVERally,iracing-arcade -ErrorAction SilentlyContinue
Get-Content 'results/sessions/2026-10-04-owner-session-5/session.txt'
pwsh -NoProfile -File tools/testing/Session.ps1 -Replay -Name 2026-10-04-owner-session-5 -TimeoutMinutes 8
```

If `tools/testing/Session.ps1` is absent because this checkout is on the newer
component-layout branch, **do not reset/switch someone else's working tree**.
Use the release's separately extracted SessionTools archive or an isolated
checkout of its recorded source. Copy the whole reference directory unchanged
under that tooling root's `results/sessions/`; verify source file hashes. The
normal player ZIP alone does not include Session.ps1 or the developer probe.

On this machine the verified optional archive is already extracted at
`E:\Source\games\dbce-mods-art-of-rally\results\session-tools-040-final`.
If rebuilding tooling is unnecessary, use that frozen copy. From its root:

```powershell
# Only if the matching optional probe needs installing, with the game CLOSED:
pwsh -NoProfile -File tools/testing/Install-Recorder.ps1 -SkipBuild
# Reference destination must be new; copy source files and environment backups intact.
New-Item -ItemType Directory -Force results/sessions | Out-Null
if (Test-Path results/sessions/2026-10-04-owner-session-5) { throw 'Reference already exists; use it without copying over it.' }
Copy-Item -LiteralPath 'E:\Source\games\dbce-mods-art-of-rally\results\sessions\2026-10-04-owner-session-5' -Destination results/sessions -Recurse
pwsh -NoProfile -File tools/testing/Session.ps1 -Replay -Name 2026-10-04-owner-session-5 -TimeoutMinutes 8
```

Use the first checkout command when it is available; copying the private reference
is only needed for another tooling root. Never copy over an existing case. Format-2
session 5 predates completion seals; preserve its source bytes and metadata.

Steam launches normally; dismiss any real Steam prompt before interpreting
startup as failed. Menus and the route run automatically. A successful run is
about five minutes including menus. It closes the game and restores its saved
environment. `-KeepGameOpen` intentionally defers restoration until game exit.

Expected evidence in a **new** `replay-YYYYMMDD-HHMMSS` directory:

- `result.txt` begins `passed`; launcher exit code 0. Failure is 1, abort is 2.
- `posesApplied=13455`, `maxApplicationError=0.00`, no skipped UNDERWAY rows;
  finish, results and final menu reached. Transient non-driving/menu skip counts
  may differ and are reported; do not demand those counts equal a prior run.
- Camera follows the recorded car normally, no flying/reset loop. Observe this;
  the numeric pose check alone cannot certify camera quality.
- `owner-environment/restored.txt` exists after normal game close. Settings.xml
  and TripleScreen.xml match the owner backup, with current display preferences
  retained throughout replay. Original source files remain unchanged.

## Stop, status and recovery

```powershell
pwsh -NoProfile -File tools/testing/Session.ps1              # bounded status request
pwsh -NoProfile -File tools/testing/Session.ps1 -Stop        # abort/save as appropriate
```

Raw **F12** also takes over playback. Output suppression remains latched for the
process; restart the game for normal wheel/network output. Do not expect a
stopped run to report passed. Close the game normally before restoring files.
If the launcher reports a pending restore, use the **specific directory it printed**:

```powershell
pwsh -NoProfile -File tools/testing/Session.ps1 -RestoreEnvironment 'E:\path\to\replay-...\owner-environment'
```

Do not import a random registry backup or delete failed captures. Preserve
`replay.log`, result, Player.log and the exact package/source identity. Check
`%LOCALAPPDATA%\ArtOfSimRally\session-request.txt` before another launch; requests
expire, but an unexpected request must be understood, not repeatedly retried.
If the desktop becomes garbled or input fails, stop live testing and retain
Windows/Unity evidence; do not keep launching different games.

New environment backups use `playerprefs-raw.json` with original registry value
types/bytes and a verified hash/count. `reg export` silently omitted 21 of 58
values on this rig, so its successful exit status is insufficient. Restore with
Session.ps1; do not replace the raw snapshot by a partial `.reg` export.

## A new real recording and force analysis

```powershell
# Choose a NEW name; ordinary recording follows the actual player.
pwsh -NoProfile -File tools/testing/Session.ps1 -Record -Name owner-japan-new-01 -MuteOutputs
# Select the stage/car, drive, then pause and explicitly stop before quitting:
pwsh -NoProfile -File tools/testing/Session.ps1 -Stop
# Check complete.tsv/end.txt and nonempty forces/signals/telemetry/effects as applicable.
pwsh -NoProfile -File tools/testing/Session.ps1 -Replay -Name owner-japan-new-01

# Source checkout requires .NET 8 SDK:
dotnet run --project tools/testing/Replay -c Release -- --replay results/sessions/owner-japan-new-01
# Extracted optional archive requires .NET 8 runtime:
dotnet analysis/Replay.dll --replay results/sessions/owner-japan-new-01
```

`-MuteOutputs` preserves Art's normal enabled/ready FFB calculation path while
suppressing physical wheel/UDP delivery. Turning the producer itself off can
yield no force samples. Inspect manifest/config and counts; never describe an
empty stream as a valid zero-force result. Reprocess **original** physics,
not the kinematic playback's live tyre readings. Identity and zero-gain trial
examples and units are in Art's `docs/SESSION-REPLAY.md`.

## iRacing Arcade: accepted unattended reference

Installed private candidate `0.4.7-delivery-v1-repack.109`, clean runtime
`d6bab7663f93047c4fb48e1f5facfb1096f3354e`, ZIP SHA-256
`C4CE5AC0F52A4E7DEBCC3CF280AA59E8614897ACC13F0D0D84FC7B2CC843D288`.
Two cold-launch runs passed all 3,001 poses of the owner's one-minute Aragón /
Euro Truck drive (maximum position error .000030517578125 m). The owner accepted
the route/camera. The repeat completed automatic exit and exact owner restoration;
intentional Stop also exited/restored with a correctly failed partial route.
The owner requested no further repeat launches; move on to Woden offline while
their separate session uses the screen for triple-screen work.

```powershell
Set-Location 'E:\Source\games\dbce-mods-iracing-arcade'
# Historical example: recover the private reference before using this command.
# Closed game, new result directory; no manual menus or driving.
./tools/game/Replay-Recording.ps1 `
  -Recording results/reference-truck-aragon-20261004 `
  -Result ('results/replay-' + (Get-Date -Format yyyyMMdd-HHmmss))
# Interrupted runner recovery, only after game closes:
./tools/game/Replay-Recording.ps1 -Result <same-result-folder> -RestoreOnly
```

**Private reference missing after worktree consolidation:** the accepted owner
and reference recordings were not found in the canonical checkout or former
worktree on October 5. The committed acceptance report remains; the command
above cannot run until an original backup is recovered or a new reference is
recorded and qualified. Do not recreate the filename and call it the accepted case.
Historically, the reference added a sealed launch preset without changing its
four original trajectory/force/telemetry files. Native APIs join the local
player, await authentication, select offline Free Play, load setup and start.
Only one track and an unambiguous vehicle class are supported. A new recording
must be armed before Free Play Start to retain its launch setup automatically.
Focus loss, pause and eligibility failures remain aborts; physical outputs stay
muted, while original source force/telemetry remain available for analysis.

**Presentation correction pending:** generic Steam startup reused an already
small windowed state, and the runner faithfully restored that wrong baseline.
The owner expects ordinary triple-screen presentation for playback and driving.
Steam has no saved iRacing launch options. `tools/game/Set-SteamTripleLaunch.ps1`
previews the normal 7680x1440 borderless span; `-Apply` requires Steam closed.
Do not restart Steam while the owner's Woden session owns the screen. This
correction was prepared, not applied or visually retested. Do not say the rig is
ready in triples until this pending correction is completed. Preserve 70-degree
geometry and existing tuning; a display fix is not grounds to repeat the route.

See the consumer `docs/STAGE-PLAYBACK.md` and
`docs/reviews/2026-10-05-unattended-playback.md` for exact evidence and limits.
The launcher is currently source-checkout tooling; the installed generic
`Stage-Session.ps1` alone does not provide supervision/restoration.

## Woden: accepted unattended reference, triples still open

Canonical checkout: `E:/Source/games/dbce-mods-woden-rally-edge`, main. The combined
installed runtime is `0.2.14+a628e04c2101866a2a22335004f4f536aa565d93`.
The owner accepted Kenya SS1 / car 8 route and cameras after 3,601 poses passed
(maximum error .00006103515625 m / .0000172453823 degrees). Native cold startup,
normal exit and exact restoration passed. Original force analysis retained
3,553 valid / 3,477 nonzero samples, with zero numerical error and no physical
output attempts. The observed run was single-screen; normal triple presentation
is an open final check, not covered by this acceptance.

The seven sealed reference files are now protected outside disposable worktrees.
Use this exact private reference, a closed game, and a new result directory:

```powershell
Set-Location 'E:/Source/games/dbce-mods-woden-rally-edge'
./tools/game/Run-StageSession.ps1 `
  -Recording "$env:LOCALAPPDATA/Dbce/StagePlayback/references/woden-kenya-20261005" `
  -Result ('results/replay-' + (Get-Date -Format yyyyMMdd-HHmmss))
# Recovery only after this run's game closes:
./tools/game/Run-StageSession.ps1 -RestoreOnly -Result <same-result-folder>
# A new owner drive is a separate launch; select single-player Arcade practice:
./tools/game/Run-StageSession.ps1 -Record -Seconds 60 -Result results/owner-new-01
```

Build the consumer TelemetryInspector before using source-checkout supervision.
The installed Stage-Session.ps1 alone does not back up/restore the environment.
The public player preview passed 111 stock PowerShell installer checks; it is
not published or a replacement for the accepted installed artifact. See consumer
`docs/PUBLIC-RELEASE.md` and `docs/2026-10-05-playback-integration.md`.

## DRIVE Rally: qualified diagnostic recording and cold playback

Canonical checkout: `E:/Source/games/dbce-mods-drive-rally`, main. Installed clean
runtime `24d7bdf6fe18b4ec5b4821b30be385c2ee5fd55a`, session-8 player ZIP SHA-256
`A3026627D5D97044355FB9593C3C6254E8438592674F255C214B476119576253`.
Native Free Roam startup, a 60-second diagnostic capture, a separate cold replay
of **6,001/6,001 poses**, and intentional Stop passed with normal exit and exact
file/raw-preference restoration. Maximum replay error .0001220703125 m /
.000019469034601067067 degrees. Camera/HUD/wheel/dust screenshots were compared
to the original capture. No output was delivered and no display settings changed.

This is explicitly `automated-diagnostic-v1`, using actual game physics. It
travels 77.26 m then meets a tree; it is not an owner drive, route AI or full-stage
qualification. All 6,001 original rows retain solver data and 22,118 stock effect
requests. UDP was disabled in the retained setting, so no original packets were
available. No signed mod rack-force model or measured wheel torque is supplied.

```powershell
Set-Location 'E:/Source/games/dbce-mods-drive-rally'
# Existing sealed reference, game closed, NEW result directory:
./tools/game/Run-StageSession.ps1 -Recording "$env:LOCALAPPDATA/Dbce/StagePlayback/references/drive-holzberg-presentation-20261005" -Result artifacts/playback-new-01
# New automated diagnostic; omit -Diagnostic for an owner drive:
./tools/game/Run-StageSession.ps1 -Record -Diagnostic -Seconds 60 -Result artifacts/diagnostic-01
./tools/game/Run-StageSession.ps1 -Recording artifacts/diagnostic-01/recording -Result artifacts/playback-01
# No game/device output:
dotnet tools/DriveRally.Diagnostics/bin/Release/net10.0/DriveRally.Diagnostics.dll stage-review artifacts/diagnostic-01/recording
```

The reference has nine sealed streams plus `complete.tsv`; seal SHA-256
`CAE4CD2289B2A118DFD7A0CB07F43587D5DD2B44D286B71EA279A12682DFFBC5`.
Current `drive.freeroam@2` requires separately recorded visual state. Legacy @1
captures lack it and are refused before launching. Never replace the missing
data with fabricated zeros. Session-6 found transient preset IDs; session-7
found HUD/dust updates shared the suppressed physics callback. Both failures
and fixes are retained in consumer `docs/reviews/2026-10-05-stage-session8.md`.
Owner driving, this adapter in normal triples and broader interruptions remain
open. The tests preserved the current 2560x1440 ExclusiveFullScreen baseline.

For Woden/DRIVE/iRacing the raw command still lives under `vendor/playback` or
beside the installed plugin. `-Game <woden|drive|iracing> -Action Stop` and F12
stop ownership; output suppression stays latched until process exit. Verify
request-ID correlation and completion seals; an old status file is not evidence
of the current run. Supervised wrappers are the normal operator entry points.

## Maintainer entry points

For new supervised adapters, use the [owned-process watchdog contract](STAGE-SUPERVISION.md).
Pin the watcher/policy scripts together. Never kill games by name or rewrite the
monitor profile as a generic response to a stalled recording.

Woden, iRacing and DRIVE runners now use hash-pinned `Stage-RigLease.ps1`: acquire
before backup/arming/launch and release only the matching token after game close.
A fresh slot refuses the run before owner mutation. The installed generic command
does not replace the supervisor or owner-state backup/restore wrapper. Woden and
iRacing have not adopted DRIVE's current-request mute heartbeat; do not retrofit
forced termination by assuming their physical outputs are off.

### Sonic CrossWorlds preparation

Sonic's newer UE4SS loader now initializes, as observed during the separate triple
work. `games/dbce-mods-sonic-crossworlds` main `3948cad` adds a bounded read-only
game-thread observation probe with stage/install/arm/stop/removal scripts. Five
Lua 5.4 tests and isolated setup checks pass. No live probe run or in-game replay
is qualified yet. Follow that consumer's `docs/STAGE-PLAYBACK.md` for the first
test; keep its discovery data distinct from a physics-tick trajectory. Its older
companion input scheduler only proves report scheduling, not a reproduced race.

The probe queues at most one callback, accesses UObjects only on the game thread,
and prevents a late callback from reading state after Stop or a stall timeout.
Missing fields and original force data remain unavailable. Discover the actual
car tick, mode/startup authority and competing body writers before implementing
an active player; do not infer them from a camera hook or the first found pawn.

- Shared code: `dotnet/Dbce.Wheel.Playback`, tests alongside; now merged to the default branch, separate unpublished 0.2 pin (source
  `50b6b679e3c6e01025b6a50f8dd0f3eb85d9d4bb`). Art uses the
  earlier 0.1 pin intentionally; a documentation update does not require repinning.
- Run `dotnet run --project dotnet/Dbce.Wheel.Playback.Tests -c Release` for
  device-free core coverage. These checks never establish live game support.
- Shared lessons and this runbook are indexed from toolkit AGENTS.md, CLAUDE.md,
  STATE.md and knowledge/README.md. Update the evidence/status after each new
  adapter's live qualification; keep failures, source identity and limits visible.
