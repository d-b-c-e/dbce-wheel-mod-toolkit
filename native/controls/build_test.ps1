# Builds and runs the controls contract checks (STD-033): the shared vectors and the [Controls] section reader. MSVC x64
# and x86, warnings as errors. No game, no device.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$here = $PSScriptRoot
$build = Join-Path $here 'build'
foreach ($arch in 'x64', 'x86') {
    $vcvars = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $build $arch
    New-Item -ItemType Directory -Force $dir | Out-Null
    $exe = Join-Path $dir 'test_controls.exe'
    $cmd = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 test_controls.cpp /Fo`"$dir\\`" /Fe`"$exe`" && `"$exe`" controls-vectors.txt"
    $out = cmd /c $cmd 2>&1
    $out | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw "controls checks failed ($arch)" }
    # Pedal rest shaping (dbce_shape.hpp).
    $shape = Join-Path $dir 'test_shape.exe'
    $out = cmd /c "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 test_shape.cpp /Fo`"$dir\\`" /Fe`"$shape`" && `"$shape`"" 2>&1
    $out | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw "shape checks failed ($arch)" }
}
