# Builds and runs the panel render check (MSVC x64): ImGui (vendored, unchanged) at /W0, the panel at /W4.
# Screenshots land in build\ for visual review. System32 comes first on PATH before vcvars, as the game builds do.
param([switch]$InputCustodyOnly)
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$here = $PSScriptRoot
$build = Join-Path $here 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$im = '..\third_party\imgui'
# CPU-only production message-hook fixture: no real window, hook, cursor or GPU.
foreach ($arch in 'x64', 'x86') {
    $vv = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $build "input-$arch"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $cpuSrc = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp"
    $cpuObj = 'imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets' | ForEach-Object { "`"$dir\$_.obj`"" }
    $outInput = cmd /c "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vv`" >nul && cd /d `"$here`" && cl /nologo /c /O2 /MT /EHsc /std:c++17 /W0 /I$im $cpuSrc /Fo`"$dir\\`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_input_custody.cpp $($cpuObj -join ' ') /Fo`"$dir\\`" /Fe`"$dir\test_input_custody.exe`" user32.lib && `"$dir\test_input_custody.exe`"" 2>&1
    $outInput | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $outInput; throw "input custody check failed ($arch)" }
    # UI-1 settings descriptor reader (CPU only): the GRID and art samples parse, non-standard descriptors are refused.
    $outDesc = cmd /c "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vv`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX test_panel_descriptor.cpp dbce_panel_descriptor.cpp /Fo`"$dir\\`" /Fe`"$dir\test_panel_descriptor.exe`" && `"$dir\test_panel_descriptor.exe`" samples" 2>&1
    $outDesc | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $outDesc; throw "descriptor check failed ($arch)" }
    # dbce-mod.json (XC-1): the whole-file reader's dump of each fixture matches the goldens the PowerShell and C# readers use.
    $outMod = cmd /c "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vv`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX test_mod_file.cpp dbce_panel_descriptor.cpp /Fo`"$dir\\`" /Fe`"$dir\test_mod_file.exe`" && `"$dir\test_mod_file.exe`" ..\..\schema\fixtures" 2>&1
    $outMod | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $outMod; throw "dbce-mod.json check failed ($arch)" }
}
if ($InputCustodyOnly) { return }
$imgui = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx11.cpp $im\backends\imgui_impl_dx12.cpp $im\backends\imgui_impl_dx9.cpp"
$cmd = "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /c /O2 /MT /EHsc /std:c++17 /W0 /I$im $imgui /Fo`"$build\\`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_panel_render.cpp dbce_panel.cpp dbce_panel_descriptor.cpp dbce_panel_input.cpp dbce_overlay_d3d11.cpp dbce_overlay_d3d12.cpp /Fo`"$build\\`" `"$build\imgui.obj`" `"$build\imgui_draw.obj`" `"$build\imgui_tables.obj`" `"$build\imgui_widgets.obj`" `"$build\imgui_impl_dx11.obj`" `"$build\imgui_impl_dx12.obj`" /Fe`"$build\test_panel_render.exe`" && `"$build\test_panel_render.exe`" `"$build`""
# STD-023 frame-rate statistics (pure).
$outFs = cmd /c "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX test_frame_stats.cpp /Fo`"$build\\`" /Fe`"$build\test_frame_stats.exe`" && `"$build\test_frame_stats.exe`"" 2>&1
$outFs | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $outFs | Select-Object -Last 25; throw 'frame-rate check failed' }
$out = cmd /c $cmd 2>&1
$out | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw 'panel render check failed' }

# D3D12 overlay path on WARP through the real present hook and queue capture.
$objs = 'dbce_panel', 'dbce_panel_input', 'dbce_overlay_d3d11', 'dbce_overlay_d3d12', 'imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets', 'imgui_impl_dx11', 'imgui_impl_dx12' | ForEach-Object { "`"$build\$_.obj`"" }
$cmd12 = "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_overlay_d3d12.cpp /Fo`"$build\\`" $($objs -join ' ') /Fe`"$build\test_overlay_d3d12.exe`" /link user32.lib && `"$build\test_overlay_d3d12.exe`" `"$build`""
$out12 = cmd /c $cmd12 2>&1
$out12 | Select-String -Pattern ' warning | error |FAIL|PASS|log:|pixel' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out12 | Select-Object -Last 25; throw 'D3D12 overlay check failed' }

# Another overlay (Steam's) hooking Present again over ours must not make the two hooks call each other forever.
$cmdRe = "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_overlay_rehook.cpp /Fo`"$build\\`" $($objs -join ' ') /Fe`"$build\test_overlay_rehook.exe`" /link user32.lib && `"$build\test_overlay_rehook.exe`" && `"$build\test_overlay_rehook.exe`" entry"
$outRe = cmd /c $cmdRe 2>&1
$outRe | Select-String -Pattern ' warning | error |FAIL|PASS|log:' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $outRe | Select-Object -Last 25; throw 'present hook re-hook check failed' }

# Swap chains the game recreates on its window are followed; one the overlay cannot draw on leaves the game's input
# alone (WARP, hidden window).
$cmdRc = "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_overlay_recreate.cpp /Fo`"$build\\`" $($objs -join ' ') /Fe`"$build\test_overlay_recreate.exe`" /link user32.lib && `"$build\test_overlay_recreate.exe`""
$outRc = cmd /c $cmdRc 2>&1
$outRc | Select-String -Pattern ' warning | error |FAIL|PASS|log:|pixel|flip chain' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $outRc | Select-Object -Last 25; throw 'D3D11 recreate check failed' }

# D3D9 overlay with this machine's D3D9 device on a hidden window (no WARP for D3D9): Present/Reset lifecycle, on x64
# and on x86, which most D3D9 games are.
$cmd9 = "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_overlay_d3d9.cpp dbce_overlay_d3d9.cpp /Fo`"$build\\`" $($objs -join ' ') `"$build\imgui_impl_dx9.obj`" /Fe`"$build\test_overlay_d3d9.exe`" && `"$build\test_overlay_d3d9.exe`" `"$build`""
$b32 = Join-Path $build 'x86'
New-Item -ItemType Directory -Force $b32 | Out-Null
$imgui32 = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx11.cpp $im\backends\imgui_impl_dx9.cpp"
$objs32 = 'imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets', 'imgui_impl_dx11', 'imgui_impl_dx9' | ForEach-Object { "`"$b32\$_.obj`"" }
$cmd9x86 = "set `"PATH=$env:WINDIR\System32;$env:WINDIR;%PATH%`" && call `"$(Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat')`" >nul && cd /d `"$here`" && cl /nologo /c /O2 /MT /EHsc /std:c++17 /W0 /I$im $imgui32 /Fo`"$b32\\`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_overlay_d3d9.cpp dbce_overlay_d3d9.cpp dbce_overlay_d3d11.cpp dbce_panel.cpp dbce_panel_descriptor.cpp dbce_panel_input.cpp /Fo`"$b32\\`" $($objs32 -join ' ') /Fe`"$b32\test_overlay_d3d9.exe`" && `"$b32\test_overlay_d3d9.exe`" `"$b32`""
foreach ($c9 in $cmd9, $cmd9x86) {
    $out9 = cmd /c $c9 2>&1
    $out9 | Select-String -Pattern ' warning | error |FAIL|PASS|log:|pixel|Reset:|in-flight' | ForEach-Object { $_.Line }
    if ($LASTEXITCODE) { $out9 | Select-Object -Last 25; throw 'D3D9 overlay check failed' }
}
