# Builds and runs the camera set checks (test.cpp): MSVC x64 and x86, warnings as errors. Files are written
# only under build\<arch>. No game, no device.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$here = $PSScriptRoot
foreach ($arch in 'x64', 'x86') {
    $vcvars = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $here "build\$arch"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $exe = Join-Path $dir 'test_camera_sets.exe'
    # vcvars needs System32 and the VS Installer (vswhere) on PATH, or it leaves INCLUDE without the Windows SDK.
    $out = cmd /c "set `"PATH=$env:WINDIR\System32;$env:WINDIR;${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;%PATH%`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS test.cpp /Fo`"$dir\\`" /Fe`"$exe`" && `"$exe`" `"$dir`"" 2>&1
    $out | Select-String -Pattern ' warning | error |FAIL|PASS|Assertion' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $out | Select-Object -Last 25; throw "camera set checks failed ($arch)" }
}
