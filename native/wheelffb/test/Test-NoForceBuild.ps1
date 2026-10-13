<#
.SYNOPSIS
    Qualifies the internal no-force WheelFfb build for one architecture, against fake devices and the built DLLs only.

.DESCRIPTION
    1. Builds the product and the no-force DLL (build.ps1 checks exports, header, marker; the no-force build writes its
       provenance).
    2. The two DLLs export exactly the same names.
    3. build_injection_test.bat: the existing scenarios on the product source, then the no-force harness (latched
       from load, every force export refused with the fakes untouched, no guard or watchdog, reads and injection live).
    4. build_noforce_dll_check.bat: the actual no-force DLL is armed at its first call, has the product's version and
       refuses force at the gate.
    No device, game, window (beyond one message-only test window) or force is touched.

.EXAMPLE
    .\Test-NoForceBuild.ps1
    .\Test-NoForceBuild.ps1 -Arch x86
#>
[CmdletBinding()]
param([ValidateSet('x64', 'x86')][string]$Arch = 'x64')

$ErrorActionPreference = 'Stop'
$test = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $test

& (Join-Path $root 'build.ps1') -Arch $Arch
& (Join-Path $root 'build.ps1') -Arch $Arch -NoForce

$vsPath  = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$dumpbin = Get-ChildItem "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" | Select-Object -First 1 -ExpandProperty FullName
function Get-ExportNames([string]$Dll) {
    $text = (& $dumpbin /exports $Dll) -join "`n"
    @([regex]::Matches($text, '(?m)^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object)
}
$sub = if ($Arch -eq 'x86') { '\x86' } else { '' }
$product = Get-ExportNames (Join-Path $root "build$sub\WheelFfb.dll")
$noForce = Get-ExportNames (Join-Path $root "build\noforce$sub\WheelFfb.dll")
if (Compare-Object $product $noForce) { throw "Export names differ between the product and the no-force DLL ($Arch)" }
Write-Host "Product and no-force DLL export the same $($product.Count) names ($Arch)." -ForegroundColor Green

& cmd.exe /c (Join-Path $test 'build_injection_test.bat') $Arch
if ($LASTEXITCODE -ne 0) { throw "build_injection_test.bat $Arch failed ($LASTEXITCODE)" }
& cmd.exe /c (Join-Path $test 'build_noforce_dll_check.bat') $Arch
if ($LASTEXITCODE -ne 0) { throw "build_noforce_dll_check.bat $Arch failed ($LASTEXITCODE)" }
Write-Host "No-force build qualified ($Arch)." -ForegroundColor Green
