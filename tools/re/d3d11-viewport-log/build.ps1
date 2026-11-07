# Builds the x86 d3d11 logging proxy with MSVC (vcvars32) into build\d3d11.dll and checks its exports.
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$build = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$cmd = "`"$vcvars`" >nul && cd /d `"$PSScriptRoot`" && cl /nologo /LD /O2 /MT /W3 /EHsc d3d11log.cpp /Fo`"$build\\`" /Fe`"$build\d3d11.dll`" /link /DEF:d3d11.def user32.lib && dumpbin /nologo /exports `"$build\d3d11.dll`""
$out = cmd /c $cmd 2>&1
$out | Select-String -NotMatch '^\s*$' | Select-Object -Last 70
if ($LASTEXITCODE) { throw 'build failed' }
$exports = @($out | Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$missing = @('D3D11CreateDevice', 'D3D11CreateDeviceAndSwapChain' | Where-Object { $exports -notcontains $_ })
if ($exports.Count -ne 51 -or $missing.Count) { throw "unexpected exports ($($exports.Count))" }
"built $build\d3d11.dll: 51 exports, SHA-256 $((Get-FileHash "$build\d3d11.dll").Hash)"
