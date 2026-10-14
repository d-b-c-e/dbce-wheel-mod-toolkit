# Builds and runs the C++ racing-row checks (shared/display/dbce_racing_row.hpp) against the shared vectors. MSVC x64
# and x86 as C++17, and x64 as C++20, warnings as errors. Reads the live display list only; no game, no device.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$here = $PSScriptRoot
$vectors = (Resolve-Path (Join-Path $here '..\..\..\shared\display\racing-row-vectors.txt')).Path
foreach ($build in @(@{ arch = 'x64'; std = 'c++17' }, @{ arch = 'x86'; std = 'c++17' }, @{ arch = 'x64'; std = 'c++20' })) {
    $vcvars = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($build.arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $here ('build\{0}-{1}' -f $build.arch, $build.std)
    New-Item -ItemType Directory -Force $dir | Out-Null
    $exe = Join-Path $dir 'test_racing_row.exe'
    $cmd = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:$($build.std) test_racing_row.cpp user32.lib /Fo`"$dir\\`" /Fe`"$exe`" && `"$exe`" `"$vectors`""
    $out = cmd /c $cmd 2>&1
    $label = '{0} {1}' -f $build.arch, $build.std
    $out | Select-String -Pattern ' warning | error |FAIL|PASS|live displays' | ForEach-Object { "${label}: $($_.Line)" }
    if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw "racing-row native checks failed ($label)" }
}
