#Requires -Version 5.1
<# The Wheelkit half of the player package's setup (CFG-3, STD-053; 08-install-and-configuration.md item 1). Every mod
   package carries the full Wheelkit offline installer, and Install.bat runs this script before the mod installer.
   It keeps exactly one central Wheelkit, always the newest: it installs %LOCALAPPDATA%\Programs\Wheelkit when it is
   missing, updates it when the bundled package is newer, and leaves it alone when it is already as new. The game is
   then added to Wheelkit by the mod installer that runs next (the installed dbce-mod.json is what Wheelkit reads).
   Runs in Windows PowerShell 5.1 and PowerShell 7. Tested by tools\tests\Test-SetupWheelkit.ps1 (missing, older, equal).
#>
[CmdletBinding()]
param(
    # The bundled Wheelkit-<version>-win-x64.zip. Defaults to the one beside this script.
    [string]$Package = '',
    # The install folder. Defaults to %LOCALAPPDATA%\Programs\Wheelkit; tests pass a temp folder.
    [string]$Target = ''
)
$ErrorActionPreference = 'Stop'
trap { Write-Host ''; Write-Host $_.Exception.Message -ForegroundColor Red; exit 1 }

if (-not $Package) {
    $zips = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'Wheelkit-*-win-x64.zip' -File -ErrorAction SilentlyContinue)
    if ($zips.Count -ne 1) { throw 'The Wheelkit installer is missing from this download. Download the zip again and extract all of it. Nothing changed.' }
    $Package = $zips[0].FullName
}
$Package = [IO.Path]::GetFullPath($Package)
if (-not [IO.File]::Exists($Package)) { throw 'The Wheelkit installer is missing from this download. Download the zip again and extract all of it. Nothing changed.' }
$match = [regex]::Match([IO.Path]::GetFileName($Package), '^Wheelkit-(\d+\.\d+\.\d+)-win-x64\.zip$')
if (-not $match.Success) { throw "The Wheelkit package must be named Wheelkit-<version>-win-x64.zip, not $([IO.Path]::GetFileName($Package)). Nothing changed." }
$bundled = $match.Groups[1].Value

$defaultTarget = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs\Wheelkit'))
if (-not $Target) { $Target = $defaultTarget }
$Target = [IO.Path]::GetFullPath($Target)
$marker = Join-Path $Target 'wheelkit-install.json'

$installed = ''
if ([IO.File]::Exists($marker)) {
    $m = [IO.File]::ReadAllText($marker, [Text.Encoding]::UTF8) | ConvertFrom-Json
    $installed = (([string]$m.version) -split '\+')[0]
}
$needs = $installed -eq ''
if (-not $needs) { $needs = ([Version]$bundled).CompareTo([Version]$installed) -gt 0 }

if (-not $needs) {
    Write-Host "Wheelkit $installed is already up to date in $Target."
    return
}

# Quit a running copy from this folder first; --quit asks it to stop, as its tray Quit does.
foreach ($p in @(Get-Process Wheelkit -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($Target, [StringComparison]::OrdinalIgnoreCase) })) {
    Start-Process (Join-Path $Target 'Wheelkit.exe') -ArgumentList '--quit' -Wait
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Process -Id $p.Id -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
}
if (@(Get-Process Wheelkit -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($Target, [StringComparison]::OrdinalIgnoreCase) }).Count) {
    throw 'Wheelkit is still running from that folder: let its current operation finish, or Quit it from its tray icon. Nothing changed.'
}
# Only replace our own install: a folder without our marker belongs to its owner.
if ((Test-Path -LiteralPath $Target) -and -not (Test-Path -LiteralPath $marker) -and @(Get-ChildItem -LiteralPath $Target -Force -ErrorAction SilentlyContinue).Count) {
    throw "$Target exists and is not a Wheelkit install. Leave it to its owner. Nothing changed."
}

$stage = "$Target.new"
if (Test-Path -LiteralPath $stage) { [IO.Directory]::Delete($stage, $true) }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($Package, $stage)
if (-not (Test-Path -LiteralPath (Join-Path $stage 'Wheelkit.exe'))) {
    [IO.Directory]::Delete($stage, $true)
    throw 'The Wheelkit package has no Wheelkit.exe. Nothing changed.'
}
if (Test-Path -LiteralPath $Target) { [IO.Directory]::Delete($Target, $true) }
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Target))
[IO.Directory]::Move($stage, $Target)
[IO.File]::WriteAllText($marker, (@{ schema = 'wheelkit.install@1'; version = $bundled; package = [IO.Path]::GetFileName($Package); installed = [DateTime]::UtcNow.ToString('o') } | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))
# The Start menu entry goes with the central install only, never a test or side install.
if ($Target.TrimEnd('\') -ieq $defaultTarget.TrimEnd('\')) {
    $shell = New-Object -ComObject WScript.Shell
    $link = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'Wheelkit.lnk'))
    $link.TargetPath = Join-Path $Target 'Wheelkit.exe'
    $link.WorkingDirectory = $Target
    $link.IconLocation = (Join-Path $Target 'Wheelkit.exe') + ',0'
    $link.Description = 'Wheelkit: racing mods and rig setup'
    $link.Save()
}
Write-Host "Installed Wheelkit $bundled in $Target."
