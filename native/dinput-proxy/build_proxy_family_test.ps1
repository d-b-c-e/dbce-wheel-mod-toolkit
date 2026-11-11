# Builds and runs the attach-time proxy checks (test_proxy_family.cpp: dbce_proxy_core.hpp's attach, dbce_proxy_span.hpp,
# dbce_proxy_dxgi.hpp, dbce_proxy_muted.hpp), MSVC x64 at /W4 /WX. No game, no display change, no device beyond the
# system keyboard. The test's log folder is removed afterwards.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$here = $PSScriptRoot
$dir = Join-Path $here 'build\proxy-family'
New-Item -ItemType Directory -Force $dir | Out-Null
$logDir = Join-Path $env:LOCALAPPDATA 'dbce\proxyfamilytest'
try {
    $o = cmd /c "`"$(Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat')`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 test_proxy_family.cpp /Fo`"$dir\\`" /Fe`"$dir\test_proxy_family.exe`" && `"$dir\test_proxy_family.exe`"" 2>&1
    $o | Select-String -Pattern ' warning | error |FAIL|PASS' | ForEach-Object { $_.Line }
    if ($LASTEXITCODE) { $o | Select-Object -Last 25; throw 'proxy family checks failed' }
} finally {
    if ([IO.Directory]::Exists($logDir)) { Remove-Item -LiteralPath $logDir -Recurse -Force }
}
