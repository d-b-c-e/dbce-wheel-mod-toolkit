<#
.SYNOPSIS
    Promotes a force-feedback tune from a game folder into this repo as a new
    published profile version.

.DESCRIPTION
    A tune you arrive at by driving lives in force-profiles.user.ini next to the
    game, where nothing backs it up and a Steam file verification would remove
    it. This brings it home: the section is appended to
    profiles/force-profiles.ini as the next version of its family, so it is
    committed, diffable, and available to every consumer from the next release.

    Published versions are immutable, so this NEVER overwrites an existing id.
    It takes the family name from the id you give and assigns the next free
    version, which is what you want anyway - the point of a version is that
    "arcade-outrun@2 felt harsh" stays true forever.

    Reads force-profiles.user.ini first, then force-profiles.ini, so a tune that
    overrides a shipped id is found in preference to the shipped one.

.PARAMETER From
    The game folder holding force-profiles.user.ini (or force-profiles.ini).

.PARAMETER Id
    Profile to take, as name@version, e.g. mytune@1.

.PARAMETER As
    Family name to publish it under. Defaults to the family in -Id. The version
    is always chosen here, never taken from -Id.

.PARAMETER Description
    One line saying what this tune is and where it came from. Prompted for if
    omitted, because a profile without one is unusable in six weeks.

.PARAMETER WhatIf
    Show the section that would be appended and change nothing.

.EXAMPLE
    .\tools\Import-Profile.ps1 -From 'E:\...\game' -Id mytune@1 -As arcade-outrun
    .\tools\Import-Profile.ps1 -From 'E:\...\game' -Id arcade-outrun@2 -WhatIf
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)] [string]$From,
    [Parameter(Mandatory = $true)] [string]$Id,
    [string]$As,
    [string]$Description
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$master = Join-Path $repo 'profiles\force-profiles.ini'
if (-not (Test-Path $master)) { throw "not found: $master" }
if (-not (Test-Path $From)) { throw "not found: $From" }

# ---- read the section out of the game folder --------------------------------
$sources = @('force-profiles.user.ini', 'force-profiles.ini') |
    ForEach-Object { Join-Path $From $_ } | Where-Object { Test-Path $_ }
if (-not $sources) { throw "no force-profiles.ini or force-profiles.user.ini in $From" }

function Get-Section {
    param([string]$Path, [string]$Want)
    # $null on the LEFT throughout: "$collection -ne $null" filters the
    # collection instead of testing it, which is the classic way to get an
    # empty result here and conclude the profile is missing.
    $body = $null; $found = $null
    foreach ($line in (Get-Content $Path)) {
        $t = ($line -split '[#;]', 2)[0].Trim()
        if ($t -match '^\[(.+)\]$') {
            if ($null -ne $body) { $found = $body }
            $body = $null
            if ($Matches[1].Trim() -ieq $Want) { $body = New-Object System.Collections.Generic.List[string] }
            continue
        }
        if ($null -ne $body) { $body.Add($line) }
    }
    if ($null -ne $body) { return ,$body }
    return ,$found
}

$section = $null; $sourceFile = $null
foreach ($s in $sources) {
    $section = Get-Section -Path $s -Want $Id
    if ($null -ne $section -and $section.Count -gt 0) { $sourceFile = $s; break }
    $section = $null
}
if (-not $section) { throw "profile '$Id' is not in $($sources -join ' or ')" }

# ---- work out the family and the next free version --------------------------
$family = if ($As) { $As } else { ($Id -split '@')[0] }
if ($family -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw "family name '$family' should be lower case letters, digits, dot, dash or underscore" }

$masterText = Get-Content $master -Raw
$existing = [regex]::Matches($masterText, '(?m)^\s*\[\s*' + [regex]::Escape($family) + '@(\d+)\s*\]') |
    ForEach-Object { [int]$_.Groups[1].Value }
$next = if ($existing) { ($existing | Measure-Object -Maximum).Maximum + 1 } else { 1 }
$newId = "$family@$next"

if (-not $Description) {
    $Description = Read-Host "One line describing '$newId' (what it is, where it came from)"
}
if (-not $Description) { throw "a description is required - a profile without one is unusable later" }

# ---- render, keeping the file's own line endings ----------------------------
$nl = if ($masterText -match "`r`n") { "`r`n" } else { "`n" }
# `inherits` is KEPT. Dropping it would leave the imported profile carrying
# only the handful of keys the tune overrode, with everything else falling back
# to code defaults instead of the parent's values - a silently different tune,
# which is the one outcome this whole exercise exists to prevent. Only the
# description is replaced.
$keep = $section |
    Where-Object { $t = ($_ -split '[#;]', 2)[0].Trim(); $t -and ($t -notmatch '^\s*description\s*=') } |
    ForEach-Object { $_.TrimEnd() }

$inherits = $section |
    ForEach-Object { ($_ -split '[#;]', 2)[0].Trim() } |
    Where-Object { $_ -match '^\s*inherits\s*=\s*(.+)$' } |
    ForEach-Object { $Matches[1].Trim() } | Select-Object -First 1
if ($inherits -and $masterText -notmatch ('(?m)^\s*\[\s*' + [regex]::Escape($inherits) + '\s*\]')) {
    throw "'$Id' inherits from '$inherits', which is not in profiles\force-profiles.ini. " +
          "Import the parent first, or flatten the tune before importing it."
}

$block = @(
    "",
    "# ---------------------------------------------------------------------------",
    "# $newId - imported $(Get-Date -Format 'yyyy-MM-dd') from $(Split-Path $sourceFile -Leaf)",
    "# ---------------------------------------------------------------------------",
    "# Driven, then promoted. Published versions are immutable: to change this,",
    "# add $family@$($next + 1) rather than editing here.",
    $(if ($inherits) { "# Inherited from $inherits, so the lines below ARE the change." } else { "#" }),
    "#",
    "[$newId]",
    "description = $Description"
) + $keep

$rendered = ($block -join $nl)

if ($WhatIfPreference) {
    Write-Host "Would append to $master :" -ForegroundColor Cyan
    Write-Host $rendered
    exit 0
}

if ($PSCmdlet.ShouldProcess($master, "append profile $newId")) {
    $out = $masterText.TrimEnd("`r", "`n") + $nl + $rendered + $nl
    [IO.File]::WriteAllText($master, $out)
    Write-Host "Appended $newId to profiles\force-profiles.ini" -ForegroundColor Green
    Write-Host "  taken from $sourceFile (as '$Id')" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "Next, because a new profile changes the pinned model output:" -ForegroundColor Yellow
    Write-Host "  native\forcemodel\build_conformance.bat --update" -ForegroundColor Yellow
    Write-Host "  dotnet test Dbce.Wheel.sln" -ForegroundColor Yellow
    Write-Host "  then commit both, and the profile reaches consumers on the next release." -ForegroundColor Yellow
}
