# Builds and runs the force-layer checks (MSVC x64): the effect-gain fixtures, and a compile of dbce_ffb_layer.cpp with
# the shared panel (no game, no device).
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$here = $PSScriptRoot
$build = Join-Path $here 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$im = '..\third_party\imgui'
$cmd = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc test_ffb_gain.cpp /Fo`"$build\\`" /Fe`"$build\test_ffb_gain.exe`" && `"$build\test_ffb_gain.exe`" && cl /nologo /c /O2 /MT /W4 /WX /EHsc /std:c++17 /I$im dbce_ffb_layer.cpp /Fo`"$build\\`""
$out = cmd /c $cmd 2>&1
$out | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw 'force-layer checks failed' }
'compiled dbce_ffb_layer.cpp'

# 32-bit consumers (DiRT 3): the layer and the panel/overlay compile for x86 too, where __stdcall argument counts matter.
$vcvars32 = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$b32 = Join-Path $build 'x86'
New-Item -ItemType Directory -Force $b32 | Out-Null
$cmd32 = "`"$vcvars32`" >nul && cd /d `"$here`" && cl /nologo /c /O2 /MT /W4 /WX /EHsc /std:c++17 /I$im dbce_ffb_layer.cpp ..\panel\dbce_panel.cpp ..\panel\dbce_panel_input.cpp ..\panel\dbce_overlay_d3d11.cpp ..\panel\dbce_overlay_d3d12.cpp /Fo`"$b32\\`""
$out32 = cmd /c $cmd32 2>&1
$out32 | Select-String -Pattern ' warning | error ' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $out32 | Select-Object -Last 25; throw 'x86 compile failed' }
'compiled the layer and the panel for x86'