# UE4SS panel (STD-030)

The shared F6 panel (`../panel`) for Unreal games that run the toolkit's triple renderer under UE4SS (ADRIFT, Sonic
CrossWorlds), as a small DLL a UE4SS **Lua** mod loads with `package.loadlib`: nothing depends on the UE4SS C++ ABI, so
one build works with any UE4SS release. D3D11 and D3D12 (the overlay draws on the game's own present queue).

| File | Role |
| --- | --- |
| `dbce_ue4ss_panel.cpp` | The DLL: export `luaopen_dbce_ue4ss_panel` (starts once, pins itself, returns no Lua values). Pages: Cameras (STD-022 Off / Surround / Separate monitors over the renderer's `Triple` = Off or Auto plus a `Screens` key it ignores; side-screen angle 0-90°; rig values in Advanced; STD-023 frame rate) and Help. Changes are written to the renderer's `config.ini` at once and apply at the next game start. |
| `dbce_renderer_config.hpp` | Pure reader/writer of the renderer's sectionless `config.ini` (keys before the panel's own `[Panel]` section), tested by `test_renderer_config.cpp`. |
| `lua/scripts/main.lua` | The DbcePanel Lua mod: finds its own folder and loads `panel\dbce-ue4ss-panel.dll`. |
| `test_ue4ss_panel_host.cpp` | Loads the DLL from a UE4SS-shaped folder and draws through a WARP D3D12 swap chain: panel on the centre third, `config.ini` unchanged by looking. |
| `build_test.ps1` | Builds the DLL and runs the checks, including the loader in UE4SS's own Lua 5.4 when the RE-UE4SS source is present. |

Per game, `panel.ini` beside the DLL:

```ini
[Mod]
Game = ADRIFT
Version = 0.2.0
LogName = adrift                              ; %LOCALAPPDATA%\dbce\<LogName>\dbce-<LogName>-panel.log, panel-dev.txt
RendererConfig = ..\..\DbceTriple\config.ini  ; relative to the DLL's folder (Sonic: ..\..\SonicTriple\config.ini)
```

Install layout: `ue4ss\Mods\DbcePanel\scripts\main.lua`, `ue4ss\Mods\DbcePanel\panel\dbce-ue4ss-panel.dll` and
`panel.ini`, plus `DbcePanel : 1` in `mods.txt`. The game's installer owns those files (receipt) and should treat the
renderer's `config.ini` as the player's file, as ADRIFT's `tools\Install-Triple.ps1` does.

ADRIFT 0.2.0 ships the first copy of this code as its own `dbce-adrift-panel.dll` (export `luaopen_dbce_adrift_panel`);
it moves to this shared DLL after its launch check.
