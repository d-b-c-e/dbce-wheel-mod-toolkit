<# The shared installer engine (PKG-1; dbce-project-mgmt docs/architecture/2026-11/04-cross-cutting.md B.2 and B.6).
   New-ModPackage.ps1 ships it as dbce\Install.ps1 in every mod package; Install.bat, Uninstall.bat and Setup.bat call it.
   Runs in Windows PowerShell 5.1 and PowerShell 7. It reads the package's dbce-mod.json and release-manifest.json and
   writes the game folder's dbce-mod.json with the installed section (docs/DBCE-MOD-JSON.md). Kinds handled: dinput8.

   Install: checks every package file against release-manifest.json, refuses a game folder reached through a link and a
   conflicting file another mod owns, backs up what it replaces in dbce-<id>-backups\<stamp>, seeds config files only
   when absent, and writes every file through a temp file, reads it back and puts everything back if one write fails.
   Installing the same build again changes nothing.
   -Uninstall removes the files the install record lists while they are unchanged, puts back game files it replaced,
   and keeps settings the player changed. -Rollback puts back the exact state before the last install.
   A receipt of the game's earlier installer (package.oldReceipts) is read once, so its file counts as this mod's.
   -Mode Install|Uninstall|Rollback and -PackageRoot are the arguments Wheelkit's PackageScriptRunner passes.
   -Mode Apply (CFG-2; Wheelkit runs it on Play, with the game closed): reads the game's profile from the shared store
   (-StorePath, else %LOCALAPPDATA%\Wheelkit\store\profiles.json; docs/STORE.md) and, when the package has the game's
   dbce\Apply.ps1, writes the files that script returns (game-relative path to bytes, $null deletes) in one transaction.
   Apply.ps1 is the only place a game's boot-time files are written from a profile. #>
[CmdletBinding()]
param(
    [string]$GameDir = '',
    [switch]$Uninstall,
    [switch]$Rollback,
    [switch]$Setup,
    [string]$Mode = '',
    [string]$PackageRoot = '',
    [string]$StorePath = ''
)
$ErrorActionPreference = 'Stop'
# Players see one plain sentence, not PowerShell's error record.
trap { Write-Host ''; Write-Host $_.Exception.Message -ForegroundColor Red; exit 1 }
. (Join-Path $PSScriptRoot 'DbceModFile.ps1')

$action = 'Install'
if ($Uninstall) { $action = 'Uninstall' }
if ($Rollback) { if ($Uninstall) { throw 'Choose -Uninstall or -Rollback, not both. Nothing changed.' }; $action = 'Rollback' }
if ($Setup) { if ($Uninstall -or $Rollback) { throw 'Choose one of -Setup, -Uninstall and -Rollback. Nothing changed.' }; $action = 'Setup' }
if ($Mode) {
    if ($Mode -notin 'Install', 'Uninstall', 'Rollback', 'Apply') { throw "Unknown -Mode '$Mode'. Use Install, Uninstall, Rollback or Apply. Nothing changed." }
    if (($Uninstall -or $Rollback -or $Setup) -and $Mode -ne $action) { throw "-Mode $Mode disagrees with -$action. Nothing changed." }
    $action = $Mode
}
if (-not $PackageRoot) { $PackageRoot = Split-Path -Parent $PSScriptRoot }
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
$packageMod = Join-Path $PackageRoot 'dbce-mod.json'
if (-not [IO.File]::Exists($packageMod)) { throw 'The download is incomplete (dbce-mod.json is missing). Download the zip again and extract all of it. Nothing changed.' }
$mod = Read-DbceModFile -Path $packageMod
if ($null -ne $mod.Installed) { throw 'This package is damaged: its dbce-mod.json already has an install record. Download the zip again. Nothing changed.' }
if ($mod.Package.Kind -ne 'dinput8') { throw "This installer does not handle '$($mod.Package.Kind)' mods yet. Nothing changed." }
$gameName = if ($mod.Folder) { $mod.Folder } else { 'the game' }

function Hash([byte[]]$Bytes) {
    $h = [Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($h.ComputeHash($Bytes)).Replace('-', '').ToLowerInvariant() } finally { $h.Dispose() }
}
function Read-Bytes([string]$Path) {
    if ([IO.File]::Exists($Path)) { return ,([IO.File]::ReadAllBytes($Path)) }
    if ([IO.Directory]::Exists($Path)) { throw "Expected a file but found a folder: $Path. Nothing changed." }
    return $null
}
function Same-Bytes($Left, $Right) {
    if ($null -eq $Left -or $null -eq $Right) { return ($null -eq $Left -and $null -eq $Right) }
    return (Hash $Left) -ceq (Hash $Right)
}
# Refuses a path that is, or sits under, a symbolic link or junction: the copy must land in the real game folder.
function Assert-Unlinked([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        if ([IO.File]::Exists($cursor) -or [IO.Directory]::Exists($cursor)) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "$cursor is a link to another folder. Run the installer with -GameDir set to the real game folder. Nothing changed."
            }
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}
function Assert-Relative([string]$Rel) {
    if (-not $Rel -or [IO.Path]::IsPathRooted($Rel) -or $Rel.Contains('\') -or ($Rel -split '/') -contains '..') { throw "This package is damaged (bad path '$Rel'). Download the zip again. Nothing changed." }
}
function In-Game([string]$Rel) { Assert-Relative $Rel; Join-Path $GameDir ($Rel.Replace('/', '\')) }
function Put-Bytes([string]$Path, [byte[]]$Bytes) {
    $temp = $Path + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllBytes($temp, $Bytes)
        if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temp, $Path, [NullString]::Value) } else { [IO.File]::Move($temp, $Path) }
    } finally { if ([IO.File]::Exists($temp)) { [IO.File]::Delete($temp) } }
}
# Removes the empty folders a deleted file leaves behind, up to the game folder.
function Remove-EmptyParents([string]$Path) {
    $dir = [IO.Path]::GetDirectoryName($Path)
    while ($dir -and $dir.Length -gt $GameDir.Length -and [IO.Directory]::Exists($dir) -and -not [IO.Directory]::EnumerateFileSystemEntries($dir).GetEnumerator().MoveNext()) {
        [IO.Directory]::Delete($dir); $dir = [IO.Path]::GetDirectoryName($dir)
    }
}
# One transaction: every game-relative path gets its new bytes ($null deletes it), each write is read back, and on any
# failure every path is put back as it was, so the game folder never holds half an install.
function Set-Files($Changes) {
    $keys = @($Changes.Keys)
    foreach ($k in $keys) { Assert-Unlinked (In-Game $k) }
    $old = @{}; foreach ($k in $keys) { $old[$k] = Read-Bytes (In-Game $k) }
    $made = New-Object System.Collections.ArrayList
    try {
        foreach ($k in $keys) {
            $path = In-Game $k; $new = $Changes[$k]
            if (Same-Bytes $old[$k] $new) { continue }
            if ($null -eq $new) { [IO.File]::Delete($path); Remove-EmptyParents $path }
            else {
                $dir = [IO.Path]::GetDirectoryName($path); $missing = @()
                while (-not [IO.Directory]::Exists($dir)) { $missing += $dir; $dir = [IO.Path]::GetDirectoryName($dir) }
                if ($missing.Count) { [void][IO.Directory]::CreateDirectory($missing[0]); [void]$made.AddRange($missing) }
                Put-Bytes $path $new
            }
            if (-not (Same-Bytes (Read-Bytes $path) $new)) { throw "$k did not read back as written." }
        }
    } catch {
        $why = $_.Exception.Message
        foreach ($k in $keys) {
            $path = In-Game $k
            try {
                if (Same-Bytes (Read-Bytes $path) $old[$k]) { continue }
                if ($null -eq $old[$k]) { [IO.File]::Delete($path) } else { Put-Bytes $path $old[$k] }
            } catch { throw "Could not change $k ($why), and could not put it back: $($_.Exception.Message). The backups are in the dbce-$($mod.Id)-backups folder." }
        }
        foreach ($d in $made) { if ([IO.Directory]::Exists($d) -and -not [IO.Directory]::EnumerateFileSystemEntries($d).GetEnumerator().MoveNext()) { [IO.Directory]::Delete($d) } }
        throw "Could not change the game folder ($why). Everything was put back as it was."
    }
}

# Finds the game in Steam's libraries (registry, libraryfolders.vdf, the app manifest's installdir). Several: asks.
function Find-Game {
    $libraries = @()
    foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam') {
        $value = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        if (-not $value) { continue }
        foreach ($name in 'SteamPath', 'InstallPath') { $p = $value.PSObject.Properties[$name]; if ($p -and $p.Value) { $libraries += ([string]$p.Value).Replace('/', '\') } }
    }
    foreach ($steam in @($libraries)) {
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if ([IO.File]::Exists($vdf)) { foreach ($line in [IO.File]::ReadAllLines($vdf)) { if ($line -match '"path"\s+"(.+?)"') { $libraries += $Matches[1].Replace('\\', '\') } } }
    }
    $found = @()
    foreach ($library in @($libraries | ForEach-Object { $_.TrimEnd('\') } | Sort-Object -Unique)) {
        $folder = $mod.Folder
        $acf = Join-Path $library ('steamapps\appmanifest_' + $mod.SteamAppId + '.acf')
        if ($mod.SteamAppId -and [IO.File]::Exists($acf) -and ([IO.File]::ReadAllText($acf) -match '"installdir"\s+"([^"\\/]+)"')) { $folder = $Matches[1] }
        if (-not $folder) { continue }
        $candidate = Join-Path $library ('steamapps\common\' + $folder)
        foreach ($exe in $mod.Exe) { if ([IO.File]::Exists((Join-Path $candidate $exe))) { $found += $candidate; break } }
    }
    $found = @($found | Sort-Object -Unique)
    if ($found.Count -eq 1) { return $found[0] }
    if ($found.Count -eq 0) { throw "$gameName was not found in your Steam libraries. Run the installer again with -GameDir ""<the folder with $($mod.Exe -join ' or ')>"". Nothing changed." }
    for ($i = 0; $i -lt $found.Count; $i++) { Write-Host ("{0}: {1}" -f ($i + 1), $found[$i]) }
    $pick = Read-Host "Which $gameName folder? Number"
    if ($pick -notmatch '^\d+$' -or [int]$pick -lt 1 -or [int]$pick -gt $found.Count) { throw 'No folder was chosen. Nothing changed.' }
    $found[[int]$pick - 1]
}

if (-not $GameDir) { $GameDir = Find-Game; Write-Host "$($gameName): $GameDir" }
$GameDir = [IO.Path]::GetFullPath($GameDir).TrimEnd('\')
if (-not [IO.Directory]::Exists($GameDir)) { throw "The folder $GameDir does not exist. Nothing changed." }
if (-not @($mod.Exe | Where-Object { [IO.File]::Exists((Join-Path $GameDir $_)) }).Count) { throw "$GameDir has no $($mod.Exe -join ' or '). Choose the folder the game is installed in. Nothing changed." }
Assert-Unlinked $GameDir
foreach ($exe in $mod.Exe) {
    $running = @(Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($exe)) -ErrorAction SilentlyContinue)
    try { if ($running.Count) { throw "$gameName is running. Close it, then run this again. Nothing changed." } } finally { foreach ($p in $running) { $p.Dispose() } }
}

$receiptPath = Join-Path $GameDir 'dbce-mod.json'
$backupsRel = "dbce-$($mod.Id)-backups"
$backupsRoot = Join-Path $GameDir $backupsRel
Assert-Unlinked $receiptPath; Assert-Unlinked $backupsRoot
$current = $null
if ([IO.File]::Exists($receiptPath)) {
    $current = Read-DbceModFile -Path $receiptPath
    if ($current.Id -ne $mod.Id) { throw "This folder's dbce-mod.json belongs to another mod ($($current.Name)). Nothing changed." }
    if ($null -eq $current.Installed) { throw "This folder's dbce-mod.json has no install record. Nothing changed." }
}
$owned = @{}; $ownedFiles = New-Object System.Collections.ArrayList
function Add-Owned([string]$Rel, [string]$Sha) {
    if ($owned.ContainsKey($Rel.ToLowerInvariant())) { return }
    $owned[$Rel.ToLowerInvariant()] = $Sha.ToLowerInvariant(); [void]$ownedFiles.Add([pscustomobject]@{ Path = $Rel; Sha256 = $Sha.ToLowerInvariant() })
}
if ($null -ne $current) { foreach ($f in $current.Installed.Files) { Add-Owned $f.Path $f.Sha256 } }
# Receipts of the game's installer before this one (package.oldReceipts, {file, sha256}) are read once: the file they
# list is this mod's while unchanged. Install deletes the old receipt (its backup lets -Rollback restore it); uninstall
# also deletes the old backups folder, which holds only earlier builds of this mod.
$oldPresent = @()
foreach ($o in $mod.Package.OldReceipts) {
    $path = In-Game $o.Receipt; Assert-Unlinked $path
    if (-not [IO.File]::Exists($path)) { continue }
    $oldPresent += $o.Receipt
    $r = $null; try { $r = [IO.File]::ReadAllText($path) | ConvertFrom-Json } catch { }
    if ($null -ne $r -and $r.file -and [string]$r.sha256 -match '^[0-9A-Fa-f]{64}$') {
        $rel = ([string]$r.file).Replace('\', '/'); Assert-Relative $rel; Add-Owned $rel ([string]$r.sha256)
    }
}
function Is-Ours([string]$Rel, $Bytes) { $null -ne $Bytes -and $owned.ContainsKey($Rel.ToLowerInvariant()) -and (Hash $Bytes) -ceq $owned[$Rel.ToLowerInvariant()] }
function Remove-Backups([string]$Path) { Assert-Unlinked $Path; if ([IO.Directory]::Exists($Path)) { [IO.Directory]::Delete($Path, $true) } }

if ($action -eq 'Setup') {
    $tool = Join-Path $PSScriptRoot 'dbce-ui.exe'
    if (-not [IO.File]::Exists($tool)) { throw 'This package has no setup tool. Start the game and press F6 for the settings.' }
    if ($null -eq $current) { throw "$($mod.Name) is not installed in $GameDir. Double-click Install.bat first." }
    Start-Process -FilePath $tool -ArgumentList @('--setup', ('"' + $receiptPath + '"')) -WorkingDirectory $GameDir
    Write-Host "The $($mod.Name) setup window is open. Close it when you are done."
    return
}

if ($action -eq 'Apply') {
    if ($null -eq $current) { throw "$($mod.Name) is not installed in $GameDir. Install it first. Nothing changed." }
    . (Join-Path $PSScriptRoot 'DbceStore.ps1')
    if (-not $StorePath) { $StorePath = Join-Path (Get-DbceStoreFolder) 'profiles.json' }
    if (-not [IO.File]::Exists($StorePath)) { Write-Host 'There are no Wheelkit profiles yet. Nothing changed.'; return }
    $choice = Get-DbceStoreChoice -Store (Read-DbceStore -Path $StorePath) -GameId $mod.Id
    $hook = Join-Path $PSScriptRoot 'Apply.ps1'
    if ([IO.File]::Exists($hook)) {
        $changes = & $hook -GameDir $GameDir -Choice $choice -Mod $mod
        if ($changes -isnot [Collections.IDictionary]) { throw 'This package is damaged: dbce\Apply.ps1 returned no file list. Nothing changed.' }
        if ($changes.Count) { Set-Files $changes }
    }
    Write-Host "$($mod.Name) uses the profile $($choice.Profile.Name)."
    return
}

if ($action -eq 'Uninstall') {
    if ($null -eq $current -and -not $oldPresent.Count) { Write-Host "$($mod.Name) is not installed in $GameDir. Nothing changed."; return }
    # Files changed after the install are the player's (a setting) or another tool's: they stay.
    $changes = [ordered]@{}; $kept = @(); $blocked = @()
    foreach ($f in $ownedFiles) {
        $bytes = Read-Bytes (In-Game $f.Path)
        if ($null -eq $bytes) { continue }
        if ((Hash $bytes) -ceq $f.Sha256) { $changes[$f.Path] = $null } else { $kept += $f.Path }
    }
    # Game files the mod replaced go back where the mod's file was removed. One that cannot keeps the backups.
    $originals = Join-Path $backupsRoot 'original'
    if ([IO.Directory]::Exists($originals)) {
        foreach ($file in [IO.Directory]::GetFiles($originals, '*', 'AllDirectories')) {
            $rel = $file.Substring($originals.Length + 1).Replace('\', '/')
            $here = Read-Bytes (In-Game $rel)
            if ($null -eq $here -or ($changes.Contains($rel) -and $null -eq $changes[$rel])) { $changes[$rel] = [IO.File]::ReadAllBytes($file) }
            elseif (-not (Same-Bytes $here ([IO.File]::ReadAllBytes($file)))) { $blocked += $rel }
        }
    }
    $changes['dbce-mod.json'] = $null
    foreach ($o in $oldPresent) { $changes[$o] = $null }
    Set-Files $changes
    Write-Host "Removed $($mod.Name) from $GameDir."
    if ($kept.Count) { Write-Host "Kept because they changed after the install (your settings stay): $($kept -join ', ')." }
    if ($blocked.Count) { Write-Host "The game's own copy of $($blocked -join ', ') is kept in $originals; Steam's ""Verify integrity of game files"" also brings it back." }
    else {
        Remove-Backups $backupsRoot
        foreach ($o in $mod.Package.OldReceipts) { if ($o.Backups) { Remove-Backups (In-Game $o.Backups) } }
    }
    Write-Host "You can start $gameName as usual."
    return
}

if ($action -eq 'Rollback') {
    if ($null -eq $current -or -not $current.Installed.Backups) { throw 'There is no earlier install to go back to. Nothing changed.' }
    $backup = [IO.Path]::GetFullPath((In-Game $current.Installed.Backups))
    if (-not $backup.StartsWith($backupsRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'The install record points outside this game folder. Nothing changed.' }
    Assert-Unlinked $backup
    $listPath = Join-Path $backup 'backup.json'
    if (-not [IO.File]::Exists($listPath)) { throw "The backup in $backup is missing. Nothing changed." }
    $changes = [ordered]@{}
    foreach ($e in @(([IO.File]::ReadAllText($listPath) | ConvertFrom-Json).files)) {
        $rel = [string]$e.path
        $here = Read-Bytes (In-Game $rel)
        if ($rel -ne 'dbce-mod.json' -and $null -ne $here -and -not (Is-Ours $rel $here)) { throw "$rel changed after the install, so going back would lose it. Nothing changed." }
        if ($e.sha256) {
            $bytes = Read-Bytes (Join-Path $backup ($rel.Replace('/', '\')))
            if ($null -eq $bytes -or (Hash $bytes) -cne [string]$e.sha256) { throw "The backup of $rel is missing or changed. Nothing changed." }
            $changes[$rel] = $bytes
        } else { $changes[$rel] = $null }
    }
    Set-Files $changes
    if ([IO.File]::Exists($receiptPath)) {
        Remove-Backups $backup
        $was = (Read-DbceModFile -Path $receiptPath).Installed.Version
        Write-Host "Went back to $($mod.Name) $was in $GameDir."
    } else {
        Remove-Backups $backupsRoot
        if (@($mod.Package.OldReceipts | Where-Object { [IO.File]::Exists((In-Game $_.Receipt)) }).Count) { Write-Host "Went back to the earlier install of $($mod.Name) in $GameDir." }
        else { Write-Host "Went back to $gameName without the mod in $GameDir." }
    }
    Write-Host "You can start $gameName as usual."
    return
}

# Install. Every package file must be exactly as packaged before anything in the game folder changes.
$manifestPath = Join-Path $PackageRoot 'release-manifest.json'
if (-not [IO.File]::Exists($manifestPath)) { throw 'The download is incomplete (release-manifest.json is missing). Download the zip again and extract all of it. Nothing changed.' }
$manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
$listed = @{}
foreach ($f in @($manifest.files)) {
    $rel = [string]$f.path; Assert-Relative $rel
    $bytes = Read-Bytes (Join-Path $PackageRoot ($rel.Replace('/', '\')))
    if ($null -eq $bytes -or (Hash $bytes) -cne ([string]$f.sha256).ToLowerInvariant()) { throw "The download is incomplete or changed ($rel). Download the zip again and extract all of it. Nothing changed." }
    $listed[$rel.ToLowerInvariant()] = $true
}
if ([string]$manifest.commit -notmatch '^[0-9a-f]{40}$' -or [string]$manifest.version -cne $mod.Version) { throw 'This package is damaged (release-manifest.json does not match dbce-mod.json). Download the zip again. Nothing changed.' }
function Package-Bytes([string]$Rel) {
    Assert-Relative $Rel
    if (-not $listed.ContainsKey($Rel.ToLowerInvariant())) { throw "This package is damaged ($Rel is not in release-manifest.json). Download the zip again. Nothing changed." }
    [IO.File]::ReadAllBytes((Join-Path $PackageRoot ($Rel.Replace('/', '\'))))
}

$changes = [ordered]@{}; $files = New-Object System.Collections.ArrayList
$conflicts = @($mod.Package.Conflicts | ForEach-Object { $_.ToLowerInvariant() })
$entries = @($mod.Package.Payload) + @($mod.Package.Optional | Where-Object { $listed.ContainsKey($_.From.ToLowerInvariant()) })
foreach ($e in $entries) {
    $new = Package-Bytes $e.From
    $here = Read-Bytes (In-Game $e.To)
    if ($null -ne $here -and $conflicts -contains $e.To.ToLowerInvariant() -and -not (Is-Ours $e.To $here)) {
        throw "$($e.To) in the game folder belongs to another mod or tool, so $($mod.Name) cannot use it. Remove that mod first, then run this again. Nothing changed."
    }
    $changes[$e.To] = $new
    [void]$files.Add([ordered]@{ path = $e.To; sha256 = (Hash $new) })
}
# Settings files are seeded only when absent (STD-044). A seed the player never changed stays on the record, so
# uninstall removes it; once changed it is the player's and is never touched.
foreach ($e in $mod.Package.SeedConfig) {
    $new = Package-Bytes $e.From
    $here = Read-Bytes (In-Game $e.To)
    if ($null -eq $here) { $changes[$e.To] = $new; [void]$files.Add([ordered]@{ path = $e.To; sha256 = (Hash $new) }) }
    elseif (Is-Ours $e.To $here) { [void]$files.Add([ordered]@{ path = $e.To; sha256 = (Hash $here) }) }
}
# Files of the previous install that this build no longer ships go, while unchanged. So do old receipts.
$keep = @{}; foreach ($f in $files) { $keep[$f.path.ToLowerInvariant()] = $true }
foreach ($f in $ownedFiles) {
    if ($keep.ContainsKey($f.Path.ToLowerInvariant())) { continue }
    $here = Read-Bytes (In-Game $f.Path)
    if (Is-Ours $f.Path $here) { $changes[$f.Path] = $null }
}
foreach ($o in $oldPresent) { $changes[$o] = $null }
$same = $null -ne $current -and $current.Installed.Commit -ceq [string]$manifest.commit -and $current.Installed.Version -ceq $mod.Version
foreach ($k in @($changes.Keys)) { if (-not (Same-Bytes (Read-Bytes (In-Game $k)) $changes[$k])) { $same = $false } }
if ($same) {
    Write-Host "$($mod.Name) $($mod.Version) is already installed in $GameDir. Nothing changed."
    Write-Host "Start $gameName to play."
    return
}

# Back up everything this install changes, so -Rollback can put back the exact state before it.
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$backupRel = "$backupsRel/$stamp"
$backup = Join-Path $backupsRoot $stamp
$originals = Join-Path $backupsRoot 'original'
Assert-Unlinked $backup
$madeRoot = -not [IO.Directory]::Exists($backupsRoot)
$list = New-Object System.Collections.ArrayList
try {
    foreach ($k in @($changes.Keys) + 'dbce-mod.json') {
        $here = Read-Bytes (In-Game $k)
        [void]$list.Add([ordered]@{ path = $k; sha256 = $(if ($null -eq $here) { $null } else { Hash $here }) })
        if ($null -eq $here) { continue }
        $copy = Join-Path $backup ($k.Replace('/', '\'))
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($copy)); [IO.File]::WriteAllBytes($copy, $here)
        # A game file the mod replaces for the first time is kept for uninstall.
        $orig = Join-Path $originals ($k.Replace('/', '\'))
        if ($k -ne 'dbce-mod.json' -and $oldPresent -notcontains $k -and -not (Is-Ours $k $here) -and -not [IO.File]::Exists($orig)) {
            [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($orig)); [IO.File]::WriteAllBytes($orig, $here)
        }
    }
    [void][IO.Directory]::CreateDirectory($backup)
    [IO.File]::WriteAllText((Join-Path $backup 'backup.json'), (@{ schema = 'dbce.mod-backup@1'; files = $list.ToArray() } | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding $false))

    $installed = [ordered]@{
        version = $mod.Version; commit = [string]$manifest.commit; installedUtc = [DateTime]::UtcNow.ToString('o')
        loaderInstalledByUs = $false; files = $files.ToArray(); backups = $backupRel
    }
    $record = ($installed | ConvertTo-Json -Depth 4) -split "`r?`n"
    $text = [IO.File]::ReadAllText($packageMod, [Text.Encoding]::UTF8).TrimEnd()
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    if (-not $text.EndsWith('}')) { throw 'This package is damaged (dbce-mod.json). Download the zip again. Nothing changed.' }
    $text = $text.Substring(0, $text.Length - 1).TrimEnd() + ',' + $nl + '  "installed": ' + ($record -join ($nl + '  ')) + $nl + '}' + $nl
    $changes['dbce-mod.json'] = (New-Object Text.UTF8Encoding $false).GetBytes($text)
    Set-Files $changes
} catch {
    if ($madeRoot) { Remove-Backups $backupsRoot } else { Remove-Backups $backup }
    throw
}
Write-Host "Installed $($mod.Name) $($mod.Version) in $GameDir."
$keys = if ($null -ne $mod.Settings -and $mod.Settings.Keys) { " In the game: $($mod.Settings.Keys)." } else { '' }
Write-Host "Start $gameName to play.$keys"
