# Input macros: record once, replay to reach gameplay

For unattended tests in any game whose dinput8 proxy uses the fence (`native/dinput-proxy/dbce_fence.hpp`). The owner
plays the route once with the real wheel while the proxy records it; agents then replay the file through the fence's
test injection with its recorded timing. No screen analysis: at chosen points an `expect` checkpoint compares one game
frame with a stored reference image and stops the run on a mismatch, so a swallowed press cannot walk a test into a
forbidden menu (such as online multiplayer).

## Format

Plain text, one item per line; `#` starts a comment.

```
dev wheel {d71b8350-61b7-11f1-8001-444553540000}     # a name for a device instance GUID
0.500 wheel button 31 1                              # seconds, device, kind, index, value
0.800 wheel button 31 0
1.000 wheel hat 0 18000
1.300 wheel hat 0 -1
1.500 wheel axis 0 16384 range=-32768..32767         # an axis value in that range (default 0..65535)
+2 wheel button 31 1 ms=300                          # "+" = seconds after the previous line (3.500); ms= = explicit hold
+1.5 expect title.bmp 20                             # checkpoint: a reference beside the macro, max mean difference
+2 expect main-menu.bmp 15 region=0.05,0.3,0.35,0.7  # only that crop of the frame (the menu's highlighted items)
```

- Time: seconds from the start of playback (the recorder: from the moment `record.on` was seen), non-decreasing,
  finite and at most one hour.
- Device: a name from a `dev` line, a name given to the player with `-Device name={guid}`, or a GUID itself.
- `button <0-127> <1|0>` down or up; `hat <0-3> <0-35999|-1>` angle or centred; `axis <0-7> <value> [range=a..b]`.
- A down, an angle or an axis value is held until the same object's next event, or for `ms=` (50-15000). Up and
  centred send nothing: the real device is at rest. Holds are cut at 15 s (the injection limit).
- `[<seconds>] expect <reference.bmp> [maxDiff] [region=x0,y0,x1,y1]`: a checkpoint (without a time, right after the
  previous line); the region is in 0-1 frame coordinates, top-left origin. Give
  the screen time to settle first, as in the example. The lines after a checkpoint keep their spacing. Place checkpoints only after every hold and key has been released:
  the capture wait delays scheduled releases, so a 300 ms press would stay down while the frame is fetched.
- `[<seconds>] key <scan> [ms=<50-15000>]`: a keyboard key by hardware scan code (`0x1C` or `28` is Enter; plain
  codes 0x01-0x7F, no E0-extended keys), held for `ms` (default 300), sent with `SendInput` and `KEYEVENTF_SCANCODE`.
  For screens that ignore the wheel (GRID 2019's title "PRESS A" takes only Enter). Keys go to whatever window is in
  front, so a macro with a key step is refused unless the run has `-RequireForeground` (dry runs too), and each key
  send follows a passing lease, identity and foreground-window check. A key still down when playback stops gets its
  key-up in the same cleanup as holds. The recorder does not record keys; add them by hand.
- Checkpoint frames come from the game's `frame-dump.req` responder (the dinput proxy). For a game without one
  (BepInEx/Unity, UE4SS), `-FrameSource WindowCapture` takes one PNG of the game's largest visible window with
  `tools/window-capture` (build it first; no focus change or input) and converts it to the same 24-bit BMP. It needs
  `-ProcessId` or `-Launch`, also for `-RecordCheckpoint`. Record references with the same source you replay with.
- `[<seconds>] cmd <text>`: appends the rest of the line (comment and outer spaces trimmed) plus CRLF to the file given
  by `-CommandFile`, for a game whose menus take no wheel or key input but has its own command channel (ADRIFT's UE4SS
  dev channel `cmd.txt`). Refused without `-CommandFile` (dry runs too). Each append follows a passing lease and
  identity check; no foreground is needed. A macro with only `cmd` steps needs no `ffb-fence.on`/`inject.on`. The text
  is not interpreted: a game-side name lookup belongs in the game's channel, not in the macro.

## Record (the owner)

1. `%LOCALAPPDATA%\dbce\<game>\ffb-fence.on` must exist when the game starts (no force reaches the wheel while fenced).
2. Create `%LOCALAPPDATA%\dbce\<game>\record.on` (before launch to keep the launch timing, or any time later).
3. Play the route. Delete `record.on` to stop; each start writes a new `macro-<yyyyMMdd-HHmmss>.txt` in that folder
   (never an existing file; a taken name gets `-2`, `-3`, ...).
4. Optionally rename `d0` in its `dev` line to a role (`wheel`), trim the tail and add checkpoints.

## Play (an agent)

`inject.on` and `ffb-fence.on` must have existed when the game started.

```
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -ProcessId <pid> [-Device wheel={guid}] [-LeasePath p -LeaseToken t [-RequireForeground]]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -Launch <exe> [-LaunchWaitSeconds 60]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -Launch <appid|steam://...> -GameExe <game exe> [-LaunchWaitSeconds 60]
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -DryRun     # print the steps only
```

- `-Game` is a plain folder token (letters, digits, `-`, `_`).
- The game is identified by PID and creation time (`tools/playback/Stage-ProcessIdentity.ps1`); a Steam launch follows
  the one new `-GameExe` process, not Steam's. The identity and (with `-LeasePath`) the rig lease are checked at least
  every second of every wait (launch waits, the Steam follow loop, the steps and the final hold) and right after the
  last sleep before every send; on loss playback stops.
- Focus is opt-in: fenced controllers are read in the background. `-RequireForeground` (needs the lease) brings the game to
  the front once at startup (below); if that is still unmet after the one fallback, playback stops before any macro
  step is sent (the fallback's own F24 press is the only input). Afterwards every check also
  observes (read-only, never re-requested) that the same game window is the foreground window and belongs to the game.
- Foreground at startup, once, in this order (`tools/macro/MacroForeground.ps1`): the game window already in front;
  else the ordinary `Stage-GameForeground` request (input-free); else a reobservation; else ONE activation-key
  fallback, because unattended launches on the rig never get focus (foreground lock timeout at its maximum; novi
  found that one F24 press through `SendInput`, then `SetForegroundWindow`, brings the game to the front). The
  fallback first revalidates the lease, the game's birth time and path, and that its exact window is visible and not
  minimized; it sends nothing if Shift, Ctrl, Alt, Windows or F24 is already held; it sends F24 down and up in one
  `SendInput` call and releases F24 if only one event went in or the call failed; then `SetForegroundWindow` on that
  window. Success comes only from observing that exact window in front, never from an API return. It is logged
  (attempted, events inserted, result) and reported as `foreground` in the JSON result; it is never retried, and
  nothing activates after playback starts (losing focus then aborts through the guarded waits).
- `SendInput` is global input. We cannot claim that no key reaches another window, nor that F24 is unbound in every
  program: the F24 press goes to whatever is in front at that moment. It is a deliberate, minimal, logged exception,
  only with `-RequireForeground`, and only after the ordinary request is genuinely unmet. No timeout, policy or
  elevation changes.
- `-DeliveryLog <the mod's log>`: stop sending when input is not reaching the game. Before each inject step (button,
  hat, axis) the player notes the log's length; the step then needs a line written after that point naming its kind,
  index and device GUID (case-insensitive; lines with "refused" or "not delivered" never count) within its ms + 1000.
  DiRT 3 logs `inject: "inject raw button 31 dev={...} value=1 ms=300" -> button 31 sample 1 on {...} for 300 ms`;
  GRID's pad path `inject (pad): inject: button N on {...} ended`. A miss stops with `step N not delivered` (N counts
  the macro's steps in time order), releases running holds and sends nothing further. It is checked in every guarded
  wait, so a later step can go out before an earlier step's deadline. The result adds `stepsDelivered`. Key steps
  are not checked here (they keep the foreground guard).
- When playback stops early, every hold still running gets a 50 ms release sample; a complete run waits for its
  last hold to end. A malformed macro fails (exit 1) before anything is launched or sent.
- The last stdout line is a JSON result:
  `{"macroDispatched":true,"stepsSent":12,"stepsTotal":12,"checkpointsPassed":2,"checkpoints":[{"name":"title","diff":4.2,"maxDiff":20,"passed":true},...],"stoppedReason":null,"gameplayReady":null}`.
  Every checkpoint run, passed or failed, reports its measured `diff`, so limits can be tuned from evidence.
  Exit 0 only when `macroDispatched` (every step sent, every checkpoint matched, the last hold over).
  `gameplayReady` is always null here: the player only knows what it sent. The game's own adapter (a frame dump,
  telemetry or log line) supplies the witness that gameplay was reached.

## Checkpoints

The game mod must answer `%LOCALAPPDATA%\dbce\<game>\frame-dump.req`: it deletes the request and writes
`frame-HHMMSS-<tag>.bmp` (24-bit, any size; DiRT 3 writes quarter size through the triple-screen toolkit's
`dbce_frame_dump_d3d11.hpp`) in the same folder. The player creates the request, takes the first BMP written after it
(waiting up to 5 s; an unanswered request is removed), reduces both images to a 64x12 grid of cell means (with a
`region`: that crop only, on a finer 48x24 grid) and compares the mean absolute difference per channel (0-255; default
`maxDiff` 20). A whole frame cannot tell which item of a menu over an animated background is highlighted (DiRT 3
passed its main-menu checkpoint at 30 and then drifted into the credits): give menus a region around their items. A mismatch stops the run with `stoppedReason`
`checkpoint <name> mismatch <value>` and releases running holds; no frame is `checkpoint <name> failed: ...`.

Make a reference once, with the game on the screen the checkpoint expects (an agent or the owner):

```
pwsh tools\macro\Play-Macro.ps1 -Macro <file> -Game <folder> -RecordCheckpoint title   # writes title.bmp beside <file>
```

It never overwrites an existing reference. Look at the BMP once before relying on it.

## Tests

`tools/macro/Test-Macro.ps1` (offline: a sleeping pwsh stands in for the game, and another answers frame requests with
synthetic BMPs) and the recorder section of `native/dinput-proxy/test_fence.cpp` (`build_ffb_layer.ps1`), which share
`tools/macro/fixtures/recorded.macro`.

## Adopting it in a game proxy

Re-vendor `native/dinput-proxy/dbce_fence.hpp` and the new `dbce_macro_record.hpp` (same folder) into `src/vendor`,
then, where the proxy sets up the fence:

```cpp
static dbce::macro::Recorder g_recorder("<game>");          // the %LOCALAPPDATA%\dbce folder name
if (g_fence) dbce::fence::settings().recorder = &g_recorder;
```
