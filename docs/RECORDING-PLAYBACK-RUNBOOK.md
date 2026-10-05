# Start and verify recording/playback

Updated 2026-10-05. Read [the failure lessons](../knowledge/RECORDING-PLAYBACK-LESSONS.md)
before changing an adapter. This runbook is intended to work without recovering
conversation context. Source/layout migrations on other branches do not change
the identity of a tested installed artifact.

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

## Woden, DRIVE Rally and iRacing Arcade

At this checkpoint these adapters are **implemented, packaged, offline-tested,
but not runtime-qualified**. Do not cite Art's success as their acceptance.
Their isolated source branches are `codex/session-playback` under
`C:\Users\antho\.codex\worktrees\session-playback\dbce-mods-<game>`; iRacing's
exact package uses the sibling `dbce-mods-iracing-arcade-package` worktree.
Read each `docs/STAGE-PLAYBACK.md` for its exact package identity and limits.
Preserve newer installed triple-screen work before deploying an older-base plugin.

With a matching adapter installed, the commands are shared:

```powershell
./vendor/playback/Stage-Session.ps1 -Game iracing -Action Record -Path C:/captures/iracing-owner-01 -Seconds 60 -Launch
./vendor/playback/Stage-Session.ps1 -Game iracing -Action Status
./vendor/playback/Stage-Session.ps1 -Game iracing -Action Stop
./vendor/playback/Stage-Session.ps1 -Game iracing -Action Replay -Path C:/captures/iracing-owner-01 -Output C:/captures/iracing-replay-01
```

Substitute `woden` or `drive`. These adapters handle **one continuous stage**;
they do not navigate menus. Select the matching offline single-player stage/car
manually; iRacing requires explicit offline Free Play. Arming expires after
180 seconds. A new command must receive its matching request-ID acknowledgement
in `%LOCALAPPDATA%\Dbce\StagePlayback\<game>\status.txt`. Do not treat an old
status file or prepared request as a running recording.

Verify `trajectory.tsv`, `trajectory.meta`, `source.jsonl`, game-specific
sidecars and `complete.tsv`, with one source row per trajectory row. Stop/F12,
pause/focus loss and changed eligibility release ownership. Physical outputs stay
muted until restart. Observe camera/HUD and the final frame. Shared core 0.2 checks
next-tick position/rotation; actual hooks still require per-game qualification.

## Maintainer entry points

- Shared code: `dotnet/Dbce.Wheel.Playback`, tests alongside; branch
  `codex/session-playback-core`, separate unpublished 0.2 pin. Art uses the
  earlier 0.1 pin intentionally; a documentation update does not require repinning.
- Run `dotnet run --project dotnet/Dbce.Wheel.Playback.Tests -c Release` for
  device-free core coverage. These checks never establish live game support.
- Shared lessons and this runbook are indexed from toolkit AGENTS.md, CLAUDE.md,
  STATE.md and knowledge/README.md. Update the evidence/status after each new
  adapter's live qualification; keep failures, source identity and limits visible.
