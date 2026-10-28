# Input macros: record once, replay to reach gameplay

For unattended tests in any game whose dinput8 proxy uses the fence (`native/dinput-proxy/dbce_fence.hpp`). The owner
plays the route once with the real wheel while the proxy records it; agents then replay the file through the fence's
test injection with its recorded timing, with no screen reads between steps. Check the result once at the end.

## Format

Plain text, one item per line; `#` starts a comment.

```
dev wheel {d71b8350-61b7-11f1-8001-444553540000}     # a name for a device instance GUID
0.500 wheel button 31 1                              # seconds, device, kind, index, value
0.800 wheel button 31 0
1.000 wheel hat 0 18000
+2 wheel button 31 1 ms=300                          # "+" = seconds after the previous event; ms= = explicit hold
1.500 wheel axis 0 16384 range=-32768..32767         # an axis value in that range (default 0..65535)
```

- Time: seconds from the start of playback (the recorder: from the moment `record.on` was seen), non-decreasing.
- Device: a name from a `dev` line, a name given to the player with `-Device name={guid}`, or a GUID itself.
- `button <0-127> <1|0>` down or up; `hat <0-3> <0-35999|-1>` angle or centred; `axis <0-7> <value> [range=a..b]`.
- A down, an angle or an axis value is held until the same object's next event, or for `ms=` (50-15000). Up and
  centred send nothing: the real device is at rest. Holds are cut at 15 s (the injection limit).
- Keyboard keys are not supported: the fence neither records nor injects the keyboard.

## Record (the owner)

1. `%LOCALAPPDATA%\dbce\<game>\ffb-fence.on` must exist when the game starts (no force reaches the wheel while fenced).
2. Create `%LOCALAPPDATA%\dbce\<game>\record.on` (before launch to keep the launch timing, or any time later).
3. Play the route. Delete `record.on` to stop; each start writes a new `macro-<yyyyMMdd-HHmmss>.txt` in that folder.
4. Optionally rename `d0` in its `dev` line to a role (`wheel`) and trim the tail.

## Play (an agent)

`inject.on` and `ffb-fence.on` must have existed when the game started.

```
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -ProcessId <pid> [-Device wheel={guid}] [-LeasePath p -LeaseToken t]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -Launch <exe> [-LaunchWaitSeconds 60]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -Launch <appid|steam://...> -GameExe <game exe> [-LaunchWaitSeconds 60]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -DryRun     # print the injection lines only
```

- `-Game` is a plain folder token (letters, digits, `-`, `_`). Times must be finite and at most one hour.
- The game is identified by PID and creation time (`tools/playback/Stage-ProcessIdentity.ps1`); a Steam launch follows
  the one new `-GameExe` process, not Steam's. Before every batch and at least every second of a wait, the identity
  and (with `-LeasePath`) the rig lease are rechecked; on loss playback stops.
- Focus is opt-in: fenced controllers are read in the background. `-RequireForeground` (needs the lease) makes one
  `Stage-GameForeground` request and stops, sending nothing, if foreground is unmet.
- When playback stops early, every hold still running gets a 50 ms release sample; a complete run waits for its
  last hold to end. A malformed macro fails (exit 1) before anything is launched or sent.
- The last stdout line is a JSON result:
  `{"macroDispatched":true,"stepsSent":12,"stepsTotal":12,"stoppedReason":null,"gameplayReady":null}`. Exit 0 only
  when `macroDispatched`. `gameplayReady` is always null here: the player only knows what it sent. The game's own
  adapter (a frame dump, telemetry or log line) supplies the witness that gameplay was reached.

Tests: `tools/macro/Test-Macro.ps1` (offline, a sleeping pwsh stands in for the game) and the
recorder section of `native/dinput-proxy/test_fence.cpp` (`build_ffb_layer.ps1`), which share
`tools/macro/fixtures/recorded.macro`.

## Adopting it in a game proxy

Re-vendor `native/dinput-proxy/dbce_fence.hpp` and the new `dbce_macro_record.hpp` (same folder) into `src/vendor`,
then, where the proxy sets up the fence:

```cpp
static dbce::macro::Recorder g_recorder("<game>");          // the %LOCALAPPDATA%\dbce folder name
if (g_fence) dbce::fence::settings().recorder = &g_recorder;
```
