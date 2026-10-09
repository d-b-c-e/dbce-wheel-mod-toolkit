# Builds and runs the panel render check (MSVC x64): ImGui (vendored, unchanged) at /W0, the panel at /W4.
# Screenshots land in build\ for visual review.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$here = $PSScriptRoot
$build = Join-Path $here 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$im = '..\third_party\imgui'
$imgui = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx11.cpp"
$cmd = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /c /O2 /MT /EHsc /std:c++17 /W0 /I$im $imgui /Fo`"$build\\`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /I$im test_panel_render.cpp dbce_panel.cpp dbce_panel_input.cpp dbce_overlay_d3d11.cpp /Fo`"$build\\`" `"$build\imgui.obj`" `"$build\imgui_draw.obj`" `"$build\imgui_tables.obj`" `"$build\imgui_widgets.obj`" `"$build\imgui_impl_dx11.obj`" /Fe`"$build\test_panel_render.exe`" && `"$build\test_panel_render.exe`" `"$build`""
$out = cmd /c $cmd 2>&1
$out | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw 'panel render check failed' }
