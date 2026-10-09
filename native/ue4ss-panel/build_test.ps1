# Builds the UE4SS panel DLL (build\dbce-ue4ss-panel.dll, MSVC x64) and runs its offline checks: the renderer-config
# fixtures, the DLL from a UE4SS-shaped folder with a panel.ini drawing on a WARP D3D12 swap chain (build\host-panel.png),
# and the Lua loader in UE4SS's own Lua 5.4 when the RE-UE4SS source is at hand. Dear ImGui at /W0, ours at /W4 /WX.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$here = $PSScriptRoot
$build = Join-Path $here 'build'
$obj = Join-Path $build 'obj'
New-Item -ItemType Directory -Force $build, $obj | Out-Null
$im = '..\third_party\imgui'
$imgui = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx11.cpp $im\backends\imgui_impl_dx12.cpp"
$imObjs = 'imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets', 'imgui_impl_dx11', 'imgui_impl_dx12' | ForEach-Object { "`"$obj\$_.obj`"" }
$ours = 'dbce_ue4ss_panel.cpp ..\panel\dbce_panel.cpp ..\panel\dbce_panel_input.cpp ..\panel\dbce_overlay_d3d11.cpp ..\panel\dbce_overlay_d3d12.cpp'
$dll = Join-Path $build 'dbce-ue4ss-panel.dll'
$cmd = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /c /O2 /MT /EHsc /std:c++17 /W0 /I$im $imgui /Fo`"$obj\\`" && cl /nologo /LD /O2 /MT /W4 /WX /EHsc /std:c++17 /I$im $ours $($imObjs -join ' ') /Fo`"$obj\\`" /Fe`"$dll`" /link user32.lib && dumpbin /nologo /exports `"$dll`""
$out = cmd /c $cmd 2>&1
$out | Select-String -Pattern ' warning | error ' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out | Select-Object -Last 20; throw 'build failed' }
$exports = @($out | Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
if (($exports -join ',') -ne 'luaopen_dbce_ue4ss_panel') { throw "unexpected exports: $($exports -join ', ')" }
"built ${dll}: luaopen_dbce_ue4ss_panel, SHA-256 $((Get-FileHash $dll).Hash)"

$tests = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 test_renderer_config.cpp /Fo`"$build\\`" /Fe`"$build\test_renderer_config.exe`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 test_ue4ss_panel_host.cpp /Fo`"$build\\`" /Fe`"$build\test_ue4ss_panel_host.exe`""
$out = cmd /c $tests 2>&1
$out | Select-String -Pattern ' warning | error ' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out | Select-Object -Last 20; throw 'test build failed' }
& "$build\test_renderer_config.exe"; if ($LASTEXITCODE) { throw 'renderer config fixtures failed' }

# A UE4SS-shaped folder: Mods\DbcePanel\panel\<dll> + panel.ini, Mods\DbceTriple\config.ini (synthetic rig values).
$mods = Join-Path $build 'host\ue4ss\Mods'
New-Item -ItemType Directory -Force "$mods\DbcePanel\panel", "$mods\DbcePanel\scripts", "$mods\DbceTriple" | Out-Null
Copy-Item -LiteralPath $dll -Destination "$mods\DbcePanel\panel\dbce-ue4ss-panel.dll" -Force
Set-Content -LiteralPath "$mods\DbcePanel\panel\panel.ini" -Value "[Mod]`r`nGame = Test game`r`nVersion = 9.9.9`r`nLogName = ue4sstest`r`nRendererConfig = ..\..\DbceTriple\config.ini" -Encoding ascii
Set-Content -LiteralPath "$mods\DbceTriple\config.ini" -Value "# synthetic`r`nTriple = Auto`r`nPanelWidthMm = 600`r`nPanelHeightMm = 340`r`nEyeDistanceMm = 0`r`nVerticalFovDegrees = 60`r`nSideAngle = 65`r`nBezelMm = 10" -Encoding ascii
& "$build\test_ue4ss_panel_host.exe" "$mods\DbcePanel\panel\dbce-ue4ss-panel.dll" "$mods\DbceTriple\config.ini" "$build\host-panel.png"
if ($LASTEXITCODE) { throw 'panel host check failed' }

$luaSrc = 'E:\Source\games\dbce-mods-sonic-crossworlds\third_party\RE-UE4SS\deps\first\LuaRaw'
if (Test-Path "$luaSrc\src\lua.c") {
    $luaOut = Join-Path $build 'lua'
    New-Item -ItemType Directory -Force $luaOut | Out-Null
    if (!(Test-Path "$luaOut\lua.exe")) {
        $files = (Get-ChildItem "$luaSrc\src" -Filter *.c | Where-Object Name -ne 'luac.c' | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '
        $null = cmd /c "`"$vcvars`" >nul && cl /nologo /O2 /MT /W0 /I`"$luaSrc\include`" $files /Fo`"$luaOut\\`" /Fe`"$luaOut\lua.exe`"" 2>&1
        if ($LASTEXITCODE) { throw 'Lua interpreter build failed' }
    }
    Copy-Item -LiteralPath (Join-Path $here 'lua\scripts\main.lua') -Destination "$mods\DbcePanel\scripts\main.lua" -Force
    $logPath = Join-Path $env:LOCALAPPDATA 'dbce\ue4sstest\dbce-ue4sstest-panel.log'
    $from = if (Test-Path $logPath) { (Get-Item $logPath).Length } else { 0 }
    $said = & "$luaOut\lua.exe" -e "dofile([[$mods\DbcePanel\scripts\main.lua]]); local t = os.clock(); while os.clock() - t < 2 do end" 2>&1
    $stream = [IO.File]::Open($logPath, 'Open', 'Read', 'ReadWrite'); $null = $stream.Seek($from, 'Begin')
    $new = [IO.StreamReader]::new($stream).ReadToEnd(); $stream.Dispose()
    if (($said -join ' ') -notmatch 'panel started \(F6\)' -or $new -notmatch 'present hook installed' -or $new -notmatch 'Test game panel 9\.9\.9') { $said; $new; throw 'Lua loader check failed' }
    'PASS Lua loader check (package.loadlib from the mod folder, panel.ini read, present hook installed)'
} else { 'Lua loader check skipped (no RE-UE4SS Lua source)' }
