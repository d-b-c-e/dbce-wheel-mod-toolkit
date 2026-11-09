# Builds and runs every offline test in this toolkit. No arguments needed: `pwsh build.ps1`. About ten minutes.
# -Only <text> runs the steps whose name contains that text, such as `pwsh build.ps1 -Only panel`.
# Needs no wheel, game or GPU. The hardware smoke test (native\wheelffb\test\build_smoke.bat) is separate.
param([string]$Only = '')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
# cmd truncates a PATH over 8191 characters, and vcvars then loses the Windows SDK. Keep only what the builds need.
if ($env:PATH.Length -gt 8000) {
    $keep = 'git', 'cmake', 'ninja', 'python', 'pwsh', 'dotnet', 'node' | ForEach-Object { (Get-Command $_ -ErrorAction SilentlyContinue | Select-Object -First 1).Source } | Where-Object { $_ } | ForEach-Object { Split-Path $_ }
    $env:PATH = (@("$env:WINDIR\System32", $env:WINDIR, "$env:WINDIR\System32\WindowsPowerShell\v1.0", "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer") + $keep | Select-Object -Unique) -join ';'
}
$failed = [Collections.Generic.List[string]]::new()
function Step([string]$Name, [scriptblock]$Run) {
    if ($Only -and $Name -notlike "*$Only*") { return }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $global:LASTEXITCODE = 0
    $threw = $false
    try { $out = & $Run 2>&1 } catch { $out = @($_); $threw = $true }
    if ($threw -or $LASTEXITCODE) { $failed.Add($Name); "FAIL $Name"; $out | Select-Object -Last 25 } else { "ok   $Name ($([int]$watch.Elapsed.TotalSeconds) s)" }
}
function Script([string]$Path) { Step $Path { pwsh -NoProfile -File $Path } }
function Batch([string]$Path, [string]$Arguments = '') { Step "$Path $Arguments".Trim() { cmd /d /c "`"$Path`" $Arguments" } }

Step 'dotnet test Dbce.Wheel.sln' { dotnet test Dbce.Wheel.sln --nologo -v q }
Step 'dotnet run Dbce.Wheel.ModFile.Tests' { dotnet run --project dotnet/Dbce.Wheel.ModFile.Tests -v q }
Script 'native\wheelffb\build.ps1'
Batch 'native\wheelffb\build.bat' 'x86'
Batch 'native\forza\build_test.bat'
Batch 'native\forcemodel\build_conformance.bat'
Batch 'native\dinput-proxy\build_test.bat'
Script 'native\dinput-proxy\build_ffb_layer.ps1'
Script 'native\dinput-proxy\build_xinput_fence_test.ps1'
Script 'native\panel\build_test.ps1'
Script 'native\ui-host\build.ps1'
Script 'native\selftest\build_test.ps1'
foreach ($t in 'Test-DbceModFile', 'Test-DbceStore', 'Test-SelfTestSchema', 'Test-SyncToolkit', 'Test-SyncNative', 'Test-NativeBinding',
               'Test-UnityConsumer', 'Test-ModPackage', 'Test-SetupWheelkit', 'Test-NewGameMod') { Script "tools\tests\$t.ps1" }
Script 'tools\powershell\Test-DbceWheel.ps1'

if ($failed.Count) { throw "$($failed.Count) step(s) failed: $($failed -join ', ')" }
'build and offline tests passed'
