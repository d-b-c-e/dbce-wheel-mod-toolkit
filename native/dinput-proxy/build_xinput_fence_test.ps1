# Builds and runs the XInput output fence checks (dbce_xinput_fence.hpp against a fake XInputSetState), MSVC x64 and
# x86. No pad, no XInput library, no game.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$here = $PSScriptRoot
foreach ($arch in 'x64', 'x86') {
    $vv = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $here "build\xinput-fence-$arch"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $o = cmd /c "`"$vv`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc test_xinput_fence.cpp /Fo`"$dir\\`" /Fe`"$dir\test_xinput_fence.exe`" && `"$dir\test_xinput_fence.exe`"" 2>&1
    $o | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $o | Select-Object -Last 25; throw "xinput fence checks failed ($arch)" }
}
