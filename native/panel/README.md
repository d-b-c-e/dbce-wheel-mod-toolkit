# Shared F6 panel for native mods

Internal hosts may call `SetStatusLine(text, StatusTone)` from a worker thread;
`ClearStatusLine()` removes it. The default is hidden. Text is copied into a
255-byte UTF-8 buffer. Use the existing `SessionStatusPresentation` wording and
report Saved only after a complete sealed save; the host owns expiry. This is
a passive banner on the center display, rendered by the existing D3D11/D3D12
path even when F6 is closed. It never takes input or draws a closed-panel
cursor. It adds no recorder, hotkey, config switch, force path or public UI.

Help draws the host's `drawPage(PAGE_HELP, advanced)` content before the small
repository text links. A host can place its separate issue/report action there;
the shared renderer owns the link ordering and styling. This does not change
report preparation, browser admission, input handling or overlay behavior.

The standard **Wheel settings** panel (STD-004, STD-017, STD-022, STD-030; UX-1 in `docs/CONSUMER-UX.md`) for mods that
are native DLLs (DirectInput proxies and similar), drawn with Dear ImGui (`native/third_party/imgui`, vendored, MIT).

| File | What it does |
|---|---|
| `dbce_panel.h/.cpp` | Frame and widgets: title, `View: [Simple] [Advanced]`, Stop FFB, Close, flat tabs in the standard order (Controls, FFB, Cameras, Telemetry, Help; empty pages hidden, Help always), Off/On and choice segments, percent sliders with Default, status rows, bounded groups, compact right-aligned commands, a "Saved" note. Opaque dark panel, one accent; 8/16/32 grid and 16/14/20 text, scaled with the window height (1080 = 100%, never below 100%) times the Help page's UI scale. On a three-screen span it sits on the centre screen (STD-019). Also `Slider` with its own unit (degrees). |
| `dbce_frame_stats.hpp` | STD-023 frame rate: the overlay calls `NoteFrame()` on every present of the game's swap chain; ten-second windows (average fps, 1% low, worst frame) shown under the Cameras page (Help when there is none) and logged every 30 s, with `Config::displayText` (the mod's own words for the display, for example a planar span) or else `Config::triplesOn` when set. Pure statistics tested by `test_frame_stats.cpp`. |
| `dbce_panel_input.h/.cpp` | Input without replacing the window procedure: a thread-local `WH_GETMESSAGE` hook on the game window's thread. F6 toggles, F8 stops force feedback, Esc closes; while open, keyboard, mouse and raw input go to the panel (Alt+Tab, Alt+F4 and Windows keys excepted). Events are queued and drained on the render thread. `DetachInput()` lets go of the window (a host that stops drawing, such as `StopD3D9`), from any thread: the panel closes and F6 and every message reach the game until `AttachInput` again; a message already being handled finishes first (the hook checks its window and handles a message under the same lock). |
| `dbce_overlay_d3d11.h/.cpp` | D3D11 overlay: patches the DXGI swap-chain `Present`/`Present1`/`ResizeBuffers` table entries (found on a temporary device on a hidden window, from a worker thread), adopts the largest swap chain's window (a new chain on that same window, the game recreating it, replaces the old one), draws only while the panel is open and restores the game's render targets, holding no view of the back buffer past its Present (one would keep a released swap chain alive and block its flip-model replacement). A chain it cannot draw on (neither D3D11 nor an enabled D3D12 one) closes the panel and gives the window's input back to the game. D3D12 swap chains are drawn by the optional D3D12 renderer when enabled. |
| `dbce_overlay_d3d12.h/.cpp` | Optional D3D12 support (`EnableD3D12()` before `StartD3D11`): the shared present hook draws D3D12 swap chains with the game's own present queue, captured from `ID3D12CommandQueue::ExecuteCommandLists` (patched on a probe queue); per back buffer an allocator, RTV and fence value; ResizeBuffers waits for the GPU and releases the buffers. Checked offline on WARP (`test_overlay_d3d12.cpp`: the real hooks, a flip-model swap chain, the panel read back from the presented buffer). |
| `dbce_overlay_d3d9.h/.cpp` | D3D9 overlay: `StartD3D9(device, log)` with the game's own `IDirect3DDevice9`, since D3D9 gives every device its own method table (on the heap) and a probe device cannot reach the game's. Replaces that device's Present and Reset entries with the shared loop-safe slot hook, attaches input to the device window on the first Present, draws only while open on the back buffer inside its own BeginScene/EndScene when the device is not lost, and puts back the game's render targets, depth surface and viewport. Reset releases ImGui's default-pool objects first. A pure device is used only when it answers the Get* calls the restore needs (viewport, world/view/projection, a render state), as the Windows 10/11 runtime does against the documented rule; otherwise refused. A new device releases the previous one's ImGui objects and reference at once; `StopD3D9(device)`, before the game releases its device, closes the panel, gives the window's input back to the game and lets go of the device; the next adopted Present takes input again. Not covered: D3D9Ex PresentEx/ResetEx and `IDirect3DSwapChain9::Present`; a recreated device needs another call. Also link `dbce_overlay_d3d11.cpp` and `imgui_impl_dx11.cpp` (the shared hook core). Checked offline on x64 and x86 with the GPU's D3D9 device on a hidden window (`test_overlay_d3d9.cpp`: closed/open/closed pixels, game state kept, Reset at a new size). |
| `test_panel_render.cpp`, `build_test.ps1` | Offline render check on WARP: every page in Simple (and Advanced at 2560x1440) at 1280x720, 1920x1080, 2560x1440, 3840x2160 and 7680x1440, PNGs in `build\` for visual review, with geometry checks (inside the window; centre screen of a span). |

## Using it in a mod

1. Vendor `native/panel` and `native/third_party/imgui` into the mod (as with `forza_packet.h`), compile the panel
   files, `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` and `backends/imgui_impl_dx11.cpp`.
2. At initialization (not in `DllMain`): fill `dbce::panel::Config` (pages the mod offers, `drawPage`, `ffbOn` and
   `stopFfb` when it can stop forces, the canonical ini path), call `Configure`, then `dbce::overlay::StartD3D11(log)`.
   A D3D9 game instead calls `dbce::overlay::StartD3D9(device, log)` once its device exists, and also compiles
   `dbce_overlay_d3d9.cpp` and `backends/imgui_impl_dx9.cpp`.
3. Draw each page with the panel widgets; save settings to the mod's own ini (Wheelkit reads the same file).
4. While `dbce::panel::IsOpen()`, suppress driving input the mod can see (DirectInput buttons, keyboard, mouse).

### Help repository links

Set `Config::modRepositoryUrl` and `Config::wheelkitRepositoryUrl` to the mod and
Wheelkit repository URLs. Help renders each as a small text hyperlink using the
existing secondary font and Dear ImGui's text-link widget. Empty or null values
omit a link, preserving existing callers. These links have no filled button or
full-row hit area; opening a URL requires an explicit click. The existing mod
Help callback and any issue/report action remain separate.

The descriptor strings must outlive the panel, as with `modName`. Rebuild and
vendor the header and implementation together; do not mix compiled Config layouts.
The existing offline panel-render fixture supplies both URLs for its Help frames,
without clicking them. On October 16 the first ADRIFT report consumer compiled
this exact renderer/ordering with its existing DLL target: no reported warnings
or errors, sole Lua export verified. DLL SHA256
`BCADDFEE4A1095D524A7B1CF9CFB5698FCCC5A966A6B5FACBA9A74C75A0E58A1`;
original log `E:/Source/games/.worktrees/mako-adrift-native-report-20261016/build/native-report-20261016.log`.
The fixture and rendered/browser checks were not run. The link renderer adds no
report generator, uploader or issue submission; hosts adopt `native/support`
separately for reviewed report preparation and data-only browser drafts.

## Limits (2026-10-08)

Rendered offline only; not yet seen in a game. No binding capture, wheel/pad navigation of the panel or Settings button
yet. XInput pads are not suppressed while the panel is open. The D3D9 overlay (2026-10-23) is checked offline only;
no D3D9 game uses it yet.

## Living with the Steam overlay (2026-10-09)

Steam's overlay can take our Present hook as its "original". It patches the first bytes of dxgi's Present (5 bytes,
`e9` to a relay), puts dxgi's entry back in the vtable slot, and keeps what it found in the slot (ours) as the function
it calls next. Ours then called dxgi's entry, which jumped back to Steam, until the stack ran out. This killed V-Rally 4
on its first frame; the CrashRpt dumps are in `_archive\2026-10-09\vr4-crash-*`.

- **Installed builds (fb90c22, 80f2456):** each slot hook counts its nesting per thread. On the first re-entry it calls
  the system function with the module file's original bytes put back for that call. Only V-Rally 4 takes this path
  today; the other six panel games show no loop.
- **Review (Astra, hcom 2916/2948, inbox 2026-10-09-astra-overlay-3e7ed72-followup.md):**
  - rewriting live code is not safe against other threads;
  - a restore can overwrite a newer patch;
  - a code copy rebuilt later from disk is not bound to the loaded build;
  - skipping a Present or ExecuteCommandLists is not a valid fallback.

  3e7ed72 (copy from disk with a jump back, no live writes) answers the first two. It is committed but **not
  installed anywhere**.
- **Agreed next version (not started):**
  - relocate instructions with Microsoft Detours;
  - copy them from the live code at install, only when it matches the module file and its headers;
  - otherwise, or if the copy cannot be made, do not install the hook at all;
  - D3D11 first; the D3D12 queue hook stays out unless forwarding is guaranteed;
  - tests count real forwarding calls.
