# dbce_ui.dll: the F6 panel core in-process, for non-native engines (UI-2)

Host A of `docs/architecture/2026-11/00-overview.md` section 3 (in the hub, `dbce-project-mgmt`). It is the shared
native panel (`native/panel`, Dear ImGui) in a DLL of its own, for mods that are not native DLLs: Unity Mono 2.0,
Mono 4.x and IL2CPP first. The mod loads it; it draws the descriptor pages (UI-1) inside the game's own frames,
exactly as the native and UE4SS games do.

| File | What it is |
|---|---|
| `dbce_ui.h` | The C API: configure the descriptor, push values, poll changes and actions, navigation commands, toasts, D3D9 attach, status and log lines. No callbacks, so the core never calls managed code (and never on the render thread). |
| `dbce_ui.cpp`, `dbce_ui.def` | The DLL. Loading it starts a thread that installs the DXGI present hook (D3D11, plus D3D12 when `d3d12.dll` is already loaded) and the input hook on the swap chain's window. Logs to `dbce_ui.log` beside the DLL and to `dbce_ui_poll_log`. |
| `test_ui_host.cpp`, `build.ps1` | Builds `build\x64\dbce_ui.dll` and `build\x86\dbce_ui.dll` (/O1, static CRT, under 2 MB each; exports checked against the .def), then on each architecture a host that only calls `LoadLibrary`, presents on a WARP D3D11 swap chain, opens F6 by `DBCE_UI_NAV_OPEN`, goes to Force feedback with Right and Down, changes Strength and the switch, and reads both back with `dbce_ui_poll_event`. An arrow key posted to the window moves the same focus and never reaches the game; F8 arrives as the `panel.stop-ffb` action. Pixels show the panel open and closed and a toast; `build\<arch>\focus-ffb-strength.bmp` shows the focus box. Then a D3D9 device is handed over through one of its textures and taken by the render event: the panel is drawn on it, the arrows follow its window, and the detach event lets go of it (skipped, not failed, when the machine offers no D3D9 device). |

`test_wrapper.ps1` (run last by `build.ps1`) calls every P/Invoke of the C# wrapper once against the x64 DLL, in a
PowerShell 7 process of its own, so the wrapper and the DLL cannot drift apart.

`shared/ui/DbceUi.cs` is the C# wrapper (one source for net35, Mono 4.x and net6.0; UTF-8 byte arrays, no delegates).
`dotnet/Dbce.Wheel.UiHost` compiles it for net35 and net6.0 as part of `Dbce.Wheel.sln`.

## Using it from a Unity mod

1. Ship `x64\dbce_ui.dll` (and `x86\dbce_ui.dll` for 32-bit games) with the plugin. `DbceUi.Load(fullPath, out error)`
   loads the one that matches the process; that starts the hooks.
2. `DbceUi.Configure(settingsJson, iniPath, ffbStop: true, out error)` with the `settings` block of `dbce-mod.json`,
   then push the saved values (`SetToggle`, `SetChoice`, `SetNumber`, `SetText`, `SetRowVisible`).
3. In `Update`: `while (DbceUi.TryPollEvent(out e))` apply and save each change (STD-044), then `NoteSaved`. Capture a
   binding on `Capture` and show its input with `SetText`. `panel.stop-ffb` is F8. While `DbceUi.IsOpen`, keep the
   game's own input blocked as the mod already does.
4. Wheel navigation: forward the wheel's D-pad and two buttons as `Navigate(Up/Down/Left/Right/Select/Back)`, and the
   Open settings button as `Open`. The keyboard's arrows, Enter/Space and Esc take the same path inside the core.
5. A D3D9 game: `DbceUi.AttachD3D9(texture.GetNativeTexturePtr())`, then
   `GL.IssuePluginEvent(DbceUi.RenderEventFunc, DbceUi.RenderAttachD3D9)` so the device is taken on the render thread;
   `RenderDetachD3D9` before the game releases that device.

## Limits (2026-11-07)

Checked offline on WARP only (x86 and x64); no game loads it yet (UI-4 moves art of rally onto it). D3D9 is checked on
this machine's own D3D9 device (there is no WARP for D3D9). F7 (camera comparison) and camera keys are not routed through the API. The DLL is not yet in the release zip or a
`Sync-Toolkit` part; the first Unity consumer adds that.
