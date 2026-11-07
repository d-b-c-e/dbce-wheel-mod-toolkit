#Requires -Version 7.0
<# The player package of one mod (PKG-1; dbce-project-mgmt docs/architecture/2026-11/04-cross-cutting.md B.6, STD-040).
   Reads the committed dbce-mod.json at the game repo's root and writes "<name> <version>.zip" (name is the mod's
   product name, such as "DiRT 4 Wheel Mod") with a .sha256 file beside it, in <repo>\dist unless -Out says otherwise.

   The zip holds, at the root: Install.bat, Uninstall.bat, Setup.bat (with a setup tool), README.txt, dbce-mod.json and
   release-manifest.json (version, commit, SHA-256 of every other file, all computed here). Then the payload, optional
   and seed files at their "from" paths (relative to the repo root here, and to the package root in the zip), the
   loader bundle when there is one, dbce\Install.ps1 with its reader, dbce\dbce-ui.exe (the setup tool, for a mod with
   settings) and licenses\ (the repo's LICENSE*, THIRD-PARTY-NOTICES* and licenses\ folder, plus the toolkit's).

   Refuses a repo with uncommitted tracked changes, a dbce-mod.json that is not committed, fails the schema or has an
   installed section, a missing payload file, a kind or knownBuilds the installer does not handle yet, and an
   existing zip. Tested by tools\tests\Test-ModPackage.ps1. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Repo,
    [string]$Out = '',
    # dbce-ui.exe for "Setup.bat"; defaults to the toolkit's native\ui-exe\build\dbce-ui.exe.
    [string]$SetupTool = ''
)
$ErrorActionPreference = 'Stop'
$toolkit = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$installer = Join-Path $toolkit 'installer'
. (Join-Path $installer 'DbceModFile.ps1')
Set-StrictMode -Off

$Repo = [IO.Path]::GetFullPath($Repo)
$modPath = Join-Path $Repo 'dbce-mod.json'
if (-not [IO.File]::Exists($modPath)) { throw "$Repo has no dbce-mod.json at its root." }
$head = (git -C $Repo rev-parse HEAD 2>$null)
if ($LASTEXITCODE -or "$head" -notmatch '^[0-9a-f]{40}$') { throw "$Repo is not a git repository with a commit." }
git -C $Repo ls-files --error-unmatch -- dbce-mod.json *> $null
if ($LASTEXITCODE) { throw 'dbce-mod.json is not committed; commit it first. Nothing packaged.' }
if (git -C $Repo status --porcelain --untracked-files=no) { throw 'Commit the source first; nothing packaged.' }

$text = [IO.File]::ReadAllText($modPath)
if (-not (Test-Json -Json $text -SchemaFile (Join-Path $toolkit 'schema\dbce-mod.schema.json') -ErrorAction SilentlyContinue)) { throw 'dbce-mod.json does not match schema\dbce-mod.schema.json; nothing packaged.' }
$mod = Read-DbceModFile -Path $modPath
if ($null -ne $mod.Installed) { throw "The repo's dbce-mod.json must not have an installed section (only the installer writes it); nothing packaged." }
if ($mod.Package.Kind -ne 'dinput8') { throw "The shared installer does not handle '$($mod.Package.Kind)' mods yet; nothing packaged." }
if (@($mod.Package.KnownBuilds).Count) { throw 'The shared installer does not check knownBuilds yet; nothing packaged.' }

if (-not $Out) { $Out = Join-Path $Repo 'dist' }
$Out = [IO.Path]::GetFullPath($Out)
$zip = Join-Path $Out "$($mod.Name) $($mod.Version).zip"
if ([IO.File]::Exists($zip)) { throw "$zip already exists; keep it and choose a new version. Nothing packaged." }
$stage = Join-Path $Out ('.stage-' + [guid]::NewGuid().ToString('N'))
function Put([string]$From, [string]$To) {
    $target = Join-Path $stage $To
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
    [IO.File]::Copy($From, $target)
}
try {
    foreach ($e in @($mod.Package.Payload) + @($mod.Package.SeedConfig)) {
        $src = Join-Path $Repo $e.From
        if (-not [IO.File]::Exists($src)) { throw "$($e.From) is missing; build the mod first. Nothing packaged." }
        Put $src $e.From
    }
    foreach ($e in $mod.Package.Optional) { $src = Join-Path $Repo $e.From; if ([IO.File]::Exists($src)) { Put $src $e.From } }
    if ($null -ne $mod.Package.Loader -and $mod.Package.Loader.Bundle) {
        $bundle = Join-Path $Repo $mod.Package.Loader.Bundle
        if (-not [IO.Directory]::Exists($bundle)) { throw "The loader bundle $($mod.Package.Loader.Bundle) is missing. Nothing packaged." }
        foreach ($f in [IO.Directory]::GetFiles($bundle, '*', 'AllDirectories')) { Put $f (Join-Path $mod.Package.Loader.Bundle ([IO.Path]::GetRelativePath($bundle, $f))) }
    }
    Put $modPath 'dbce-mod.json'
    Put (Join-Path $installer 'Install.ps1') 'dbce\Install.ps1'
    Put (Join-Path $installer 'DbceModFile.ps1') 'dbce\DbceModFile.ps1'
    Put (Join-Path $installer 'Install.bat') 'Install.bat'
    Put (Join-Path $installer 'Uninstall.bat') 'Uninstall.bat'
    Put (Join-Path $toolkit 'LICENSE') 'licenses\dbce-wheel-mod-toolkit-LICENSE.txt'
    $hasSetup = $null -ne $mod.Settings
    if ($hasSetup) {
        if (-not $SetupTool) { $SetupTool = Join-Path $toolkit 'native\ui-exe\build\dbce-ui.exe' }
        if (-not [IO.File]::Exists($SetupTool)) { throw "The setup tool $SetupTool is missing; run native\ui-exe\build.ps1 or pass -SetupTool. Nothing packaged." }
        Put $SetupTool 'dbce\dbce-ui.exe'
        Put (Join-Path $installer 'Setup.bat') 'Setup.bat'
        Put (Join-Path $toolkit 'native\third_party\imgui\LICENSE.txt') 'licenses\DearImGui-MIT.txt'
    }
    foreach ($f in Get-ChildItem -LiteralPath $Repo -File | Where-Object { $_.Name -like 'LICENSE*' -or $_.Name -like 'THIRD-PARTY-NOTICES*' }) { Put $f.FullName "licenses\$($f.Name)" }
    $licenses = Join-Path $Repo 'licenses'
    if ([IO.Directory]::Exists($licenses)) { foreach ($f in [IO.Directory]::GetFiles($licenses, '*', 'AllDirectories')) { Put $f (Join-Path 'licenses' ([IO.Path]::GetRelativePath($licenses, $f))) } }

    $game = if ($mod.Folder) { $mod.Folder } else { 'the game' }
    $readme = @("$($mod.Name) $($mod.Version)", '', 'Install', "  1. Close $game.",
        '  2. Double-click Install.bat. It finds the game through Steam and adds the mod.',
        '     If it cannot find the game, it tells you how to point it at the game folder.', '')
    if ($hasSetup) { $readme += @('Settings', '  In the game: press F6. With the game closed: double-click Setup.bat.', '') }
    if ($mod.Settings -and $mod.Settings.Keys) { $readme += @('Keys', "  $($mod.Settings.Keys)", '') }
    $readme += @('Undo', "  Close $game and double-click Uninstall.bat. The game's own files come back.",
        '  Settings you changed are kept.', '')
    if ($mod.LogFile) { $readme += @("Log: $($mod.LogFile)", '') }
    if ($mod.Repository) { $readme += @("Help and updates: https://github.com/$($mod.Repository)") }
    [IO.File]::WriteAllLines((Join-Path $stage 'README.txt'), [string[]]$readme, (New-Object Text.UTF8Encoding $false))

    $files = foreach ($f in Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object { [IO.Path]::GetRelativePath($stage, $_.FullName) }) {
        [ordered]@{ path = [IO.Path]::GetRelativePath($stage, $f.FullName).Replace('\', '/'); sha256 = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    }
    $manifest = [ordered]@{ schema = 'dbce.release-manifest@1'; id = $mod.Id; name = $mod.Name; version = $mod.Version; commit = "$head"; files = @($files) }
    [IO.File]::WriteAllText((Join-Path $stage 'release-manifest.json'), ($manifest | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding $false))
    [void][IO.Directory]::CreateDirectory($Out)
    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip)
} finally { if ([IO.Directory]::Exists($stage)) { [IO.Directory]::Delete($stage, $true) } }
$sha = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$zip.sha256", "$sha  $([IO.Path]::GetFileName($zip))`n", [Text.Encoding]::ASCII)
"Packaged $zip ($(@($files).Count + 1) files, commit $($head.Substring(0, 12)))"
