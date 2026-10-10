# Game-window capture

Uses Windows Graphics Capture for one visible window owned by the specified
process. No desktop capture, focus changes or injected input. Build with
`dotnet build tools/window-capture -c Release`.

```powershell
dbce-window-capture <pid> --list
dbce-window-capture <pid> centre.png --hwnd 0x12345 --width 1280
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
