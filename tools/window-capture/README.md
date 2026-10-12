# Game-window capture

Uses Windows Graphics Capture for one visible window owned by the specified
process. Saves a single PNG or a bounded burst from one continuous capture
session. No desktop capture, focus changes or injected input. Build with
`dotnet build tools/window-capture -c Release`.

```powershell
dbce-window-capture <pid> --list
dbce-window-capture <pid> centre.png --hwnd 0x12345 --width 1280
dbce-window-capture <pid> fresh-burst-folder --hwnd 0x12345 --duration 10 --max-frames 300 --width 1920
```

Listing returns JSON with process start time and each visible top-level window's
handle, title, class and desktop rectangle. Select the centre view by its observed
rectangle; never assume the first/largest window is the centre. Separate Unity
displays can be equal sized, and a side view correctly omits the HUD. Retain the
listing with the images. Handles must come from the same live process and are
checked for owner and visibility before and after capture. A stale/foreign handle
is refused. Do not reuse handles across launches.

Without `--hwnd`, the existing largest-window policy remains. Use explicit
selection for games that create multiple rendering windows. `--list` reads only
window metadata and does not start graphics capture. Exclusive fullscreen can
produce a black frame; this tool never changes display settings to work around it.

## Bounded bursts

Either `--duration` or `--max-frames` selects burst mode; output must be a fresh
directory with an existing parent. An existing folder is refused atomically,
including one left by a failed capture. Duration is finite, greater than zero and
at most 30 seconds (default 5); saved frames are limited to 1–300 (default 300).
The first reached bound stops capture. At most the current PNG conversion/write
finishes after the deadline. The duration starts immediately before capture,
excluding D3D setup. `--width` only downscales and preserves aspect ratio.

There are four WGC buffers, a queue of at most two frames, one consumer and at
most 300 metadata records. Queue overflow discards the newest delivered frame
and disposes it immediately. Stop/deadline/failure disposes queued frames; late
callback frames are disposed too. Single-frame mode uses the same ownership
rules, fixing the old unconsumed `TrySetResult` frame leak.

`frame-0001.png` etc. and `capture.json` form the result. The manifest contains:

- Exact process ID/start time and chosen HWND/class/rectangle; the same process
  instance, live owned visible HWND and class are rechecked before **every** save.
- Each saved image's SHA-256, source/output dimensions, compositor timestamp
  (`SystemRelativeTimeTicks`, 100 ns TimeSpan ticks on the system QPC timeline),
  callback arrival UTC/elapsed seconds and PNG completion UTC. Arrival/save UTC
  are not the compositor's render time.
- Saved count and counts for delivered frames, full-queue drops, rejected late
  frames, queued frames discarded on stop and callbacks with no frame.
  `skippedObservedFrames` is delivered minus saved, including an in-flight frame
  lost to failure. Frames discarded internally by Windows before delivery are
  unavailable: `captureServiceDroppedFrames` is explicitly null.
- `complete`/`failed`, end reason and error. Target close/replacement, lost
  identity/visibility, resized content, callback failure, no frame for five
  seconds or no saved frame in the requested duration fail with nonzero exit.
  Retained partial PNGs are evidence, not a successful burst.

The timestamps expose sampled gaps; they cannot prove absence of a flash or blank
between saved frames. PNG encoding/downscaling can skip substantial numbers of
frames. A complete burst means its bounded capture completed, not that every game
or compositor frame was observed. Cursor suppression and border suppression are
best-effort WGC options; the helper never changes focus or display settings.

Microsoft documents the compositor QPC timestamp in
[SystemRelativeTime](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime),
the worker-thread callback in
[CreateFreeThreaded](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.createfreethreaded),
and target replacement in
[Closed](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscaptureitem.closed).
Context7 was unavailable for this source check; these are the official API references.

## Offline checks

```powershell
dotnet build tools/window-capture -c Release -warnaserror
dotnet run --project tools/window-capture-tests -c Release
```

The executable harness source-links the option parser and bounded inbox. It
checks limit refusal, legacy single/list arguments, overflow disposal, exact
consumer ownership, terminal cleanup, the single-frame loser case and concurrent
stop/callback accounting. It opens no graphics, game, window or device. Build and
30 checks pass; real WGC burst/transition qualification remains pending a
coordinated game capture. The existing single-frame/list implementation is
preserved but not rerun against a live window in this source-only task.
