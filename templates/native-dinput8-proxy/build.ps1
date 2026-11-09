# Builds the x64 dinput8.dll proxy into build\, checks its six exports and runs the offline tests.
# No arguments needed: `pwsh build.ps1`. Run it before pushing.
param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
# src/vendor must equal the toolkit commits in src/vendor/VERSION (never edit it; change the toolkit and re-sync).
$sync = Join-Path $PSScriptRoot '..\_toolkits\dbce-wheel-mod-toolkit\tools\Sync-Native.ps1'
if (Test-Path -LiteralPath $sync) {
    & pwsh -NoProfile -File $sync -GameRepo $PSScriptRoot -Check | Select-Object -Last 1
    if ($LASTEXITCODE) { throw 'src/vendor differs from src/vendor/VERSION' }
} else { 'toolkit checkout not found: vendor check skipped' }
# cmd truncates a PATH over 8191 characters, and vcvars then loses the Windows SDK. Keep only what the build needs.
if ($env:PATH.Length -gt 8000) {
    $keep = 'git', 'pwsh' | ForEach-Object { (Get-Command $_ -ErrorAction SilentlyContinue | Select-Object -First 1).Source } | Where-Object { $_ } | ForEach-Object { Split-Path $_ }
    $env:PATH = (@("$env:WINDIR\System32", $env:WINDIR, "$env:WINDIR\System32\WindowsPowerShell\v1.0", "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer") + $keep | Select-Object -Unique) -join ';'
}
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$build = Join-Path $PSScriptRoot 'build'
$obj = Join-Path $build 'obj'
New-Item -ItemType Directory -Force $obj, "$build\host" | Out-Null
function Run([string]$What, [string]$Command) {
    $out = cmd /d /c "`"$vcvars`" >nul && cd /d `"$PSScriptRoot`" && $Command" 2>&1
    $out | Select-String -Pattern ' warning | error |^ok  |^FAIL|passed$' | ForEach-Object { $_.Line }
    if ($LASTEXITCODE) { $out | Select-Object -Last 20; throw "$What failed" }
    $out
}
# Dear ImGui is vendored unchanged and compiled without warnings; the mod and the toolkit sources at /W4.
$im = 'src\vendor\third_party\imgui'
$imgui = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx11.cpp"
$imObjs = 'imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets', 'imgui_impl_dx11' | ForEach-Object { "`"$obj\$_.obj`"" }
$toolkit = 'src\vendor\panel\dbce_panel.cpp src\vendor\panel\dbce_panel_descriptor.cpp src\vendor\panel\dbce_panel_input.cpp src\vendor\panel\dbce_overlay_d3d11.cpp src\vendor\dinput-proxy\dbce_ffb_layer.cpp'
[void](Run 'Dear ImGui build' "cl /nologo /c /O2 /MT /EHsc /std:c++17 /W0 /I$im $imgui /Fo`"$obj\\`"")
$out = Run 'proxy build' "cl /nologo /LD /O2 /MT /W4 /EHsc /std:c++17 /I$im src\proxy.cpp $toolkit $($imObjs -join ' ') /Fo`"$obj\\`" /Fe`"$build\dinput8.dll`" /link /DEF:src\dinput8.def user32.lib && dumpbin /nologo /exports `"$build\dinput8.dll`""
$exports = @($out | Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
if ($exports.Count -ne 6) { throw "expected 6 exports, found: $($exports -join ', ')" }
"built $build\dinput8.dll: 6 exports"
if ($BuildOnly) { return }

# The start check, under the game's exe name beside a copy of the DLL.
[void](Run 'host build' "cl /nologo /O2 /MT /W4 /EHsc tests\host.cpp /Fo`"$obj\\`" /Fe`"$build\host\__EXE__`" `"$build\dinput8.lib`"")
Copy-Item -LiteralPath "$build\dinput8.dll" -Destination "$build\host\dinput8.dll" -Force
& "$build\host\__EXE__"
if ($LASTEXITCODE) { throw 'start check failed' }
'build and offline tests passed'
