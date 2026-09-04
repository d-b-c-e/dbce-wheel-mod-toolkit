<#
.SYNOPSIS
    Builds WheelFfb.dll (x64) and verifies every export named in WheelFfb.def.

.DESCRIPTION
    The compile lives in build.bat (MSVC environment setup and quoting are a
    minefield from PowerShell). This wrapper verifies the exports with dumpbin:
    a mangled or missing export fails silently at runtime as an
    EntryPointNotFoundException deep inside a game, which looks like "no FFB".

    vcvars64.bat may print "'vswhere.exe' is not recognized" on some machines;
    that comes from inside Microsoft's script and is harmless.

.PARAMETER CopyTo
    Folder to copy the built DLL into (a consumer's vendored lib folder or a
    game's plugin folder).

.PARAMETER As
    File name to copy it as, when a game dictates the name it P/Invokes
    (art of rally: UnityForceFeedback.dll). Default WheelFfb.dll.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -CopyTo "D:\...\artofrally\Mods\ArtOfSimRally" -As UnityForceFeedback.dll
#>
[CmdletBinding()]
param(
    [string]$CopyTo,
    [string]$As = 'WheelFfb.dll'
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$dll  = Join-Path $here 'build\WheelFfb.dll'
$def  = Join-Path $here 'WheelFfb.def'

Write-Host "Building x64 WheelFfb.dll..." -ForegroundColor Cyan
& cmd.exe /c "$here\build.bat"
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
if (-not (Test-Path $dll)) { throw "Build reported success but $dll is missing" }
Write-Host "Built $dll" -ForegroundColor Green

# Every name in the .def is part of the C ABI contract; verify all of them.
$required = Get-Content $def | Where-Object { $_ -match '^\s+[A-Za-z_][A-Za-z0-9_]*\s*$' } | ForEach-Object { $_.Trim() }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath  = & $vswhere -latest -products * -property installationPath
    $dumpbin = Get-ChildItem "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue |
               Select-Object -First 1 -ExpandProperty FullName
    if ($dumpbin) {
        $exports = (& $dumpbin /exports $dll) -join "`n"
        $missing = $required | Where-Object { $exports -notmatch "\s$_\s*(`n|$)" }
        if ($missing) { throw "DLL is missing exports named in WheelFfb.def: $($missing -join ', ')" }
        Write-Host "All $($required.Count) exports present and undecorated (x64)." -ForegroundColor Green
    } else { Write-Warning "dumpbin not found; exports not verified" }
} else { Write-Warning "vswhere not found; exports not verified" }

# The C header must list every export too - a native consumer resolves by name.
$header = Get-Content (Join-Path $here 'include\wheelffb.h') -Raw
$notInHeader = $required | Where-Object { $header -notmatch "X\([a-z]+,\s*$_\s*," }
if ($notInHeader) { throw "include\wheelffb.h is missing exports named in WheelFfb.def: $($notInHeader -join ', ')" }
Write-Host "include\wheelffb.h lists all $($required.Count) exports." -ForegroundColor Green

if ($CopyTo) {
    if (-not (Test-Path $CopyTo)) { throw "Destination folder not found: $CopyTo" }
    $dest = Join-Path $CopyTo $As
    Copy-Item $dll $dest -Force
    Write-Host "Copied to $dest" -ForegroundColor Green
}
