#Requires -Version 7.0
# Offline: the recorder's fixture (native/dinput-proxy/test_fence.cpp writes exactly it) becomes the expected injection
# lines; a hand-written macro with names, relative times and ms= works; malformed macros exit nonzero. No game.
$ErrorActionPreference = 'Stop'
$play = Join-Path $PSScriptRoot 'Play-Macro.ps1'
$fx = Join-Path $PSScriptRoot 'fixtures'
$checks = 0
function Check([bool]$ok, [string]$name) { if (!$ok) { throw "FAIL: $name" }; $script:checks++ }
function Lines($out) { @($out | ForEach-Object { "$_" }) }

$dev = '{000000a1-0000-0000-0000-000000000000}'
$got = Lines (& $play -Macro (Join-Path $fx 'recorded.macro') -Game test -DryRun)
$want = @(
    "0.500 inject raw button 31 dev=$dev value=1 ms=300",
    "1.000 inject raw hat 0 18000 dev=$dev value=18000 ms=300",
    "1.500 inject raw axis 0 dev=$dev value=16384 ms=500 range=-32768..32767",
    "2.000 inject raw axis 0 dev=$dev value=0 ms=300 range=-32768..32767")
Check (($got -join "`n") -eq ($want -join "`n")) "recorded fixture -> injection lines:`n$($got -join "`n")"

$hand = Join-Path ([IO.Path]::GetTempPath()) "dbce-macro-test-$PID.macro"
try {
    Set-Content -LiteralPath $hand -Value @('dev wheel {00000000-0000-0000-0000-0000000000ff}  # a role name', '2 wheel button 3 1 ms=150', '+1.5 wheel hat 1 9000 ms=200')
    $got = Lines (& $play -Macro $hand -Game test -DryRun -Device 'wheel={000000b2-0000-0000-0000-000000000000}')
    Check ($got[0] -eq '2.000 inject raw button 3 dev={000000b2-0000-0000-0000-000000000000} value=1 ms=150' -and
           $got[1] -eq '3.500 inject raw hat 1 9000 dev={000000b2-0000-0000-0000-000000000000} value=9000 ms=200') "names, -Device, relative time, ms=: $($got -join ' / ')"
    foreach ($bad in 'button 31 6', '1 wheel button 31 2', '1 wheel hat 0 36000', '1 nobody button 1 1', '0.2 wheel button 1 1', '1 wheel axis 0 70000', '1 wheel button 1 1 ms=20', 'dev wheel notaguid') {
        Set-Content -LiteralPath $hand -Value @('dev wheel {00000000-0000-0000-0000-0000000000ff}', '0.5 wheel button 1 1', $bad)
        & pwsh -NoProfile -File $play -Macro $hand -Game test -DryRun 2>&1 | Out-Null
        Check ($LASTEXITCODE -ne 0) "malformed macro exits nonzero: $bad"
    }
    # Unbounded times and an unsafe -Game are refused before anything runs, dry run or not.
    foreach ($bad in 'NaN wheel button 1 1', 'Infinity wheel button 1 1', '1e300 wheel button 1 1', '+3000 wheel button 1 1') {
        Set-Content -LiteralPath $hand -Value @('dev wheel {00000000-0000-0000-0000-0000000000ff}', '+1000 wheel button 1 1', $bad)
        foreach ($mode in @('-DryRun'), @('-ProcessId', $PID)) {
            $o = & pwsh -NoProfile -File $play -Macro $hand -Game test @mode 2>&1
            Check ($LASTEXITCODE -ne 0 -and "$o" -match 'bad time|time past') "unbounded time refused ($mode): $bad"
        }
    }
    foreach ($g in '..\x', 'a/b', 'a b', '') {
        & pwsh -NoProfile -File $play -Macro (Join-Path $fx 'recorded.macro') -Game $g -DryRun 2>&1 | Out-Null
        Check ($LASTEXITCODE -ne 0) "-Game must be a plain folder token: '$g'"
    }

    # Real playback into a temporary LOCALAPPDATA, with a sleeping pwsh standing in for the game (no game, no device).
    $saved = $env:LOCALAPPDATA
    $env:LOCALAPPDATA = Join-Path ([IO.Path]::GetTempPath()) "dbce-macro-local-$PID"
    $flags = New-Item -ItemType Directory -Force (Join-Path $env:LOCALAPPDATA 'dbce\macrotest')
    'ffb-fence.on', 'inject.on' | ForEach-Object { New-Item -ItemType File -Force (Join-Path $flags $_) | Out-Null }
    $injectFile = Join-Path $flags 'inject.txt'
    $g = '{000000b2-0000-0000-0000-000000000000}'
    try {
        Set-Content -LiteralPath $hand -Value @("dev g $g", '0.1 g button 3 1 ms=600')
        & pwsh -NoProfile -File $play -Macro $hand -Game macrotest 2>&1 | Out-Null
        Check ($LASTEXITCODE -ne 0 -and !(Test-Path $injectFile)) 'no -ProcessId or -Launch: refused, nothing sent'

        $game = Start-Process pwsh -ArgumentList '-NoProfile', '-Command', 'Start-Sleep 30' -PassThru -WindowStyle Hidden
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $json = (& pwsh -NoProfile -File $play -Macro $hand -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -eq 0 -and $json.macroDispatched -and $json.stepsSent -eq 1 -and $null -eq $json.stoppedReason -and $null -eq $json.gameplayReady) "complete run: JSON result $($json | ConvertTo-Json -Compress)"
        Check ($timer.ElapsedMilliseconds -ge 700) "complete run waits for the last hold to finish ($($timer.ElapsedMilliseconds) ms)"
        Check ((Get-Content $injectFile) -eq "inject raw button 3 dev=$g value=1 ms=600") 'complete run: the press was sent, no release needed'
        Stop-Process -Id $game.Id

        # The game exits during a long hold: playback stops, the hold gets its release, nothing later is sent.
        Set-Content -LiteralPath $hand -Value @("dev g $g", '0.1 g button 3 1 ms=8000', '6 g button 4 1')
        $game = Start-Process pwsh -ArgumentList '-NoProfile', '-Command', 'Start-Sleep 2' -PassThru -WindowStyle Hidden
        $json = (& pwsh -NoProfile -File $play -Macro $hand -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -ne 0 -and !$json.macroDispatched -and $json.stepsSent -eq 1 -and $json.stoppedReason -match 'Absent') "lost game: stopped ($($json | ConvertTo-Json -Compress))"
        Check ((Get-Content $injectFile) -eq "inject raw button 3 dev=$g value=0 ms=50") "lost game: the running hold was released: $(Get-Content $injectFile)"
    } finally {
        if ($game -and !$game.HasExited) { Stop-Process -Id $game.Id }
        Remove-Item -Recurse -Force -LiteralPath $env:LOCALAPPDATA -ErrorAction SilentlyContinue
        $env:LOCALAPPDATA = $saved
    }
} finally { Remove-Item -LiteralPath $hand -ErrorAction SilentlyContinue }
"PASS $checks macro checks"
