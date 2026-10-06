# Start and verify recording/playback

Updated 2026-10-05. Read [the failure lessons](../knowledge/RECORDING-PLAYBACK-LESSONS.md)
before changing an adapter. This runbook is intended to work without recovering
conversation context. Source/layout migrations on other branches do not change
the identity of a tested installed artifact.

For unattended launch admission, `tools/playback/Stage-OwnerIdle.ps1` provides
`Assert-StageOwnerIdle` (300 seconds by default). It reads raw Windows last-input
age and refuses recent, missing or invalid observations; it never injects input,
changes focus, waits or launches. Call before taking the lease and again just
before launching. Sonic's probe runner pins and enforces this check; adoption by
other runners remains separate. A queue may wait outside the lease and retry.
Do not discount input merely because a game or launcher is foreground: this
conservative check makes no owner-versus-tool attribution. It is launch admission,
not an active-game takeover detector. Recovery/restoration must remain available
when the owner returns.

For a coordinated child operation under an existing rig slot, use
`Assert-StageRigLease -Path <slot> -Token <original-caller-token>` before work.
It validates exact, unexpired ownership without rewriting, renewing or releasing
the parent's lease. Sonic addon setup/removal accepts `-LeaseToken`; default
standalone setup still acquires/releases its own slot. Keep the original token
returned by the caller's acquisition, not an arbitrary token read from the slot.
The lease itself does not check idle input. The caller still owns settings
backup/restoration, launch/close and final release. This does not add borrowed
lease support to the Unity replay runners.

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

## iRacing Arcade: new diagnostic reference and historical owner acceptance

Current installed private runtime is clean `802a6feb2ee0028a2c024323a71abe4ef9c4ff47`,
`0.4.7-delivery-v1-repack.114`, ZIP SHA-256
`FBD8A4596AE41CA6D8F422F0DA8550B7A5CB9F43C8E26C9673D2A6AFFCFD21A8`.
A new explicitly automated native-physics diagnostic captured 3,001 poses over
60 seconds, travelled 214.632 m and peaked at 40.618 km/h. Its separate cold
replay passed every pose, including the final one, with maximum error
0.000030578 m / 0.000019015 degrees. Original force analysis passed 3,001 rows
with zero mismatches/errors and zero native attempts. Normal quit, exact saved
settings restoration and current-request watchdog mute confirmation passed.
Claude subsequently replayed all 3,001 poses at 7680 with triples in motion and
centred menus. A faint left sky seam and owner camera acceptance remain; see the
portfolio's October 5 combined check. This is not the historical owner's tape.

The complete reference is protected outside the checkout. Copy receipt verifies
five sealed streams plus the seal, 36,459,708 bytes. Seal SHA-256:
`B2FBB3F324A16FFE42299340A8B5E76BB65F9E2B4E4A6368F0A5C89EE0EA2F56`.
Consumer main `b56d4c2` documents it; see
`docs/reviews/2026-10-05-diagnostic-reference.md` for failed-first-run evidence.

The earlier owner acceptance below remains separate:

Previously installed private candidate `0.4.7-delivery-v1-repack.109`, clean runtime
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
# Current new diagnostic reference; it is not the lost owner recording.
# Closed game, new result directory; no manual menus or driving.
./tools/game/Replay-Recording.ps1 `
  -Recording "$env:LOCALAPPDATA/Dbce/StagePlayback/references/iracing-aragon-diagnostic-20261005" `
  -Result ('results/replay-' + (Get-Date -Format yyyyMMdd-HHmmss))
# New OWNER recording: native startup, then drive; save and quit before replay.
./tools/game/Replay-Recording.ps1 -Record -Seconds 60 -Result results/owner-NEW
# For another explicitly automated diagnostic, also pass -Diagnostic.
# Interrupted runner recovery, only after game closes:
./tools/game/Replay-Recording.ps1 -Result <same-result-folder> -RestoreOnly
```

**Private reference missing after worktree consolidation:** the accepted owner
and reference recordings were not found in the canonical checkout or former
worktree on October 5. The committed acceptance report remains. The current
command above uses a genuinely new labelled diagnostic, not a replacement
masquerading as the accepted owner case. Do not recreate the old filename.
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
The last recording checkpoint had no saved wide Steam options. `tools/game/Set-SteamTripleLaunch.ps1`
previews the normal 7680x1440 borderless span; `-Apply` requires Steam closed.
Do not restart Steam while the owner's Woden session owns the screen. This
correction was prepared, not applied or visually retested. Do not say the rig is
ready in triples until this pending correction is completed. Preserve 70-degree
geometry and existing tuning; a display fix is not grounds to repeat the route.

See the consumer `docs/STAGE-PLAYBACK.md` and
`docs/reviews/2026-10-05-unattended-playback.md` for exact evidence and limits.
The launcher is currently source-checkout tooling; the installed generic
`Stage-Session.ps1` alone does not provide supervision/restoration.

## Woden: accepted reference, background replay and span startup

Canonical checkout: `E:/Source/games/dbce-mods-woden-rally-edge`, main. The combined
installed runtime is `0.2.14+8678f06fa967e0985d963315b72ef879c79e3235`, player
archive session-span.2, SHA-256
`53D3115372CEFF76F0969B12250A31B91AB864DF36EEE6D637ECF617ED8D007A`.
Earlier accepted and session-focus.1 runtimes are backed up. The owner accepted Kenya
SS1 / car 8 route and cameras after 3,601 poses passed
(maximum error .00006103515625 m / .0000172453823 degrees). Native cold startup,
normal exit and exact restoration passed. Original force analysis retained
3,553 valid / 3,477 nonzero samples, with zero numerical error and no physical
output attempts. That owner run was single-screen. After a foreground prompt
exposed the old eligibility failure, core 0.2.1's explicit ReplayActive mode
scoped background execution to validated supervised muted replay. The October 6
02:05 session-span.1 run acquired naturally unfocused and completed all 3,601
steps unfocused, with inspected 7680x1440 triple-view frames, normal exit, exact
restoration and zero force writes/Forza sends. Session-span.2 then promoted the
separate-monitor span default; explicit saved opt-outs remain intact. Its 13
installed payloads were independently verified. Normal wheel input, force and
owner recording retain their focus gates.

Plain Steam startup passed on separate monitors at 03:24. The 11:34 round trip
passed borderless 7680x1440 at (0,0) on Surround, then (-2560,0) after returning
to separate monitors; Claude reported both normal exits. Pre/post/current registry
exports match byte-for-byte and all 55 original LocalLow files match. Startup
backgrounds still extend onto the sides, so centre-only front-end/loading/panel
coverage remains incomplete. Focus transitions/interruption checks and the wider
public-release/owner-drive checklist remain open; no repeat launch is needed to
re-establish these passed startup cases.

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
The exact session-span.2 player package passed 111 stock Windows PowerShell
5.1 installer checks, is installed, and is not published. See consumer
`docs/PUBLIC-RELEASE.md` and `docs/2026-10-06-span-qualification.md` for the
private receipt path, full payload hashes and acceptance limits.

## DRIVE Rally: accepted owner recording and cold playback

**October 5, 20:43 CT:** first owner route captured and replayed on unchanged
session-8 runtime: Holzberg/Sideways D, 60 seconds/1,331.724 m, all 6,001 poses,
max error 0.0001220703125 m / 0.000019014441926092303 degrees. Original solver
and presentation rows all aligned; 20,792 stock effect requests retained, no
drops/errors. UDP was disabled, so no encoded packet evidence. Both runs closed
normally and restored owner files/raw preferences at 2560x1440 fullscreen.
The owner confirmed that the route, camera and HUD matched. Physical forces
stayed muted; torque/normalization and broader interruption tests remain open.

```powershell
Set-Location 'E:/Source/games/dbce-mods-drive-rally'
# Accepted owner reference, game closed, NEW result folder:
./tools/game/Run-StageSession.ps1 -Recording "$env:LOCALAPPDATA/Dbce/StagePlayback/references/drive-holzberg-owner-20261005-203959" -Result artifacts/owner-playback-new-01
# New owner capture; drive for 60 seconds after automatic Free Roam startup:
./tools/game/Run-StageSession.ps1 -Record -RecordBiome Holzberg -Seconds 60 -Result artifacts/owner-record-new-01
```

Owner reference seal SHA-256:
`217E5458493FDE994917BA71D492DE1C1D72DD94A21CF5F41438829B9E1BC7B2`.
The first attempt correctly refused the saved Tofuyama region's Free Roam lock.
Runner `f1e6602` adds recording-only `-RecordBiome`, selected after lease/backup
and restored after exit; native unlock checks remain intact. Replay uses sealed
context without this override. See consumer `docs/reviews/2026-10-05-owner-recording.md`
for exact requests, durable evidence and limitations. No runtime reinstall is
needed for this runner option. Earlier diagnostic evidence follows.

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
Owner driving is now accepted above; Claude separately verified this adapter's
diagnostic playback in moving triples. Broader interruptions remain open. The
single-screen tests preserved the current 2560x1440 ExclusiveFullScreen baseline.

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
does not replace the supervisor or owner-state backup/restore wrapper. iRacing's
qualified repack.114 now has current-request output-mute heartbeats and identified
process supervision. Woden has not adopted that heartbeat; do not retrofit forced
termination by assuming its physical outputs are off.

### Sonic CrossWorlds preparation

**October 6 owner decision:** UE4SS + SonicTriple may ship for online play and
the renderer is now retained in the game. Its 20:44Z installation has ten
independently verified receipt-owned files. The renderer's earlier offline-only
restriction is retired; this does not authorize online probe/replay automation
or handling changes. Recording discovery remains bounded and offline. Use the
existing renderer's addon workflow and borrowed lease from consumer
`docs/STAGE-PLAYBACK.md`; remove only probe-owned files and preserve the renderer.
The development install also contains diagnostics, which need separate treatment
in a player renderer package. Exact identity/scope: consumer
`docs/2026-10-06-renderer-scope.md`.

Sonic's newer UE4SS loader now initializes, as observed during the separate triple
work. `games/dbce-mods-sonic-crossworlds` includes a bounded
game-thread observer with stage/install/arm/stop/removal scripts. Native startup
reached E-Stadium's offline Time Trial Ready/loadout with Tails and SonicSpeed.
Native footer Next opens a disabled-gadgets warning. Verified Yes focus plus
Accept returns to Ready; native Right and Down leave No focused and withhold
Accept. Four later parent/button dispatch variants also failed to enter a race;
latest `bd519c9` returned to Ready after direct native Yes press/release. Claude's
separate virtual-pad route reached E-Stadium (`64fbeb9`); next use the passive
addon during that known route, as documented in the consumer. Native startup
remains unresolved. Steam launch, native quit and exact UE Config/SEGA save restoration
pass. Nineteen Lua, 24 setup, 14 addon, 13 owner-file and five process-identity
checks pass. There is no qualified driving recording or in-game replay yet.
Follow that consumer's `docs/STAGE-PLAYBACK.md`; keep its discovery data distinct
from a physics-tick trajectory. Its older
companion input scheduler only proves report scheduling, not a reproduced race.

The probe uses a cancellable native game-thread timer for sampling, completion
and quit. It cancels before finalization and rejects late callbacks. An earlier
LoopAsync/queued-quit design crashed in Lua during finalization; the exact cause
remains unproven. The replacement passed both a 30-second smoke and 180-second
startup observation. Missing timer APIs refuse before claiming a request; a
stalled game thread is handled by the external runner's retained-lease deadline.
Missing fields and original force data remain unavailable. Discover the actual
car tick, mode/startup authority and competing body writers before implementing
an active player; do not infer them from a camera hook or the first found pawn.

Main `bce2223` adds `Stage-SessionProbe.ps1 -Addon` for an existing UE4SS install:
only the Lua script and supported `enabled.txt` marker are payloads. Loader,
settings and other mods remain exact dependencies. Fourteen addon fixture checks
pass; removal preserves later owner mod-list edits. This avoids replacing the
triple session's newer paired loader with the older diagnostic stage.

After staging/installing the current probe in a closed-game slot, run this from
the Sonic checkout to inspect the guarded offline startup path:

```powershell
./scripts/Run-StageProbe.ps1 -Result obj/probe-NEW -Seconds 180 -InspectStartup
```

The runner owns the shared lease, creates an expiring request, launches through
Steam, and restores owner files only after normal close. It recognizes the
known bootstrap and exact new shipping child. Omit `-InspectStartup` for passive
observation. A deadline leaves the game and lease for normal-close recovery with
`-RestoreOnly -Result <same-folder>`; no force-kill or display change is performed.
Remove the exact receipt-owned stage afterward. For a coordinated observation
alongside another session's race test, arm the addon without `-AutoExit` and let
that session own launch/close. The operator instructions do not qualify car hooks.

Saves are in `%APPDATA%/SEGA/SonicRacingCrossWorlds/steam/<id>`, separate from
`%LOCALAPPDATA%/UNION/Saved/Config`. Both roots are backed up and hash-validated
before either is restored. New/post-run files are retained privately in evidence.
Local restoration is not a Steam Cloud transaction guarantee. Completed probe
evidence, including owner-file backups, was archived separately from source:
36 restored runs in the 2,483-entry `sonic-startup-20261005-1552.zip`, plus four
follow-ups in the 352-entry `sonic-popup-dispatch-20261005-164246.zip`. All entry
hashes were verified; originals remain. The earlier crash is retained as a
recovered failure, not a normal-exit pass. See the consumer review for exact
archive identities. These private owner saves/configuration are not release data.

### OutRun 2006 native intake

Consumer master `c0e5a03` documents source-verified native simulation/scenario/
output boundaries in `docs/STAGE-PLAYBACK.md`. The existing 128-frame (~2s) legacy
force recorder and production-linked x86 record/read/recalculate tests passed
again. External runtime capture and gameplay playback do not exist yet. FFB Off
skips calculation, the car hook is pre-native, and Plugin_Init runs under loader
lock: preserve the calculation stream when adding output mute, observe actual
physics/presentation ownership and start/join writers at normal outer boundaries.
Unity hooks are not a native x86 adapter.

Later startup-crash investigation found the launched copy lacked all 80
Scripts/bin files and 30 BK/Bin assets. The crash caller was an OutrunMiles
script-table lookup. Only missing files were restored from the owner's matching
local installation, checked against another copy; 31 existing root/save hashes
were preserved. Read-only preflight now validates the exact EXE and 110 assets:

```powershell
./tools/Test-GameData.ps1 -GameDir '<actual Stream Deck game directory>'
```

See consumer docs/STARTUP-ASSETS-2026-10-05.md for the receipt. All asset hashes
pass. Claude's post-repair test reached title at 7680 and exited normally; 30
non-log root/save hashes remained exact and the new runtime log is preserved.
At that checkpoint native title confirmation/offline driving were unqualified.
No game bytes are committed.
Do not diagnose this as a renderer/FFB-hook regression or add a native null-pointer
workaround before retesting the intact installation. Lesson 31 covers preserving
runtime game data even when its directory is named bin.

October 6 installed runtime `f021736` subsequently passed native title/Single
Player menu entry using the exact private review package, temporary pinned pad
and A=0/Start=7 mappings; driving axes were disabled. Claude reported normal exit
0. Three installed payloads and eight original owner files independently match.
The TS-UFB01B-X device was confirmed by connected/disconnected enumeration to be
the tooling's virtual pad: name filtering had missed it and automatic selection
applied saved wheel mappings 31/34. Preserve the owner's mappings and use explicit
temporary selection for diagnostics. The bounded check and exact hashes live in
consumer `docs/2026-10-06-focused-input-followup.md`. Offline race entry and
read-only phase/pose discovery are next; gameplay recording/playback still does
not exist, and one prior intermittent freeze remains unexplained. Do not repeat
the title-only check or infer a force stream from this output-disabled menu run.

- Shared code: `dotnet/Dbce.Wheel.Playback`, tests alongside; now merged to the default branch, separate unpublished 0.2 pin (source
  `50b6b679e3c6e01025b6a50f8dd0f3eb85d9d4bb`). Art uses the
  earlier 0.1 pin intentionally; a documentation update does not require repinning.
- Run `dotnet run --project dotnet/Dbce.Wheel.Playback.Tests -c Release` for
  device-free core coverage. These checks never establish live game support.
- Runner adapters can vendor `tools/playback/Stage-RunnerLifecycle.ps1` with a
  source/hash pin. Resolve every caller path through `Resolve-StageFileSystemPath`
  before use: .NET process CWD can differ from PowerShell Set-Location/Start-Job.
  `Get-StageProcessSnapshot` separates closed-during-read from unavailable identity;
  `Get-StageProcessDisposition` refuses changed PID/start/image or multiple processes.
  Run `Test-StageRunnerLifecycle.ps1` for offline coverage.
  In a finally path, use `Complete-StageSession` with game-specific evidence,
  watchdog, disarm, restore and lease callbacks. It waits boundedly for two closed
  observations, always attempts evidence, restores despite ancillary errors, and
  retains the lease if the game remains/identity is unknown or recovery fails.
  It never stops a game or sends input. Do not turn a route pass into a whole-run
  pass if shutdown, watchdog or restoration failed.
- Shared lessons and this runbook are indexed from toolkit AGENTS.md, CLAUDE.md,
  STATE.md and knowledge/README.md. Update the evidence/status after each new
  adapter's live qualification; keep failures, source identity and limits visible.
