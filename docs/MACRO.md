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
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> [-Device wheel={guid}]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -Launch <exe> [-LaunchWaitSeconds 60] [-LeasePath p -LeaseToken t]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -DryRun     # print the injection lines only
```

Fenced controllers are read in the background, so focus is optional; with a rig lease the player asks for the
game's foreground through `tools/playback/Stage-GameForeground.ps1`. It stops if the game process exits, and a
malformed macro fails (exit 1) before anything is sent. Tests: `tools/macro/Test-Macro.ps1` (offline) and the
recorder section of `native/dinput-proxy/test_fence.cpp` (`build_ffb_layer.ps1`), which share
`tools/macro/fixtures/recorded.macro`.

## Adopting it in a game proxy

Re-vendor `native/dinput-proxy/dbce_fence.hpp` and the new `dbce_macro_record.hpp` (same folder) into `src/vendor`,
then, where the proxy sets up the fence:

```cpp
static dbce::macro::Recorder g_recorder("<game>");          // the %LOCALAPPDATA%\dbce folder name
if (g_fence) dbce::fence::settings().recorder = &g_recorder;
```
