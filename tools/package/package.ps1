<#
.SYNOPSIS
    Builds everything and produces dist\dbce-wheel-mod-toolkit-<version>.zip.

.DESCRIPTION
    Contents (all built artifacts, no source):
      native\WheelFfb.dll                       the DirectInput FFB and device layer (x64)
      native\x86\WheelFfb.dll                   the same for 32-bit games
      native\include\*.h                        C ABI + loader, Forza encoder, force model + profiles
      profiles\force-profiles.ini               the versioned force-feedback tunes, deployed beside the game
      tools\powershell\DbceWheel.psm1           Steam discovery, DirectInput enumeration, CRLF-safe INI
      dotnet\Dbce.Wheel.Telemetry.dll (+xml)    Forza encoder, idle keeper, RPM synth
      dotnet\Dbce.Wheel.Ffb.dll (+xml)          binding, shaper, model, diagnostics
      tools\forza\*.py, tools\dev\*.ps1, tools\Sync-Toolkit.ps1, tools\ffbprobe\ffbprobe.exe (if built)
      knowledge\**\*.md, docs\adr\*.md, README.md, CHANGELOG.md, LICENSE
    Consumers vendor from this zip with Sync-Toolkit.ps1.

.PARAMETER Version
    Version string for the zip and the assemblies, e.g. 0.1.0.

.PARAMETER SkipNative
    Reuse the existing native\wheelffb\build\WheelFfb.dll instead of rebuilding.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?$')]
    [string]$Version = '0.1.0',
    [switch]$SkipNative
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
Set-Location $root

Write-Host "== native ==" -ForegroundColor Cyan
if (-not $SkipNative) { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'native\wheelffb\build.ps1'); if ($LASTEXITCODE -ne 0) { throw "native build failed" } }
$native = Join-Path $root 'native\wheelffb\build\WheelFfb.dll'
if (-not (Test-Path $native)) { throw "WheelFfb.dll not built" }
Write-Host "== native x86 (32-bit games: OutRun 2006) ==" -ForegroundColor Cyan
if (-not $SkipNative) { & cmd.exe /c "$(Join-Path $root 'native\wheelffb\build.bat') x86"; if ($LASTEXITCODE -ne 0) { throw "native x86 build failed" } }
$nativeX86 = Join-Path $root 'native\wheelffb\build\x86\WheelFfb.dll'
if (-not (Test-Path $nativeX86)) { throw "x86 WheelFfb.dll not built" }
Write-Host "== DbceWheel.psm1 ==" -ForegroundColor Cyan
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tools\powershell\Test-DbceWheel.ps1') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "DbceWheel.psm1 tests failed" }

Write-Host "== dinput proxy scaffold ==" -ForegroundColor Cyan
& cmd.exe /c (Join-Path $root 'native\dinput-proxy\build_test.bat') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "dinput_proxy.h checks failed" }

Write-Host "== force model conformance (C++ vs the golden file) ==" -ForegroundColor Cyan
& cmd.exe /c (Join-Path $root 'native\forcemodel\build_conformance.bat') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "force model conformance failed" }

Write-Host "== forza_packet.h conformance ==" -ForegroundColor Cyan
& cmd.exe /c (Join-Path $root 'native\forza\build_test.bat') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "forza_packet.h test failed" }

Write-Host "== sync and native binding regression ==" -ForegroundColor Cyan
& (Join-Path $root 'tools/tests/Test-SyncToolkit.ps1') | Out-Host
& (Join-Path $root 'tools/tests/Test-NativeBinding.ps1') | Out-Host

Write-Host "== finite burst fake effects (no hardware) ==" -ForegroundColor Cyan
foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'native/wheelffb/test/build_burst_test.bat') $arch | Out-Host
 if ($LASTEXITCODE -ne 0) { throw "Burst fake effect checks failed: $arch" }
}

Write-Host "== opt-in force tracing (fake output, no hardware) ==" -ForegroundColor Cyan
foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'native/wheelffb/test/build_logging_test.bat') $arch | Out-Host
 if ($LASTEXITCODE -ne 0) { throw "Force logging checks failed: $arch" }
}

Write-Host "== finite constant burst fake effects ==" -ForegroundColor Cyan
foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'native/wheelffb/test/build_constant_burst_test.bat') $arch | Out-Host
 if ($LASTEXITCODE -ne 0) { throw "Constant burst checks failed: $arch" }
}

Write-Host "== dotnet ==" -ForegroundColor Cyan
dotnet build (Join-Path $root 'Dbce.Wheel.sln') -c Release "-p:Version=$Version" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "dotnet build failed" }
dotnet test (Join-Path $root 'Dbce.Wheel.sln') -c Release --no-build | Out-Host
if ($LASTEXITCODE -ne 0) { throw "tests failed" }

$probeSrc = Join-Path $root 'tools\ffbprobe'
if (-not (Test-Path (Join-Path $probeSrc 'ffbprobe.exe'))) {
    Write-Host "== ffbprobe ==" -ForegroundColor Cyan
    try { & cmd.exe /c (Join-Path $probeSrc 'build.bat') | Out-Host } catch { Write-Warning "ffbprobe did not build; continuing without it" }
}

$name  = "dbce-wheel-mod-toolkit-$Version"
$dist  = Join-Path $root 'dist'
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'native\x86'), (Join-Path $stage 'native\include'), (Join-Path $stage 'dotnet'), (Join-Path $stage 'tools\forza'), (Join-Path $stage 'tools\dev'), (Join-Path $stage 'tools\ffbprobe'), (Join-Path $stage 'tools\powershell'), (Join-Path $stage 'profiles'), (Join-Path $stage 'knowledge'), (Join-Path $stage 'docs\adr') | Out-Null

Copy-Item $native (Join-Path $stage 'native')
Copy-Item $nativeX86 (Join-Path $stage 'native\x86')
Copy-Item (Join-Path $root 'native\wheelffb\include\wheelffb.h') (Join-Path $stage 'native\include')
Copy-Item (Join-Path $root 'native\forza\forza_packet.h') (Join-Path $stage 'native\include')
Copy-Item (Join-Path $root 'native\forcemodel\force_model.h') (Join-Path $stage 'native\include')
Copy-Item (Join-Path $root 'native\forcemodel\force_profile.h') (Join-Path $stage 'native\include')
Copy-Item (Join-Path $root 'native\dinput-proxy\dinput_proxy.h') (Join-Path $stage 'native\include')
Copy-Item (Join-Path $root 'profiles\force-profiles.ini') (Join-Path $stage 'profiles')
Copy-Item (Join-Path $root 'tools\powershell\DbceWheel.psm1') (Join-Path $stage 'tools\powershell')
Copy-Item (Join-Path $root 'tools\powershell\Test-DbceWheel.ps1') (Join-Path $stage 'tools\powershell')
$ffbBin = Join-Path $root 'dotnet\Dbce.Wheel.Ffb\bin\Release\netstandard2.0'
Copy-Item (Join-Path $ffbBin 'Dbce.Wheel.*.dll') (Join-Path $stage 'dotnet')
Copy-Item (Join-Path $ffbBin 'Dbce.Wheel.*.xml') (Join-Path $stage 'dotnet') -ErrorAction SilentlyContinue
Copy-Item (Join-Path $root 'tools\forza\*.py') (Join-Path $stage 'tools\forza')
Copy-Item (Join-Path $root 'tools\dev\*.ps1') (Join-Path $stage 'tools\dev')
Copy-Item (Join-Path $root 'tools\Sync-Toolkit.ps1') (Join-Path $stage 'tools')
if (Test-Path (Join-Path $probeSrc 'ffbprobe.exe')) { Copy-Item (Join-Path $probeSrc 'ffbprobe.exe') (Join-Path $stage 'tools\ffbprobe') }
Copy-Item (Join-Path $probeSrc 'README.md') (Join-Path $stage 'tools\ffbprobe') -ErrorAction SilentlyContinue
Copy-Item (Join-Path $root 'knowledge\*') (Join-Path $stage 'knowledge') -Recurse
Copy-Item (Join-Path $root 'docs\*') (Join-Path $stage 'docs') -Recurse -Force
foreach ($f in 'README.md', 'CHANGELOG.md', 'LICENSE') { Copy-Item (Join-Path $root $f) $stage -ErrorAction SilentlyContinue }
Set-Content (Join-Path $stage 'VERSION') "$Version`n$(git -C $root rev-parse --short HEAD)`n$(Get-Date -Format s)"

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Packaged $zip ($([math]::Round((Get-Item $zip).Length / 1KB)) KB)" -ForegroundColor Green
