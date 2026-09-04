<#
.SYNOPSIS
    Reports which consumer repos are behind, and which of them a given release
    actually obliges to move.

.DESCRIPTION
    Reads each consumer's real pin off disk rather than trusting a
    hand-maintained table, because a hand-maintained table is exactly what goes
    stale. Pair it with docs/CONSUMERS.md, which carries what a script cannot
    know: why each consumer takes what it takes.

    Being behind is not the same as needing to move. A release that only
    touched the PowerShell module obliges one repo; flagging all four every
    time is how a tool teaches people to ignore it. So for each consumer this
    diffs the toolkit tree between its pinned tag and the current one, maps the
    changed paths to vendored parts, and only says ACTION when a part that
    consumer actually vendors was touched. Where the pinned tag is not
    available locally it says so and assumes the worst.

    Where the pin lives differs by repo, on purpose:
      * most write lib\toolkit\VERSION when Sync-Toolkit.ps1 runs
      * iracing-arcade-wheel gitignores lib\, so its pin is a line in its
        CHANGELOG.md and the file on disk is not tracked

    Exit 0 = nothing needs to move. Exit 1 = at least one consumer does, or a
    pin could not be read. Nothing is written and nothing is fetched.

.PARAMETER Root
    Folder holding the sibling repos. Default: the parent of this repo.

.PARAMETER Current
    Version to compare against. Default: this repo's latest git tag.

.PARAMETER All
    List every consumer, including those with nothing to do.

.EXAMPLE
    .\tools\Get-ConsumerStatus.ps1
    .\tools\Get-ConsumerStatus.ps1 -Current v0.3.0 -All
#>
[CmdletBinding()]
param(
    [string]$Root,
    [string]$Current,
    [switch]$All
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Root) { $Root = Split-Path -Parent $repo }

function Invoke-Git {
    param([string[]]$GitArgs)
    $ErrorActionPreference = 'Continue'
    $out = & git -C $repo @GitArgs 2>$null
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) { return $null }
    return $out
}

if (-not $Current) { $Current = (Invoke-Git @('describe', '--tags', '--abbrev=0') | Select-Object -First 1) }
if (-not $Current) { $Current = '(unknown)' }

# Which toolkit source paths make up each vendored part. A change anywhere else
# (docs, knowledge, the test harnesses) reaches no consumer.
$partSources = @{
    'native'     = @('native/wheelffb/')
    'native-x86' = @('native/wheelffb/')
    'include'    = @('native/wheelffb/include/', 'native/forza/', 'native/forcemodel/')
    'dotnet'     = @('dotnet/')
    'powershell' = @('tools/powershell/')
    'profiles'   = @('profiles/')
}
# The DLL is not rebuilt by a header-only or test-only change.
$notTheDll = @('native/wheelffb/include/', 'native/wheelffb/test/')

# Paths that live inside a part's directory but never reach a consumer: test
# projects, conformance harnesses and their golden files. A change to one of
# these obliged three repos to update for nothing, which is the third time this
# tool has cried wolf; what a part SHIPS is not the same as where it lives.
$notShipped = @(
    '*.Tests/',                                   # dotnet test projects
    'native/forcemodel/conformance',              # the harness and its golden file
    'native/forcemodel/build_conformance',
    'native/forza/test_',                         # packet conformance
    'native/forza/build_test',
    'native/dinput-proxy/test_',
    'native/dinput-proxy/build_test'
)
function Test-Shipped($path) {
    foreach ($n in $notShipped) {
        if ($n.EndsWith('/')) { if ($path -like "*$n*") { return $false } }
        elseif ($path -like "$n*") { return $false }
    }
    return $true
}

# Keep in step with docs/CONSUMERS.md. `Headers` narrows the `include` part to
# the headers a consumer actually compiles: that part ships four of them, and
# milestone-wheel-tools cares about exactly one. Without this it is told to
# update every time an unrelated header moves, which is how a checker teaches
# people to ignore it.
$consumers = @(
    @{ Name = 'art-of-sim-rally';      Parts = @('native', 'dotnet');      Pin = 'lib/toolkit/VERSION' }
    @{ Name = 'iracing-arcade-wheel';  Parts = @('native', 'dotnet');      Pin = 'CHANGELOG.md' }
    @{ Name = 'OutRun2006Tweaks-FFB';  Parts = @('include', 'native-x86', 'profiles'); Pin = 'lib/toolkit/VERSION'
       Headers = @('wheelffb.h', 'forza_packet.h', 'force_model.h', 'force_profile.h') }
    @{ Name = 'milestone-wheel-tools'; Parts = @('include', 'powershell'); Pin = 'lib/toolkit/VERSION'
       Headers = @('forza_packet.h') }
    @{ Name = 'cruisn-collection';     Parts = @();                        Pin = 'n/a' }
)

function Get-Pin {
    param([string]$RepoPath, [string]$PinKind)
    switch ($PinKind) {
        'lib/toolkit/VERSION' {
            $f = Join-Path $RepoPath 'lib\toolkit\VERSION'
            if (-not (Test-Path $f)) { return $null }
            return ([string](Get-Content $f -TotalCount 1)).Trim()
        }
        'CHANGELOG.md' {
            $f = Join-Path $RepoPath 'CHANGELOG.md'
            if (-not (Test-Path $f)) { return $null }
            # Highest version mentioned, not the first: a changelog names the
            # toolkit in several places and the newest entry is rarely the one
            # a first-match regex lands on.
            $best = $null
            foreach ($m in [regex]::Matches((Get-Content $f -Raw), '(?i)toolkit\s+(?:bumped\s+to\s+)?v?(\d+\.\d+\.\d+)')) {
                $v = [version]$m.Groups[1].Value
                if (-not $best -or $v -gt $best) { $best = $v }
            }
            if ($best) { return 'v' + $best.ToString() }
            return $null
        }
        default { return 'n/a' }
    }
}

function Normalise { param([string]$V) if ($V) { ($V -replace '^v', '') -replace '-.*$', '' } else { $null } }

# Paths changed between two tags, or $null when the range cannot be resolved.
$changeCache = @{}
function Get-ChangedPaths {
    param([string]$From, [string]$To)
    $key = "$From..$To"
    if ($changeCache.ContainsKey($key)) { return $changeCache[$key] }
    $result = $null
    if ((Invoke-Git @('rev-parse', '--verify', "$From^{commit}")) -and
        (Invoke-Git @('rev-parse', '--verify', "$To^{commit}"))) {
        $result = @(Invoke-Git @('diff', '--name-only', "$From..$To"))
    }
    $changeCache[$key] = $result
    return $result
}

$currentNorm = Normalise $Current
$rows = foreach ($c in $consumers) {
    $path = Join-Path $Root $c.Name
    if (-not (Test-Path $path)) {
        [pscustomobject]@{ Consumer = $c.Name; Pinned = '-'; Status = 'repo not found'; Act = $true; Why = '' }
        continue
    }
    $pin = Get-Pin -RepoPath $path -PinKind $c.Pin
    if ($c.Pin -eq 'n/a') {
        [pscustomobject]@{ Consumer = $c.Name; Pinned = 'n/a'; Status = 'not a consumer'; Act = $false; Why = '' }
        continue
    }
    if (-not $pin) {
        [pscustomobject]@{ Consumer = $c.Name; Pinned = '-'; Status = 'NO PIN FOUND'; Act = $true; Why = "expected $($c.Pin)" }
        continue
    }
    if ((Normalise $pin) -eq $currentNorm) {
        $label = if ($pin -match '-') { 'current (dev pin)' } else { 'current' }
        [pscustomobject]@{ Consumer = $c.Name; Pinned = $pin; Status = $label; Act = $false; Why = '' }
        continue
    }

    $tag = if ($pin -match '^v') { $pin } else { "v$pin" }
    $changed = Get-ChangedPaths -From $tag -To $Current
    if ($null -eq $changed) {
        [pscustomobject]@{ Consumer = $c.Name; Pinned = $pin; Status = 'BEHIND'; Act = $true
                           Why = 'cannot diff that range locally' }
        continue
    }

    $hit = [System.Collections.Generic.List[string]]::new()
    foreach ($part in $c.Parts) {
        foreach ($src in $partSources[$part]) {
            $files = $changed | Where-Object { $_ -like "$src*" -and (Test-Shipped $_) }
            if ($part -in 'native', 'native-x86') {
                $files = $files | Where-Object { $f = $_; -not ($notTheDll | Where-Object { $f -like "$_*" }) }
            }
            if ($part -eq 'include' -and $c.Headers) {
                $files = $files | Where-Object { $c.Headers -contains (Split-Path $_ -Leaf) }
            }
            if ($files) { if (-not $hit.Contains($part)) { $hit.Add($part) } }
        }
    }
    if ($hit.Count) {
        [pscustomobject]@{ Consumer = $c.Name; Pinned = $pin; Status = 'ACTION'; Act = $true
                           Why = "changed: $($hit -join ', ')" }
    } else {
        [pscustomobject]@{ Consumer = $c.Name; Pinned = $pin; Status = 'behind, unaffected'; Act = $false
                           Why = 'nothing it vendors changed' }
    }
}

Write-Host "toolkit release: $Current" -ForegroundColor Cyan
$show = if ($All) { $rows } else { $rows }
$show | Format-Table Consumer, Pinned, Status, Why -AutoSize | Out-Host

$act = @($rows | Where-Object { $_.Act })
if ($act) {
    Write-Host "Needs updating:" -ForegroundColor Yellow
    foreach ($a in $act) {
        $c = $consumers | Where-Object { $_.Name -eq $a.Consumer }
        $parts = ($c.Parts -join ',')
        Write-Host ("  {0}  ->  .\tools\Sync-Toolkit.ps1 -Version {1} -Parts {2}" -f $a.Consumer, $Current, $parts) -ForegroundColor Yellow
    }
    Write-Host "`nA native consumer must be REBUILT after syncing - it compiles the header in." -ForegroundColor DarkYellow
    exit 1
}
Write-Host "Nothing to do: every consumer is current or unaffected by $Current." -ForegroundColor Green
exit 0
