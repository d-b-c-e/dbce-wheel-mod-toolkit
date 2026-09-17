# Attended effect comparison (developer diagnostic)

This standalone Win32 program compiles the frozen c319b02 production adapter
into its executable. It does not load, replace or deploy the game's DLL. RC4,
its source, and its installed settings remain unchanged. No optional D test.

The developer has built this UI but has **not launched it or applied hardware
output**. Visual layout, real-driver readbacks and physical feel await the
owner's attended run. API acceptance/echo/PLAYING do not measure wheel torque.

## Owner-operated sequence

1. Close all games. Run the x64 `attended_effects.exe` when ready at the wheel.
   Opening the UI does not enumerate, acquire or apply force.
2. Click **Find wheels**, explicitly choose the wheel's name and full instance
   GUID, then click **Connect selected wheel**. This acquires exclusively and
   creates idle effects only. Missing identity refuses fallback. Connect is
   blocked if the original autocenter property cannot be read for restoration.
3. Leave **5%** selected initially. Click one test and note what you feel.
   Every button click requests one finite **120 ms** effect; no automatic
   sequence, replay, retries, or escalation. Wait for the one-second cooldown.
   10% and 20% are optional owner choices, capped at 20%.
4. **STOP / Disconnect**, closing the window or loss of focus stops effects and
   disconnects. Returning focus never reconnects or plays anything.

| Test | Output |
|---|---|
| A | Finite constant X pulse, working signed-magnitude/direction-length convention, matching the constant effect's one/two axis layout. |
| B | Production shaped sine alone: 6.25 Hz, phase 9000, full 120 ms fade. Production constant effect explicitly stopped first. |
| C | Same sine, with production `SetDeviceForcesXY(0,0)` approximately every 20 ms. No nonzero steering stream. |

A/B/C match **peak and duration**, not impulse/signed area. A's nonzero constant
has a driver-owned 120 ms lifetime checked by parameter echo before Start(1,0).
B/C use the unmodified finite production burst. The UI timer is supplemental
cleanup, not what limits nonzero force if callbacks stall. No SOLO or global
FFGAIN writes. Autocenter is transiently disabled by the production acquisition
path and restored to the read original value when it differs, including failed
acquisition cleanup; no saved wheel profile is edited.

The guard blocks known game names (art of rally, OutRun2006, Gravel, iRacing
Arcade) and accessible executable paths under Steam `steamapps/common` or
`Epic Games`. It logs the blocked process. This can include game helpers and
cannot identify every possible game installation; close other games manually.
Process-enumeration failure blocks tests. A game found before a test also
causes disconnection. Acquiring/starting requires this window to be foreground;
the finite constant rechecks focus immediately before Start.

## Evidence

The window shows the log path:
`%LOCALAPPDATA%\DbceWheel\attended-effects-YYYYMMDD-HHMMSS-PID.log`.
It records full device identity, actuator-object flags, device gain (read-only),
sine capabilities, effect axis/direction/gain/timing echo, raw query HRESULTs
and latency, and PLAYING/EMULATED status immediately after Start and before
and after C's zero updates. Device-state flags can be stale or unsupported;
PAUSED/STOPPED are not universally reliable. Unsupported reads remain failures,
not assumed zero/valid values.

C's production update export returns a boolean, not its raw SetParameters
HRESULT. The retained last-HRESULT is explicitly labelled **not the per-update
result**. It may be stale after success. No production behavior is modified to
instrument it. All samples include monotonic QPC time; log zero-update count
and actual timing before drawing a conclusion. Slow queries may mean no C
update fits before expiry; the UI calls that comparison inconclusive.

At most 256 records buffer in memory, with a dropped-record count. There is no
per-update disk write during an active test; records flush after stopping or
while idle. Production file logging is disabled in this process only. Query
latency itself can affect timing, so this is a diagnostic, not a force benchmark.

Useful interpretations (hypotheses, not automatic diagnoses):
- A felt / B not felt: investigate periodic routing/driver rendering.
- B felt / C not felt, with status disappearing across zero updates: investigate
  concurrent-effect handling.
- PLAYING maintained without felt force: status still does not establish motor
  output; use the axis/gain/filter evidence before changing strength.

## Build and offline checks

`build.bat x64` (or `x86`) compiles **without launching** the UI.
`test.bat x64` / `x86` runs only fake policy/dispatch/effect tests: default/cap,
explicit click, focus/game/cooldown gates, baseline stop before A/B/C, C-only
zero dispatch, exact finite constant axis/magnitude/direction/gain, one-shot
non-SOLO Start, duration expiry without callbacks, invalid/failed/adjusted
readback cleanup, and last-moment focus rejection. Frozen production burst
fake tests also pass on both architectures. No hardware smoke is run.

The handoff manifest identifies the exact source, executable and archive hashes.
There is no toolkit package/version bump, production export change, public
release, GitHub Actions run, or push in this task.
