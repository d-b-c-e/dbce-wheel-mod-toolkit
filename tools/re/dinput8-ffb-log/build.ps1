# Builds the muted DirectInput logger as build\x64\dinput8.dll and build\x86\dinput8.dll (MSVC) and checks exports.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
foreach ($arch in 'x64', 'x86') {
    $vcvars = Join-Path $vs ("VC\Auxiliary\Build\vcvars{0}.bat" -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $build = Join-Path $PSScriptRoot "build\$arch"
    New-Item -ItemType Directory -Force $build | Out-Null
    $cmd = "`"$vcvars`" >nul && cd /d `"$PSScriptRoot`" && cl /nologo /LD /O2 /MT /W4 /EHsc dinput8log.cpp /Fo`"$build\\`" /Fe`"$build\dinput8.dll`" /link /DEF:dinput8.def && dumpbin /nologo /exports `"$build\dinput8.dll`""
    $out = cmd /c $cmd 2>&1
    $out | Select-String -Pattern ' warning | error ' | ForEach-Object { $_.Line }
    if ($LASTEXITCODE) { $out | Select-Object -Last 20; throw "$arch build failed" }
    $exports = @($out | Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
    if ($exports.Count -ne 6) { throw "$arch exports: $($exports -join ', ')" }
    "built $build\dinput8.dll: 6 exports, SHA-256 $((Get-FileHash "$build\dinput8.dll").Hash)"
}
