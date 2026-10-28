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
    # Keyboard keys: dry run lists down and up in time order; without -RequireForeground the macro is refused, dry run
    # or not; bad scan codes and holds are malformed. (No real SendInput here.)
    Set-Content -LiteralPath $hand -Value @('dev wheel {00000000-0000-0000-0000-0000000000ff}', '1 key 0x1C', '1.1 wheel button 2 1 ms=100', '+1 key 28 ms=500')
    $got = Lines (& $play -Macro $hand -Game test -DryRun -RequireForeground)
    Check (($got -join '|') -eq ('1.000 key 0x1C down ms=300|1.100 inject raw button 2 dev={00000000-0000-0000-0000-0000000000ff} value=1 ms=100|' +
        '1.300 key 0x1C up|2.100 key 0x1C down ms=500|2.600 key 0x1C up')) "key steps in time order: $($got -join ' | ')"
    foreach ($mode in @('-DryRun'), @('-ProcessId', $PID)) {
        $o = & pwsh -NoProfile -File $play -Macro $hand -Game test @mode 2>&1
        Check ($LASTEXITCODE -ne 0 -and "$o" -match 'need -RequireForeground') "key without -RequireForeground refused ($mode)"
    }
    foreach ($bad in 'key 0x80', 'key 0', 'key 0x1C ms=10', 'key enter', 'key 0x1C 5') {
        Set-Content -LiteralPath $hand -Value @($bad)
        & pwsh -NoProfile -File $play -Macro $hand -Game test -DryRun -RequireForeground 2>&1 | Out-Null
        Check ($LASTEXITCODE -ne 0) "malformed key step: $bad"
    }
    foreach ($bad in 'expect ref.bmp NaN', 'expect ref.bmp Infinity', 'expect ref.bmp -1', 'expect ref.bmp 256') {
        Set-Content -LiteralPath $hand -Value @($bad)
        & pwsh -NoProfile -File $play -Macro $hand -Game test -DryRun 2>&1 | Out-Null
        Check ($LASTEXITCODE -ne 0) "non-finite or out-of-range checkpoint limit refused: $bad"
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

        # lulu's repro: the game exits during the final hold; that is not a success.
        Set-Content -LiteralPath $hand -Value @("dev g $g", '0.1 g button 3 1 ms=8000')
        $game = Start-Process pwsh -ArgumentList '-NoProfile', '-Command', 'Start-Sleep 4' -PassThru -WindowStyle Hidden
        $json = (& pwsh -NoProfile -File $play -Macro $hand -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -ne 0 -and !$json.macroDispatched -and $json.stoppedReason -match 'Absent') "exit during the final hold: not a success ($($json | ConvertTo-Json -Compress))"
        Check ((Get-Content $injectFile) -eq "inject raw button 3 dev=$g value=0 ms=50") 'exit during the final hold: released'

        # Checkpoints, with synthetic BMPs and a stand-in game that answers frame-dump.req the way the mods do.
        function New-Bmp([string]$file, [int]$w, [int]$h, [scriptblock]$rgb) {
            $row = ($w * 3 + 3) -band -4; $b = [byte[]]::new(54 + $row * $h)
            $b[0] = 0x42; $b[1] = 0x4D
            [BitConverter]::GetBytes([uint32]$b.Length).CopyTo($b, 2); [BitConverter]::GetBytes([uint32]54).CopyTo($b, 10)
            [BitConverter]::GetBytes([uint32]40).CopyTo($b, 14); [BitConverter]::GetBytes($w).CopyTo($b, 18); [BitConverter]::GetBytes($h).CopyTo($b, 22)
            $b[26] = 1; $b[28] = 24
            for ($y = 0; $y -lt $h; $y++) { for ($x = 0; $x -lt $w; $x++) {
                $c = & $rgb $x ($h - 1 - $y); $q = 54 + $y * $row + $x * 3
                $b[$q] = $c[2]; $b[$q + 1] = $c[1]; $b[$q + 2] = $c[0]
            } }
            [IO.File]::WriteAllBytes($file, $b)
        }
        $work = New-Item -ItemType Directory -Force (Join-Path $env:LOCALAPPDATA 'work')
        $src = Join-Path $work 'game-frame.bmp'
        New-Bmp $src 160 30 { param($x, $y) @([byte]($x * 255 / 160), [byte]($y * 8), 40) }   # the menu
        $answer = "`$end = (Get-Date).AddSeconds(60); while ((Get-Date) -lt `$end) { if (Test-Path '$flags\frame-dump.req') { Remove-Item '$flags\frame-dump.req'; " +
                  "[IO.File]::WriteAllBytes((Join-Path '$flags' ('frame-' + (Get-Date -Format HHmmss) + '-plain.bmp')), [IO.File]::ReadAllBytes('$src')) }; Start-Sleep -Milliseconds 100 }"
        $game = Start-Process pwsh -ArgumentList '-NoProfile', '-Command', $answer -PassThru -WindowStyle Hidden
        $cm = Join-Path $work 'route.macro'
        Set-Content -LiteralPath $cm -Value @("dev g $g", '0.1 g button 3 1 ms=200', '+0.5 expect menu.bmp', '+0.2 g button 4 1 ms=200')
        $saved1 = (& pwsh -NoProfile -File $play -Macro $cm -Game macrotest -RecordCheckpoint menu 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -eq 0 -and $saved1.checkpointSaved -eq (Join-Path $work 'menu.bmp') -and
               [Linq.Enumerable]::SequenceEqual([IO.File]::ReadAllBytes($src), [IO.File]::ReadAllBytes($saved1.checkpointSaved))) 'record-checkpoint saves the frame beside the macro'
        & pwsh -NoProfile -File $play -Macro $cm -Game macrotest -RecordCheckpoint menu 2>&1 | Out-Null
        Check ($LASTEXITCODE -ne 0) 'record-checkpoint never overwrites a reference'
        New-Bmp $src 160 30 { param($x, $y) @([byte]([Math]::Min(255, $x * 255 / 160 + 6)), [byte]($y * 8 + 6), 46) }   # the same menu, slightly brighter
        $json = (& pwsh -NoProfile -File $play -Macro $cm -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -eq 0 -and $json.macroDispatched -and $json.checkpointsPassed -eq 1 -and $json.stepsSent -eq 2) "checkpoint within maxDiff passes ($($json | ConvertTo-Json -Compress))"
        New-Bmp $src 160 30 { param($x, $y) @([byte](255 - $x * 255 / 160), 200, [byte]($y * 8)) }   # another screen
        Remove-Item -LiteralPath $injectFile
        $json = (& pwsh -NoProfile -File $play -Macro $cm -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -ne 0 -and !$json.macroDispatched -and $json.stoppedReason -match '^checkpoint menu mismatch \d+\.\d$' -and $json.stepsSent -eq 1) "checkpoint mismatch stops ($($json | ConvertTo-Json -Compress))"
        Check ((Get-Content $injectFile) -eq "inject raw button 3 dev=$g value=1 ms=200") 'checkpoint mismatch: nothing after it was sent'
        Check (!(Test-Path "$flags\frame-dump.req")) 'no frame request is left behind'
        Check ($json.checkpoints.Count -eq 1 -and $json.checkpoints[0].name -eq 'menu' -and $json.checkpoints[0].diff -gt 20 -and !$json.checkpoints[0].passed) 'a failed checkpoint reports its diff'

        # A region: a grey menu panel over an animated background. The highlight is on item 2 of 4 (rows 30-40, 50-60, ...).
        $menu = { param($item, $phase) { param($x, $y) $hl = $x -ge 20 -and $x -lt 60 -and $y -ge (20 + 20 * $item) -and $y -lt (30 + 20 * $item)
            if ($hl) { @(250, 250, 250) } elseif ($x -ge 20 -and $x -lt 60 -and $y -ge 20 -and $y -lt 100) { @(90, 90, 90) }
            else { @([byte](($x * 3 + $phase) % 256), [byte](($y * 5 + $phase) % 256), [byte](($phase * 2) % 256)) } }.GetNewClosure() }
        New-Bmp (Join-Path $work 'item2.bmp') 240 120 (& $menu 1 0)
        $rm = Join-Path $work 'region.macro'
        Set-Content -LiteralPath $rm -Value @("dev g $g", '0.1 g button 3 1 ms=100', '+0.3 expect item2.bmp 20 region=0.09,0.17,0.24,0.83')
        Check ((Lines (& $play -Macro $rm -Game macrotest -DryRun))[-1] -match 'expect .*item2\.bmp 20 region=0\.09,0\.17,0\.24,0\.83$') 'dry run shows the region'
        New-Bmp $src 240 120 (& $menu 1 120)   # same highlight, the background moved on
        $json = (& pwsh -NoProfile -File $play -Macro $rm -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -eq 0 -and $json.checkpoints[0].passed -and $json.checkpoints[0].diff -le 20) "region: a change outside the crop is ignored ($($json.checkpoints | ConvertTo-Json -Compress))"
        Set-Content -LiteralPath $rm -Value @("dev g $g", '0.1 g button 3 1 ms=100', '+0.3 expect item2.bmp 20')
        $json = (& pwsh -NoProfile -File $play -Macro $rm -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($LASTEXITCODE -ne 0 -and $json.checkpoints[0].diff -gt 20) "the same frame fails as a whole frame ($($json.checkpoints[0].diff))"
        New-Bmp $src 240 120 (& $menu 2 0)     # same background, the highlight on the next item
        $json = (& pwsh -NoProfile -File $play -Macro $rm -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        $whole = $json.checkpoints[0].diff
        Set-Content -LiteralPath $rm -Value @("dev g $g", '0.1 g button 3 1 ms=100', '+0.3 expect item2.bmp 20 region=0.09,0.17,0.24,0.83')
        $json = (& pwsh -NoProfile -File $play -Macro $rm -Game macrotest -ProcessId $game.Id 2>$null | Select-Object -Last 1) | ConvertFrom-Json
        Check ($whole -le 20 -and $LASTEXITCODE -ne 0 -and $json.stoppedReason -match '^checkpoint item2 mismatch' -and $json.checkpoints[0].diff -gt 20) "region: a moved highlight the whole frame misses ($whole) is caught ($($json.checkpoints[0].diff))"
        foreach ($bad in 'region=0,0,1', 'region=0.5,0,0.4,1', 'region=0,0,1,1.5', 'region=a,b,c,d') {
            Set-Content -LiteralPath $rm -Value @("expect item2.bmp $bad")
            & pwsh -NoProfile -File $play -Macro $rm -Game macrotest -DryRun 2>&1 | Out-Null
            Check ($LASTEXITCODE -ne 0) "bad region refused: $bad"
        }
    } finally {
        if ($game -and !$game.HasExited) { Stop-Process -Id $game.Id }
        Remove-Item -Recurse -Force -LiteralPath $env:LOCALAPPDATA -ErrorAction SilentlyContinue
        $env:LOCALAPPDATA = $saved
    }
} finally { Remove-Item -LiteralPath $hand -ErrorAction SilentlyContinue }
"PASS $checks macro checks"
