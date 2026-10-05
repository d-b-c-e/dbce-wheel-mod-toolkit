# Offline integration fixtures: real git tags/files, subprocess output/exit codes,
# and byte/mtime snapshots. No consumer checkout, network or hardware is touched.
$ErrorActionPreference = 'Stop'
$sourceScript = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Get-ConsumerStatus.ps1'))
$caseRoot = Join-Path ([IO.Path]::GetTempPath()) ('consumer-status-test-' + [guid]::NewGuid().ToString('N'))
$toolkit = Join-Path $caseRoot 'toolkit'
$siblings = Join-Path $caseRoot 'consumers'
$fixtureScript = Join-Path $toolkit 'tools/Get-ConsumerStatus.ps1'
$powershell = Join-Path $PSHOME $(if (Test-Path (Join-Path $PSHOME 'pwsh.exe')) { 'pwsh.exe' } else { 'powershell.exe' })
$script:checks = 0
$names = @('art-of-sim-rally', 'iracing-arcade-wheel', 'woden-rally-edge-wheel',
    'drive-rally-wheel', 'OutRun2006Tweaks-FFB', 'milestone-wheel-tools', 'cruisn-collection')
function Assert($Condition, [string]$Message) { $script:checks++; if (-not $Condition) { throw $Message } }
function Write-Fixture([string]$Path, [string]$Content) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [IO.File]::WriteAllText($Path, $Content, [Text.UTF8Encoding]::new($false))
}
function Invoke-FixtureGit([string[]]$GitArgs) {
    & git -C $toolkit @GitArgs | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Fixture git failed: $GitArgs" }
}
function Commit-Tag([string]$Tag) {
    Invoke-FixtureGit @('add', '.')
    Invoke-FixtureGit @('-c', 'user.name=ConsumerStatusTest', '-c', 'user.email=status-test@example.invalid', 'commit', '-qm', $Tag)
    Invoke-FixtureGit @('tag', $Tag)
}
function Set-Pin([string]$Name, [string]$Pin) {
    $base = Join-Path $siblings $Name
    if ($Name -eq 'iracing-arcade-wheel') { Write-Fixture (Join-Path $base 'toolkit.version') $Pin }
    elseif ($Name -eq 'drive-rally-wheel') { Write-Fixture (Join-Path $base 'dependencies.json') (@{toolkit=@{version=$Pin}} | ConvertTo-Json -Depth 3) }
    else { Write-Fixture (Join-Path $base 'lib/toolkit/VERSION') $Pin }
}
function Set-AllPins([string]$Pin) { foreach ($name in $names) { Set-Pin $name $Pin } }
function Snapshot {
    @((Get-ChildItem -LiteralPath $caseRoot -File -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $_.FullName.Substring($caseRoot.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName).Hash + ':' + $_.LastWriteTimeUtc.Ticks
    })) -join "`n"
}
function Run-Status([string]$Current = 'v0.13.0', [switch]$Human) {
    $before = Snapshot
    $arguments = @('-NoProfile', '-File', $fixtureScript, '-Root', $siblings, '-Current', $Current)
    if (-not $Human) { $arguments += '-AsJson' } else { $arguments += '-All' }
    $output = @(& $powershell @arguments 2>&1)
    $code = $LASTEXITCODE
    Assert ((Snapshot) -ceq $before) 'Status command wrote to fixture repo/consumers'
    $text = $output -join "`n"
    if ($Human) { return [pscustomobject]@{ Code = $code; Text = $text } }
    try { $data = $text | ConvertFrom-Json } catch { throw "Status output is not JSON: $text" }
    [pscustomobject]@{ Code = $code; Data = $data; Text = $text }
}
function Row($Result, [string]$Name, [string]$Component = 'baseline') {
    $rows = @($Result.Data.Rows | Where-Object { $_.Consumer -eq $Name -and $_.Component -eq $Component })
    Assert ($rows.Count -eq 1) "Expected one $Name / $Component row"
    return $rows[0]
}
function Write-Override([string]$Baseline = 'v0.8.0', [string]$Override = 'v0.13.0') {
    $base = Join-Path $siblings 'OutRun2006Tweaks-FFB/lib/toolkit'
    Write-Fixture (Join-Path $base 'NATIVE-VERSION') $Override
    Write-Fixture (Join-Path $base 'include/wheelffb.h') 'fixture ABI header, native override'
    Write-Fixture (Join-Path $base 'native/x86/WheelFfb.dll') 'fixture binary bytes, not executable'
    $files = @('include/wheelffb.h', 'native/x86/WheelFfb.dll') | ForEach-Object {
        @{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $base $_)).Hash }
    }
    $manifest = @{ schemaVersion = 1; baselinePackageVersion = $Baseline; overridePackageVersion = $Override;
        nativeVersion = '0.6.0'; architecture = 'x86'; exportCount = 41; files = @($files) }
    Write-Fixture (Join-Path $base 'NATIVE-PROVENANCE.json') ($manifest | ConvertTo-Json -Depth 5)
}
function Remove-Override {
    $base = Join-Path $siblings 'OutRun2006Tweaks-FFB/lib/toolkit'
    foreach ($file in @('NATIVE-VERSION', 'NATIVE-PROVENANCE.json')) {
        $path = Join-Path $base $file
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
}
try {
    Write-Fixture $fixtureScript (Get-Content -LiteralPath $sourceScript -Raw)
    foreach ($file in @('native/wheelffb/wheelffb.cpp', 'native/wheelffb/include/wheelffb.h',
        'native/forcemodel/force_model.h', 'native/forcemodel/force_profile.h', 'native/forcemodel/impact_mixer.h',
        'native/telemetry/signal_sample.h', 'native/forza/forza_packet.h', 'native/dinput-proxy/dinput_proxy.h',
        'dotnet/Dbce.Wheel.Ffb/Force.cs', 'dotnet/Dbce.Wheel.Telemetry/Frame.cs', 'profiles/force-profiles.ini',
        'tools/powershell/DbceWheel.psm1')) { Write-Fixture (Join-Path $toolkit $file) 'baseline' }
    Invoke-FixtureGit @('init', '-q')
    Invoke-FixtureGit @('config', 'core.autocrlf', 'false')
    Commit-Tag 'v0.8.0'
    Set-AllPins 'v0.8.0'
    $result = Run-Status 'v0.8.0'
    Assert ($result.Code -eq 0 -and $result.Data.Rows.Count -eq 7) 'Fresh register should find all seven current consumers'

    foreach ($file in @('native/wheelffb/wheelffb.cpp', 'native/wheelffb/include/wheelffb.h', 'dotnet/Dbce.Wheel.Ffb/Force.cs')) {
        Write-Fixture (Join-Path $toolkit $file) 'native and managed FFB change'
    }
    Commit-Tag 'v0.12.0'
    Write-Fixture (Join-Path $toolkit 'docs/note.md') 'documentation only'
    Commit-Tag 'v0.13.0'
    $result = Run-Status
    Assert ($result.Code -eq 1) 'Affected components must produce exit 1'
    Assert ((Row $result 'drive-rally-wheel').Status -eq 'behind, unaffected') 'Drive must not be affected by Ffb-only managed changes'
    Assert ((Row $result 'woden-rally-edge-wheel').ChangedParts -contains 'native') 'Woden native source not tracked'
    Assert ((Row $result 'cruisn-collection').Status -eq 'behind, unaffected') 'Cruisn must not inherit DirectInput adoption'

    Set-AllPins 'v0.13.0'
    Write-Fixture (Join-Path $siblings 'iracing-arcade-wheel/CHANGELOG.md') 'Future discussion: toolkit v9.9.9. Older toolkit v0.8.0.'
    Set-Pin 'OutRun2006Tweaks-FFB' 'v0.8.0'
    Write-Override
    $result = Run-Status
    Assert ($result.Code -eq 0) 'Native-only changes must not falsely affect a split baseline'
    Assert ((Row $result 'iracing-arcade-wheel').Pinned -eq 'v0.13.0') 'Explicit iRacing pin must outrank highest changelog mention'
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'baseline (split)').Status -eq 'behind, unaffected') 'Override source leaked into baseline comparison'
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'native/header override').Status -eq 'current') 'Matching native override should be current'
    $result = Run-Status 'v0.12.0'
    Assert ($result.Code -eq 0) 'Older comparison target must not require a downgrade'
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'native/header override').Status -eq 'ahead of target') 'Newer native pin not retained'

    Write-Fixture (Join-Path $toolkit 'native/forcemodel/force_model.h') 'model change'
    Write-Fixture (Join-Path $toolkit 'native/telemetry/signal_sample.h') 'signal contract change'
    Write-Fixture (Join-Path $toolkit 'profiles/force-profiles.ini') 'profile change'
    Write-Fixture (Join-Path $toolkit 'dotnet/Dbce.Wheel.Telemetry/Frame.cs') 'telemetry change'
    Commit-Tag 'v0.14.0'
    $result = Run-Status 'v0.14.0'
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'baseline (split)').ChangedParts -contains 'profiles') 'Model/profile baseline not compared independently'
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'native/header override').Status -eq 'behind, unaffected') 'Unchanged override must not require updating'
    Assert ((Row $result 'drive-rally-wheel').Status -eq 'ACTION') 'Drive Telemetry change missed'
    Assert ((Row $result 'cruisn-collection').ChangedParts -contains 'include') 'Cruisn header change missed'
    $human = Run-Status 'v0.14.0' -Human
    Assert ($human.Code -eq 1 -and $human.Text -match 'native/header override' -and $human.Text -notmatch 'Sync-Toolkit.ps1 -Version') 'Human output must describe split components without blanket sync'

    $overrideBase = Join-Path $siblings 'OutRun2006Tweaks-FFB/lib/toolkit'
    Write-Fixture (Join-Path $overrideBase 'native/x86/WheelFfb.dll') 'tampered'
    $result = Run-Status
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'native/header override').Why -match 'hash mismatch') 'Changed override bytes were trusted'
    Write-Override
    Remove-Item -LiteralPath (Join-Path $overrideBase 'NATIVE-VERSION')
    $result = Run-Status
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'native/header override').Act) 'Manifest without native pin silently fell back to baseline'
    Write-Override
    Write-Fixture (Join-Path $overrideBase 'NATIVE-PROVENANCE.json') '{malformed'
    $result = Run-Status
    Assert ((Row $result 'OutRun2006Tweaks-FFB' 'native/header override').Status -eq 'INVALID / MISSING PIN') 'Malformed override provenance not rejected'
    Remove-Override

    Set-AllPins 'v0.14.0'
    Write-Fixture (Join-Path $toolkit 'dotnet/Dbce.Wheel.Recording/Recorder.cs') 'unpublished, separate provenance'
    Commit-Tag 'v0.15.0'
    Write-Fixture (Join-Path $siblings 'drive-rally-wheel/vendor/recording/provenance.json') '{"version":"v9.9.9"}'
    $result = Run-Status 'v0.15.0'
    Assert ($result.Code -eq 0 -and (Row $result 'drive-rally-wheel').Pinned -eq 'v0.14.0') 'Recording must not broaden managed adoption or replace the release pin'
    foreach ($file in @('native/wheelffb/test/smoke.c', 'dotnet/Dbce.Wheel.Telemetry.Tests/Test.cs', 'native/forcemodel/test_signal_contracts.cpp',
        'tools/powershell/Test-DbceWheel.ps1', 'profiles/README.md')) {
        Write-Fixture (Join-Path $toolkit $file) 'test only'
    }
    Commit-Tag 'v0.16.0'
    Set-AllPins 'v0.15.0'
    $result = Run-Status 'v0.16.0'
    Assert ($result.Code -eq 0) 'Test-only changes should not oblige consumers to move'
    Invoke-FixtureGit @('tag', 'v0.16.1')
    Set-AllPins 'v0.16.0'
    $result = Run-Status 'v0.16.1'
    Assert ($result.Code -eq 0) 'Known empty git diff was mistaken for unknown range'

    Write-Fixture (Join-Path $toolkit 'native/telemetry/signal_sample.h') 'signal-only change'
    Commit-Tag 'v0.17.0'
    Set-AllPins 'v0.16.1'
    $result = Run-Status 'v0.17.0'
    Assert ((Row $result 'cruisn-collection').Status -eq 'ACTION' -and (Row $result 'OutRun2006Tweaks-FFB').Status -eq 'behind, unaffected') 'Signal header must affect Cruisn only'

    Set-AllPins 'v0.13.0'
    Set-Pin 'iracing-arcade-wheel' 'garbage'
    $result = Run-Status
    Assert ($result.Code -eq 1 -and (Row $result 'iracing-arcade-wheel').Status -eq 'INVALID / MISSING PIN') 'Malformed explicit pin fell back to changelog'
    Set-Pin 'iracing-arcade-wheel' ''
    $result = Run-Status
    Assert ((Row $result 'iracing-arcade-wheel').Status -eq 'INVALID / MISSING PIN' -and (Row $result 'iracing-arcade-wheel').PinSource -eq 'toolkit.version') ('Empty explicit pin fell back to changelog: ' + $result.Text)
    Assert ((Row $result 'iracing-arcade-wheel').Why -match 'empty pin') 'Empty file should be identified as an empty pin, not an I/O error'
    Remove-Item -LiteralPath (Join-Path $siblings 'iracing-arcade-wheel/toolkit.version')
    Write-Fixture (Join-Path $siblings 'iracing-arcade-wheel/CHANGELOG.md') 'toolkit v0.13.0'
    $result = Run-Status
    Assert ($result.Code -eq 0 -and (Row $result 'iracing-arcade-wheel').PinSource -match 'legacy inference') 'Legacy iRacing fallback lost or unlabeled'
    Set-Pin 'iracing-arcade-wheel' 'v0.13.0'
    Write-Fixture (Join-Path $siblings 'drive-rally-wheel/dependencies.json') '{not JSON'
    $result = Run-Status
    Assert ($result.Code -eq 1 -and (Row $result 'drive-rally-wheel').Why -match 'malformed dependencies') 'Malformed Drive pin was hidden'
    Set-Pin 'drive-rally-wheel' 'v0.13.0'
    Remove-Item -LiteralPath (Join-Path $siblings 'woden-rally-edge-wheel/lib/toolkit/VERSION')
    $result = Run-Status
    Assert ((Row $result 'woden-rally-edge-wheel').Why -match 'missing') 'Missing Woden pin not reported'
    Set-Pin 'woden-rally-edge-wheel' 'v0.13.0'
    Set-Pin 'art-of-sim-rally' ('local+' + ('a' * 40) + '.clean')
    $result = Run-Status
    Assert ($result.Code -eq 1 -and (Row $result 'art-of-sim-rally').Status -eq 'LOCAL / DEVELOPMENT PIN') 'Local pin misclassified as release'
    Set-Pin 'art-of-sim-rally' 'v0.13.0'
    $result = Run-Status 'v99.0.0'
    Assert ($result.Code -eq 1 -and -not $result.Data.TargetAvailable) 'Missing comparison target should require review'
    [ordered]@{ status = 'passed'; assertions = $script:checks; scope = 'offline pins, git ranges, split provenance, output/exit and read-only snapshots' } | ConvertTo-Json -Compress
} finally {
    $absolute = [IO.Path]::GetFullPath($caseRoot)
    $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
    if ((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'consumer-status-test-*') { throw 'Unsafe fixture cleanup path' }
    if (Test-Path -LiteralPath $absolute) { Remove-Item -LiteralPath $absolute -Recurse -Force }
}
# GitHub's PowerShell wrapper propagates LASTEXITCODE after dot-sourcing.
# The final missing-target subprocess intentionally returns 1; all assertions
# and cleanup must finish before declaring fixture success.
exit 0
