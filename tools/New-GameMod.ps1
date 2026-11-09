<#
.SYNOPSIS
Starts a new game mod from a template that already builds: a repo folder with the shared toolkit vendored, the mod's
descriptor, one build command and a passing start check.

.DESCRIPTION
Families so far: native-dinput8-proxy (a dinput8.dll beside the game's exe, on native/dinput-proxy/dbce_proxy_core.hpp).
The new folder is a git repo with one commit. `pwsh build.ps1` in it builds the DLL and runs the start check.
What is left for the game is only the game's: its hooks, its F6 pages and its cameras (see the TODO in src/proxy.cpp).

.EXAMPLE
pwsh tools\New-GameMod.ps1 -Id karting -Name 'Karting Superstars' -Exe KartingSuperstars.exe -SteamAppId 1234567 -Folder 'Karting Superstars'
#>
[CmdletBinding()]
param(
    [ValidateSet('native-dinput8-proxy')][string]$Family = 'native-dinput8-proxy',
    # Short lower-case name: the log folder and the stem of the log, ini and mute file names.
    [Parameter(Mandatory)][ValidatePattern('^[a-z][a-z0-9]{1,23}$')][string]$Id,
    # The game's name as players know it.
    [Parameter(Mandatory)][string]$Name,
    # The game's exe file name.
    [Parameter(Mandatory)][ValidatePattern('^[^\\/:*?"<>|]+\.exe$')][string]$Exe,
    [int]$SteamAppId = 0,
    # The game's folder name under steamapps\common.
    [string]$Folder = '',
    # Where to create the repo. Default: game-modding\dbce-mods-<id> beside the toolkits.
    [string]$Out = '',
    # Toolkit commits to vendor. Default: the heads of both toolkits' default branches.
    [string]$Commit = 'master',
    [string]$TripleCommit = 'main'
)
$ErrorActionPreference = 'Stop'
$toolkit = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$template = Join-Path $toolkit "templates\$Family"
$repoName = "dbce-mods-$Id"
if (-not $Out) { $Out = Join-Path $toolkit "..\..\$repoName" }
$Out = [IO.Path]::GetFullPath($Out)
if (Test-Path -LiteralPath $Out) { throw "$Out already exists. Nothing changed." }
if ($Name -match '["\\]') { throw 'The name cannot contain a quote or a backslash. Nothing changed.' }

$utf8 = New-Object Text.UTF8Encoding $false
function Put([string]$Relative, [string]$Text) {
    $path = Join-Path $Out $Relative
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    [IO.File]::WriteAllText($path, ($Text -replace "`r?`n", "`r`n"), $utf8)
}
function Fill([string]$Text) { $Text.Replace('__ID__', $Id).Replace('__NAME__', $Name).Replace('__EXE__', $Exe).Replace('__REPO__', $repoName) }

foreach ($file in 'src\proxy.cpp', 'src\dinput8.def', 'tests\host.cpp', 'build.ps1') {
    Put $file (Fill ([IO.File]::ReadAllText((Join-Path $template $file))))
}
$mod = [ordered]@{
    schema = 'dbce.mod@1'; id = $Id; name = "$Name Wheel Mod"; version = '0.1.0'
}
if ($SteamAppId) { $mod.steamAppId = $SteamAppId }
if ($Folder) { $mod.folder = $Folder }
$mod.exe = @($Exe)
$mod.repository = "d-b-c-e/$repoName"
$mod.log = [ordered]@{ file = "%LOCALAPPDATA%\dbce\$Id\dbce-$Id.log"; start = "dbce $([regex]::Escape($Name)) mod (?<v>[0-9]\S*) in $([regex]::Escape($Exe))" }
$mod.package = [ordered]@{ kind = 'dinput8'; loader = $null; payload = @([ordered]@{ from = 'build/dinput8.dll'; to = 'dinput8.dll' }); conflicts = @('dinput8.dll'); uiHost = 'native-panel' }
Put 'dbce-mod.json' (($mod | ConvertTo-Json -Depth 6) + "`n")
Put '.gitignore' "build/`ndist/`nartifacts/`n.local/`n"
Put '.gitattributes' "* text=auto`n*.bat text eol=crlf`n"
Put 'CLAUDE.md' "@AGENTS.md`n"
Put 'AGENTS.md' (Fill @'
# __NAME__ mod: agent guide

Portfolio rules: `E:\Source\dbce-project-mgmt\HOW-WE-WORK.md` (read it first). Current focus: `E:\Source\dbce-project-mgmt\NOW.md`. Feature status lives in the portfolio grid, not in this file.

## Game and mod
- **Game:** __NAME__, exe `__EXE__`.
- **How the mod loads:** `dinput8.dll` beside the game's exe, built on the toolkit's shared proxy core (`src/vendor/dinput-proxy/dbce_proxy_core.hpp`).
- **Log:** `%LOCALAPPDATA%\dbce\__ID__\dbce-__ID__.log`. **Settings:** `dbce-__ID__.ini` beside the exe.

## Build
- **One command:** `pwsh build.ps1` at the repo root checks the vendored toolkit, builds `build\dinput8.dll` and runs the offline tests, with no arguments. Run it before pushing.
- Player package: `pwsh ..\_toolkits\dbce-wheel-mod-toolkit\tools\package\New-ModPackage.ps1 -Repo .` (reads `dbce-mod.json`).

## Unattended runs
- Put `ffb-fence.on` in the log folder before the game starts: every device stays visible, and no force can reach one.
- Back up the game's saves and config before a run and restore them afterwards (HOW-WE-WORK, Safety).

## Shared code
- `src/vendor` is an exact copy of the toolkits at the commits in `src/vendor/VERSION`. Never edit it: change the toolkit, then run `pwsh ..\_toolkits\dbce-wheel-mod-toolkit\tools\Sync-Native.ps1 -GameRepo .`.
- Keep this repo to what is this game's: its hooks, its F6 pages and its cameras. If a sibling game needs it too, it belongs in the toolkit.
'@)
Put 'README.md' (Fill "# __NAME__ wheel mod`n`nForce feedback strength, wheel support and the F6 settings panel for __NAME__.`n`nBuild: ``pwsh build.ps1``. See ``AGENTS.md``.`n")

& pwsh -NoProfile -File (Join-Path $PSScriptRoot 'Sync-Native.ps1') -GameRepo $Out -Commit $Commit -TripleCommit $TripleCommit | Select-Object -Last 1
if ($LASTEXITCODE) { throw 'Sync-Native failed; the new folder is incomplete.' }
& git -C $Out init -q -b main
& git -C $Out add -A
& git -C $Out -c user.name='New-GameMod' -c user.email='new-game-mod@localhost' commit -q -m "Start the $Name mod from the toolkit's $Family template"
"Created $Out. Next: pwsh build.ps1, then the TODO in src\proxy.cpp."
