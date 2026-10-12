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

## Per-frame brightness diagnostics

`--metrics` selects a separate optional burst mode. It copies and samples each
processed delivered frame **before** any PNG encoding. Use the same exact PID and
observed HWND selection as above; output is again a fresh directory.

```powershell
dbce-window-capture <pid> fresh-metrics-folder --hwnd 0x12345 --metrics --duration 10 --max-frames 12 --width 1920
dbce-window-capture <pid> fresh-metrics-only-folder --hwnd 0x12345 --metrics --duration 10 --no-png
```

The capture ends at the requested duration (default 5, maximum 30 seconds) or
12,000 processed metric rows. It keeps the same four WGC buffers and two-frame
queue. GPU-to-CPU copying and analysis can still lose delivered frames to queue
overflow. This mode does not promise every game frame or improved throughput
until measured against a live target.

`metrics.jsonl` contains one row per processed frame, including the WGC timestamp,
callback UTC/elapsed time, processing UTC, analysis milliseconds, dimensions, and
left/centre/right third metrics. Each third uses up to 256 columns by 144 rows,
sampling one pixel at each stratum's centre. The exact thirds cover the full
source width, including widths not divisible by three. Empty thirds in a one- or
two-pixel image have zero samples and null measurements. Metrics use source
pixels regardless of `--width`, which affects only saved PNGs.

- `Mean` and `Max` are sampled BGRA8 luma code values, 0–255, using
  `(54 R + 183 G + 19 B + 128) / 256` with integer rounding. They are not linear
  light or physical brightness; `Max` is not an exhaustive image maximum.
- `WhiteFraction` is the fraction of samples with **all** RGB channels at least
  235; alpha is ignored. Small bright features can fall between spatial samples.
- Side threshold crossings are changes in either left or right white fraction's
  above-threshold state, in both directions. `--white-fraction` sets the threshold
  (default 0.1, finite and greater than zero, at most 1). Centre-only white does
  not trigger a side representative. The first frame records its state without
  inventing a crossing from an unobserved prior frame.

PNGs are optional. When enabled, retain first, last, and threshold-crossing frames
up to `--max-frames` (2–300, default 300) and a **256 MiB retained-bitmap budget**.
Two slots are reserved for first/last; earliest crossings fill the remainder.
The current copy, WGC buffers, encoder and metric rows are additional memory.
If two source bitmaps cannot fit, the run fails with advice to use `--no-png`.
PNG encoding starts only after capture stops, so it can extend total command wall
time beyond 30 seconds. Images are named by metric row index, and each saved
row's `Image` links to its PNG. `--no-png` retains all processed metrics without
holding representative bitmaps. Without `--metrics`, existing single/burst
behaviour and manifest schema are unchanged.

`capture.json` uses `dbce.window-metrics@1`. It reports processed/delivered counts,
the existing explicit queue/drop counters, observed side crossings, representative
capacity and crossings omitted from the reserved crossing slots, saved image
hashes, and `skippedObservedFrames = delivered - processed`. A crossing omitted
from a crossing slot may still be the final image. `captureServiceDroppedFrames`
remains null: Windows pre-delivery losses are unknown. Timestamp intervals over
50 ms and non-increasing timestamps are counted; intervals are **not** converted
to invented dropped-frame counts. No result proves an interval was flash-free.

Target identity, visibility and size checks remain active. A target failure keeps
processed rows and a failed partial manifest; it can prevent any representative
PNG saves. The same exact live target is checked before every PNG save. Use the
manifest status and counts, not image presence, to judge whether capture completed.

## Offline checks

```powershell
dotnet build tools/window-capture -c Release -warnaserror
dotnet run --project tools/window-capture-tests -c Release
```

The executable harness source-links the option parser, bounded inbox, pixel math
and representative ownership. It
checks limit refusal, legacy single/list arguments, overflow disposal, exact
consumer ownership, terminal cleanup, the single-frame loser case and concurrent
stop/callback accounting. Synthetic checks cover black, white, centre-only white,
RGB coefficients, channel thresholds, padding/plane offsets, boundary widths,
bounded sampling and first/last/crossing retention/disposal. It opens no graphics,
game, window or device. Build and 126 checks pass. Metrics-mode WGC pixel interop,
throughput and real transition qualification remain pending a coordinated capture;
the existing single/list/burst modes were not rerun live in this source-only task.
