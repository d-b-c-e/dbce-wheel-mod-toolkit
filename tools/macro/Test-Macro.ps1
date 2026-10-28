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
} finally { Remove-Item -LiteralPath $hand -ErrorAction SilentlyContinue }
"PASS $checks macro checks"
