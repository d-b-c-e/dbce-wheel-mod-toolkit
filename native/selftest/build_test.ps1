# Builds and runs the self-test interpreter check (MSVC, x64 and x86, /W4 /WX): a fake host runs every
# schema\fixtures\selftest\*.script.json and must write the goldens beside them. -Update rewrites the goldens (review
# the diff before committing). cmd gets a short PATH of its own, so a long user PATH cannot break vcvars.
param([switch]$Update)
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$here = $PSScriptRoot
$fixtures = Join-Path $here '..\..\schema\fixtures\selftest'
$path = "$env:WINDIR\System32;$env:WINDIR;$env:WINDIR\System32\Wbem;$env:WINDIR\System32\WindowsPowerShell\v1.0;$(Split-Path $vswhere)"
foreach ($arch in 'x64', 'x86') {
    $vcvars = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $here "build\$arch"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $mode = if ($Update -and $arch -eq 'x64') { ' update' } else { '' }
    $out = cmd /c "set `"PATH=$path`" && call `"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX test_selftest.cpp /Fo`"$dir\\`" /Fe`"$dir\test_selftest.exe`" && `"$dir\test_selftest.exe`" `"$fixtures`" `"$dir\work`"$mode" 2>&1
    $out | Select-String -Pattern ' warning | error |FAIL|PASS|: pass|: fail' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $out | Select-Object -Last 40; throw "self-test interpreter check failed ($arch)" }
}
