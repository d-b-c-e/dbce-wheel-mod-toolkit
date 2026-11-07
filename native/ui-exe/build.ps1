# Builds dbce-ui.exe (x64) into build\ and runs the --setup byte-diff check (test_setup.cpp): ImGui (vendored,
# unchanged) at /W0, the panel core, the exe and the check at /W4 /WX. Fails when the exe is 5 MB or more.
# -NoTest builds only.
param([switch]$NoTest)
$ErrorActionPreference = 'Stop'
# The owner's user PATH is longer than cmd accepts; vcvars needs the machine PATH (session only).
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ";${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$here = $PSScriptRoot
$im = '..\third_party\imgui'
$p = '..\panel'
$imgui = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx11.cpp"
$panel = "$p\dbce_panel.cpp $p\dbce_panel_descriptor.cpp $p\dbce_panel_input.cpp $p\dbce_overlay_d3d11.cpp"
$dir = Join-Path $here 'build'
$obj = Join-Path $dir 'obj'
New-Item -ItemType Directory -Force $obj | Out-Null
$exe = Join-Path $dir 'dbce-ui.exe'
$cmd = "`"$vcvars`" >nul && cd /d `"$here`" && " +
    "cl /nologo /c /O1 /MT /EHsc /std:c++17 /W0 /DNDEBUG /I$im $imgui /Fo`"$obj\\`" && " +
    "cl /nologo /c /O1 /MT /EHsc /std:c++17 /W4 /WX /DNDEBUG /DUNICODE /D_UNICODE /I$im $panel dbce_ui_exe.cpp /Fo`"$obj\\`" && " +
    "link /nologo /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /OUT:`"$exe`" `"$obj\*.obj`" user32.lib gdi32.lib shell32.lib d3d11.lib d3dcompiler.lib"
$out = cmd /c $cmd 2>&1
if ($LASTEXITCODE) { $out | Select-Object -Last 30; throw 'dbce-ui.exe build failed' }
$out | Select-String -Pattern ' warning ' | ForEach-Object { $_.Line }
$size = (Get-Item $exe).Length
"dbce-ui.exe $([math]::Round($size / 1MB, 2)) MB"
if ($size -ge 5MB) { throw "dbce-ui.exe is $size bytes; the limit is 5 MB" }
if ($NoTest) { return }
$sample = (Resolve-Path (Join-Path $here '..\panel\samples\art-of-rally.settings.json')).Path
$work = Join-Path $dir 'test'
$test = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /DUNICODE /D_UNICODE test_setup.cpp /Fo`"$dir\\`" /Fe`"$dir\test_setup.exe`" user32.lib && " +
    "`"$dir\test_setup.exe`" `"$exe`" `"$sample`" `"$here\fixtures`" `"$work`""
$outT = cmd /c $test 2>&1
$outT | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { $outT | Select-Object -Last 40; throw 'dbce-ui --setup check failed' }
