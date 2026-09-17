<#
.SYNOPSIS
    Read-only comparison of each consumer's declared toolkit components.
.DESCRIPTION
    Reads explicit pins, then compares only the source paths each consumer uses.
    OutRun's native/header override is checked separately from its older model,
    encoder and profiles baseline. Its two override files must match their local
    provenance manifest. A local/development pin is never silently treated as an
    official release. Separate unpublished Recording provenance is not a release pin.

    Nothing is written, fetched, downloaded, built or synced. Exit 0 means all
    declared components are current, newer than the target, or unaffected. Exit 1
    means a component changed, a repo/pin/provenance is missing or invalid, or the
    comparison needs manual review. This does not certify installed game adoption.
.PARAMETER Root
    Parent folder containing consumer repos. Defaults to this repo's parent.
.PARAMETER Current
    Toolkit tag to compare against. Defaults to the nearest local reachable tag.
.PARAMETER All
    Retained for compatibility. All consumers are shown so missing repos stay visible.
.PARAMETER AsJson
    Emit structured rows instead of the human-readable table; exit meanings are unchanged.
.EXAMPLE
    .\tools\Get-ConsumerStatus.ps1 -Root E:\Source -Current v0.13.0 -All
.EXAMPLE
    .\tools\Get-ConsumerStatus.ps1 -Root E:\Source -Current v0.13.0 -AsJson
#>
[CmdletBinding()]
param([string]$Root, [string]$Current, [switch]$All, [switch]$AsJson)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Root) { $Root = Split-Path -Parent $repo }

function Invoke-Git([string[]]$GitArgs) {
    $ErrorActionPreference = 'Continue'
    $output = @(& git -C $repo @GitArgs 2>$null)
    [pscustomobject]@{ Code = $LASTEXITCODE; Output = $output }
}

function Parse-Pin([string]$Value) {
    $value = $Value.Trim()
    $number = $null
    if ($value -match '^v?(\d+\.\d+\.\d+)(-[0-9A-Za-z][0-9A-Za-z.-]*)?$' -and
        [version]::TryParse($Matches[1], [ref]$number)) {
        $suffix = $Matches[2]
        return [pscustomobject]@{ Kind = $(if ($suffix) { 'development' } else { 'release' });
            Value = 'v' + $number.ToString() + $suffix; Number = $number }
    }
    if ($value -match '^local\+[a-f0-9]{40}\.(clean|dirty)$') {
        return [pscustomobject]@{ Kind = 'local'; Value = $value; Number = $null }
    }
    [pscustomobject]@{ Kind = 'invalid'; Value = $value; Number = $null }
}

function Pin-Result([string]$Value, [string]$Source, [string]$ErrorText = '') {
    $parsed = Parse-Pin $Value
    [pscustomobject]@{ Value = $parsed.Value; Kind = $parsed.Kind; Number = $parsed.Number;
        Source = $Source; Error = $ErrorText }
}

function Read-TextPin([string]$RepoPath, [string]$RelativePath) {
    $file = Join-Path $RepoPath $RelativePath
    if (-not (Test-Path -LiteralPath $file)) {
        return Pin-Result '' $RelativePath "missing $RelativePath"
    }
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        return Pin-Result '' $RelativePath "$RelativePath is not a file"
    }
    try {
        $firstLine = Get-Content -LiteralPath $file -TotalCount 1
        $value = if ($null -eq $firstLine) { '' } else { ([string]$firstLine).Trim() }
    }
    catch { return Pin-Result '' $RelativePath "cannot read $RelativePath" }
    $result = Pin-Result $value $RelativePath
    if ($result.Kind -eq 'invalid') { $result.Error = "invalid or empty pin in $RelativePath" }
    return $result
}

function Get-ConsumerPin([string]$RepoPath, [string]$PinKind) {
    if ($PinKind -eq 'toolkit.version') {
        # An invalid explicit pin is an error, never an excuse to guess from history.
        if (Test-Path -LiteralPath (Join-Path $RepoPath 'toolkit.version')) {
            return Read-TextPin $RepoPath 'toolkit.version'
        }
        $legacy = Join-Path $RepoPath 'CHANGELOG.md'
        if (Test-Path -LiteralPath $legacy -PathType Leaf) {
            $best = $null
            foreach ($match in [regex]::Matches((Get-Content -LiteralPath $legacy -Raw),
                '(?i)toolkit\s+(?:bumped\s+to\s+)?v?(\d+\.\d+\.\d+)')) {
                $number = $null
                if ([version]::TryParse($match.Groups[1].Value, [ref]$number) -and
                    (-not $best -or $number -gt $best)) { $best = $number }
            }
            if ($best) { return Pin-Result ('v' + $best.ToString()) 'CHANGELOG.md (legacy inference)' }
        }
        return Pin-Result '' 'toolkit.version' 'missing toolkit.version; no legacy changelog pin found'
    }
    if ($PinKind -eq 'dependencies.json:toolkit.version') {
        $file = Join-Path $RepoPath 'dependencies.json'
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            return Pin-Result '' $PinKind 'missing dependencies.json'
        }
        try { $data = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json }
        catch { return Pin-Result '' $PinKind 'malformed dependencies.json' }
        $pin = Pin-Result ([string]$data.toolkit.version) $PinKind
        if ($pin.Kind -eq 'invalid') { $pin.Error = 'invalid or missing dependencies.json toolkit.version' }
        return $pin
    }
    return Read-TextPin $RepoPath $PinKind
}

$partSources = @{
    'native' = @('native/wheelffb/')
    'native-x86' = @('native/wheelffb/')
    'include' = @('native/wheelffb/include/', 'native/forza/', 'native/forcemodel/', 'native/telemetry/', 'native/dinput-proxy/')
    'dotnet' = @('dotnet/Dbce.Wheel.Ffb/', 'dotnet/Dbce.Wheel.Telemetry/')
    'powershell' = @('tools/powershell/DbceWheel.psm1')
    'profiles' = @('profiles/force-profiles.ini')
}
$consumers = @(
    @{ Name = 'art-of-sim-rally'; Parts = @('native', 'dotnet'); Pin = 'lib/toolkit/VERSION' }
    @{ Name = 'iracing-arcade-wheel'; Parts = @('native', 'dotnet'); Pin = 'toolkit.version'
       Note = 'Recording has separate recording-toolkit.json source/binary provenance; not inferred from the release pin.' }
    @{ Name = 'woden-rally-edge-wheel'; Parts = @('native', 'dotnet'); Pin = 'lib/toolkit/VERSION'
       Note = 'Recording has separate lib/recording/provenance.json; not inferred from the release pin.' }
    @{ Name = 'drive-rally-wheel'; Parts = @('dotnet'); Pin = 'dependencies.json:toolkit.version'
       ManagedSources = @('dotnet/Dbce.Wheel.Telemetry/'); CustomVendor = $true
       Note = 'Telemetry only. Recording has separate vendor/recording/provenance.json.' }
    @{ Name = 'OutRun2006Tweaks-FFB'; Parts = @('include', 'native-x86', 'profiles'); Pin = 'lib/toolkit/VERSION'
       Headers = @('wheelffb.h', 'forza_packet.h', 'force_model.h', 'force_profile.h', 'dinput_proxy.h') }
    @{ Name = 'milestone-wheel-tools'; Parts = @('include', 'powershell'); Pin = 'lib/toolkit/VERSION'
       Headers = @('forza_packet.h') }
    @{ Name = 'cruisn-collection'; Parts = @('include', 'profiles'); Pin = 'lib/toolkit/VERSION'
       Headers = @('force_model.h', 'force_profile.h', 'impact_mixer.h', 'signal_sample.h')
       Note = 'Shared force/shaping headers and profiles; SDL device output is consumer-owned.' }
)

function Test-Shipped([string]$Path, [string]$Part, $Headers) {
    if ($Path -match '\.Tests/' -or $Path -match '/(?:test_|build_test|conformance|build_conformance)' -or
        $Path -match '^native/wheelffb/test/') { return $false }
    if ($Part -in @('native', 'native-x86') -and $Path -like 'native/wheelffb/include/*') { return $false }
    if ($Part -eq 'include' -and $Headers -and $Headers -notcontains (Split-Path $Path -Leaf)) { return $false }
    return $true
}

$changeCache = @{}
function Get-ChangedPaths([string]$From, [string]$To) {
    $key = "$From..$To"
    if ($changeCache.ContainsKey($key)) { return $changeCache[$key] }
    $known = (Invoke-Git @('rev-parse', '--verify', "$From^{commit}")).Code -eq 0 -and
             (Invoke-Git @('rev-parse', '--verify', "$To^{commit}")).Code -eq 0
    $paths = @()
    if ($known) {
        $diff = Invoke-Git @('diff', '--name-only', "$From..$To")
        $known = $diff.Code -eq 0
        $paths = @($diff.Output | Where-Object { $_ })
    }
    # A known empty diff is not an unknown range; don't return an unrolled array.
    $result = [pscustomobject]@{ Known = $known; Paths = $paths }
    $changeCache[$key] = $result
    return $result
}

function Get-NativeOverride([string]$RepoPath, $Baseline) {
    $pin = Read-TextPin $RepoPath 'lib/toolkit/NATIVE-VERSION'
    if ($pin.Error -or $pin.Kind -ne 'release') { return [pscustomobject]@{ Pin = $pin; Error = 'native override needs a valid release pin in NATIVE-VERSION' } }
    $file = Join-Path $RepoPath 'lib/toolkit/NATIVE-PROVENANCE.json'
    try {
        if ($Baseline.Kind -ne 'release' -or $Baseline.Error) { throw 'native override requires a valid baseline release pin' }
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw 'missing NATIVE-PROVENANCE.json' }
        $data = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
        if ($data.schemaVersion -ne 1 -or $data.architecture -ne 'x86') { throw 'unsupported native provenance schema/architecture' }
        if ((Parse-Pin ([string]$data.baselinePackageVersion)).Value -ne $Baseline.Value -or
            (Parse-Pin ([string]$data.overridePackageVersion)).Value -ne $pin.Value) { throw 'native provenance versions disagree with VERSION/NATIVE-VERSION' }
        $expected = @('include/wheelffb.h', 'native/x86/WheelFfb.dll')
        $files = @($data.files)
        if ($files.Count -ne $expected.Count) { throw 'native provenance must name exactly the header and x86 DLL' }
        foreach ($relative in $expected) {
            $entries = @($files | Where-Object { $_.path -ceq $relative })
            if ($entries.Count -ne 1 -or $entries[0].sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw "invalid provenance entry: $relative" }
            $artifact = Join-Path (Join-Path $RepoPath 'lib/toolkit') $relative
            if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) { throw "missing override file: $relative" }
            if ((Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash -ne $entries[0].sha256) { throw "override hash mismatch: $relative" }
        }
    } catch { return [pscustomobject]@{ Pin = $pin; Error = $_.Exception.Message } }
    [pscustomobject]@{ Pin = $pin; Error = '' }
}

if (-not $Current) { $Current = (Invoke-Git @('describe', '--tags', '--abbrev=0')).Output | Select-Object -First 1 }
if (-not $Current) { $Current = '(unknown)' }
$target = Parse-Pin $Current
if ($target.Kind -in @('release', 'development')) { $Current = $target.Value }
$targetAvailable = (Invoke-Git @('rev-parse', '--verify', "$Current^{commit}")).Code -eq 0

function Compare-Component($Consumer, $Component, $Pin, $Parts, $Headers, [string]$ErrorText = '') {
    $row = [ordered]@{ Consumer = $Consumer.Name; Component = $Component; Pinned = $Pin.Value;
        PinSource = $Pin.Source; Status = ''; Act = $false; Why = ''; ChangedParts = @();
        Note = [string]$Consumer.Note; Recommendation = '' }
    if ($ErrorText -or $Pin.Error -or $Pin.Kind -eq 'invalid') {
        $row.Status = 'INVALID / MISSING PIN'; $row.Act = $true
        $row.Why = if ($ErrorText) { $ErrorText } elseif ($Pin.Error) { $Pin.Error } else { 'pin is not a recognized release or local identity' }
        $row.Recommendation = 'Resolve the explicit pin/provenance before choosing an update.'
    } elseif ($Pin.Kind -eq 'local' -or $Pin.Kind -eq 'development') {
        $row.Status = 'LOCAL / DEVELOPMENT PIN'; $row.Act = $true
        $row.Why = 'Not an official release-adoption claim; review the exact local build provenance.'
        $row.Recommendation = 'Do not replace or downgrade this build based on release-version inference.'
    } elseif (-not $targetAvailable) {
        $row.Status = 'UNKNOWN TARGET'; $row.Act = $true; $row.Why = "comparison target $Current is not available locally"
    } elseif ($target.Number -and $Pin.Number -gt $target.Number) {
        $row.Status = 'ahead of target'; $row.Why = 'Newer than the comparison target; no downgrade suggested.'
    } elseif ($Pin.Value -eq $Current) {
        $row.Status = 'current'
    } else {
        $changed = Get-ChangedPaths $Pin.Value $Current
        if (-not $changed.Known) {
            $row.Status = 'UNKNOWN RANGE'; $row.Act = $true; $row.Why = 'cannot diff that range locally; nothing fetched'
        } else {
            $hits = [System.Collections.Generic.List[string]]::new()
            foreach ($part in $Parts) {
                $sources = if ($part -eq 'dotnet' -and $Consumer.ManagedSources) { $Consumer.ManagedSources } else { $partSources[$part] }
                foreach ($source in $sources) {
                    $files = @($changed.Paths | Where-Object {
                        ($_ -eq $source -or ($source.EndsWith('/') -and $_ -like "$source*")) -and (Test-Shipped $_ $part $Headers)
                    })
                    if ($files.Count -and -not $hits.Contains($part)) { $hits.Add($part) }
                }
            }
            $row.ChangedParts = @($hits)
            if ($hits.Count) {
                $row.Status = 'ACTION'; $row.Act = $true; $row.Why = 'changed: ' + ($hits -join ', ')
                $row.Recommendation = if ($Component -eq 'native/header override') {
                    'Review the x86 DLL and matching ABI header together; retain baseline files and update override provenance.'
                } elseif ($Component -eq 'baseline (split)') {
                    'Review baseline model/encoder/profile changes separately. Preserve the native/header override; no blanket sync.'
                } elseif ($Consumer.CustomVendor) {
                    'Use the consumer dependency workflow for its Telemetry DLL; preserve separate Recording provenance.'
                } else {
                    'Review the affected parts in the consumer before syncing; rebuild compiled headers and verify the packaged result.'
                }
            } else { $row.Status = 'behind, unaffected'; $row.Why = 'nothing this component uses changed' }
        }
    }
    if ($Pin.Source -like '*legacy inference*') { $row.Note = ($row.Note + ' Legacy changelog inference only; add an explicit toolkit.version.').Trim() }
    [pscustomobject]$row
}

$rows = @(foreach ($consumer in $consumers) {
    $path = Join-Path $Root $consumer.Name
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        Compare-Component $consumer 'baseline' (Pin-Result '' $consumer.Pin 'repo not found') $consumer.Parts $consumer.Headers
        continue
    }
    $pin = Get-ConsumerPin $path $consumer.Pin
    $split = $consumer.Name -eq 'OutRun2006Tweaks-FFB' -and
        ((Test-Path -LiteralPath (Join-Path $path 'lib/toolkit/NATIVE-VERSION')) -or
         (Test-Path -LiteralPath (Join-Path $path 'lib/toolkit/NATIVE-PROVENANCE.json')))
    if ($split) {
        $baselineHeaders = @($consumer.Headers | Where-Object { $_ -ne 'wheelffb.h' })
        Compare-Component $consumer 'baseline (split)' $pin @('include', 'profiles') $baselineHeaders
        $override = Get-NativeOverride $path $pin
        Compare-Component $consumer 'native/header override' $override.Pin @('native-x86', 'include') @('wheelffb.h') $override.Error
    } else { Compare-Component $consumer 'baseline' $pin $consumer.Parts $consumer.Headers }
})
$actionRequired = @($rows | Where-Object { $_.Act }).Count -gt 0
if ($AsJson) {
    [ordered]@{ Current = $Current; TargetAvailable = $targetAvailable; ActionRequired = $actionRequired; Rows = $rows } | ConvertTo-Json -Depth 8
} else {
    Write-Host "toolkit comparison: $Current (read-only; declared components, not installed-game certification)" -ForegroundColor Cyan
    $rows | Format-Table Consumer, Component, Pinned, Status, Why -AutoSize -Wrap | Out-Host
    foreach ($row in $rows) {
        if ($row.Note) { Write-Host "  $($row.Consumer): $($row.Note)" }
        if ($row.Recommendation) { Write-Host "  $($row.Consumer) / $($row.Component): $($row.Recommendation)" -ForegroundColor Yellow }
    }
    if (-not $actionRequired) { Write-Host 'Nothing to update for this comparison; newer pins are retained.' -ForegroundColor Green }
}
if ($actionRequired) { exit 1 }
exit 0
