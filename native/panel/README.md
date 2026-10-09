# Shared F6 panel for native mods

The standard **Wheel settings** panel (STD-004, STD-017, STD-022, STD-030; UX-1 in `docs/CONSUMER-UX.md`) for mods that
are native DLLs (DirectInput proxies and similar), drawn with Dear ImGui (`native/third_party/imgui`, vendored, MIT).

| File | What it does |
|---|---|
| `dbce_panel.h/.cpp` | Frame and widgets: title, `View: [Simple] [Advanced]`, Stop FFB, Close, flat tabs in the standard order (Controls, FFB, Cameras, Telemetry, Help; empty pages hidden, Help always), Off/On and choice segments, percent sliders with Default, status rows, bounded groups, compact right-aligned commands, a "Saved" note. Opaque dark panel, one accent; 8/16/32 grid and 16/14/20 text, scaled with the window height (1080 = 100%, never below 100%) times the Help page's UI scale. On a three-screen span it sits on the centre screen (STD-019). Also `Slider` with its own unit (degrees). |
| `dbce_frame_stats.hpp` | STD-023 frame rate: the overlay calls `NoteFrame()` on every present of the game's swap chain; ten-second windows (average fps, 1% low, worst frame) shown under the Cameras page (Help when there is none) and logged every 30 s, with `Config::triplesOn` when set. Pure statistics tested by `test_frame_stats.cpp`. |
| `dbce_panel_input.h/.cpp` | Input without replacing the window procedure: a thread-local `WH_GETMESSAGE` hook on the game window's thread. F6 toggles, F8 stops force feedback, Esc closes; while open, keyboard, mouse and raw input go to the panel (Alt+Tab, Alt+F4 and Windows keys excepted). Events are queued and drained on the render thread. |
| `dbce_overlay_d3d11.h/.cpp` | D3D11 overlay: patches the DXGI swap-chain `Present`/`Present1`/`ResizeBuffers` table entries (found on a temporary device on a hidden window, from a worker thread), adopts the largest swap chain's window, draws only while the panel is open and restores the game's render targets. D3D12 swap chains are drawn by the optional D3D12 renderer when enabled. |
| `dbce_overlay_d3d12.h/.cpp` | Optional D3D12 support (`EnableD3D12()` before `StartD3D11`): the shared present hook draws D3D12 swap chains with the game's own present queue, captured from `ID3D12CommandQueue::ExecuteCommandLists` (patched on a probe queue); per back buffer an allocator, RTV and fence value; ResizeBuffers waits for the GPU and releases the buffers. Checked offline on WARP (`test_overlay_d3d12.cpp`: the real hooks, a flip-model swap chain, the panel read back from the presented buffer). |
| `test_panel_render.cpp`, `build_test.ps1` | Offline render check on WARP: every page in Simple (and Advanced at 2560x1440) at 1280x720, 1920x1080, 2560x1440, 3840x2160 and 7680x1440, PNGs in `build\` for visual review, with geometry checks (inside the window; centre screen of a span). |

## Using it in a mod

1. Vendor `native/panel` and `native/third_party/imgui` into the mod (as with `forza_packet.h`), compile the panel
   files, `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` and `backends/imgui_impl_dx11.cpp`.
2. At initialization (not in `DllMain`): fill `dbce::panel::Config` (pages the mod offers, `drawPage`, `ffbOn` and
   `stopFfb` when it can stop forces, the canonical ini path), call `Configure`, then `dbce::overlay::StartD3D11(log)`.
3. Draw each page with the panel widgets; save settings to the mod's own ini (Wheelkit reads the same file).
4. While `dbce::panel::IsOpen()`, suppress driving input the mod can see (DirectInput buttons, keyboard, mouse).

## Limits (2026-10-08)

Rendered offline only; not yet seen in a game. No binding capture, wheel/pad navigation of the panel or Settings button
yet. XInput pads are not suppressed while the panel is open. A D3D9 overlay is still to do.

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
