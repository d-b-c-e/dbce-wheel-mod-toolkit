#Requires -Version 7.0
# Policy plus real disposable headless-process integration. No games/devices/UI.
[CmdletBinding()]
param([string]$Results = (Join-Path ([IO.Path]::GetTempPath()) ('dbce-stage-watchdog-' + [guid]::NewGuid().ToString('N'))))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage-WatchdogPolicy.ps1')
if (Test-Path $Results) { throw 'Choose a new fixture directory' }
[IO.Directory]::CreateDirectory($Results) | Out-Null
$id = 'a' * 32; $other = 'b' * 32
$start = [DateTime]::UtcNow.AddSeconds(-2); $now = [DateTime]::UtcNow
$line = '9 7680x1440 FullScreenWindow ' + $now.ToString('O') + ' Free Roam request=' + $id + ' outputsMuted=true'
if (!(Read-StageHeartbeat $line $id $start $now).outputsMuted) { throw 'Matching mute confirmation rejected' }
if (Read-StageHeartbeat $line $other $start $now) { throw 'Wrong request accepted' }
if (Read-StageHeartbeat $line $id $now.AddSeconds(1) $now) { throw 'Pre-process heartbeat accepted' }
if (Read-StageHeartbeat $line $id $start $now.AddSeconds(6)) { throw 'Stale heartbeat accepted' }
if ((Read-StageHeartbeat ($line.Replace('true','false')) $id $start $now).outputsMuted) { throw 'Unmuted heartbeat accepted as muted' }
if (Test-StageProcessIdentity 10 'C:/one.exe' 11 'C:/one.exe') { throw 'Reused PID generation accepted' }
if (Test-StageProcessIdentity 10 'C:/one.exe' 10 'C:/two.exe') { throw 'Wrong image accepted' }
Write-Host 'PASS request, freshness, mute, image and process-generation policy'
$hostExe = (Get-Command pwsh).Source
$watch = Join-Path $PSScriptRoot 'Watch-StageProcess.ps1'
$childScript = Join-Path $Results 'child.ps1'
@'
param([string]$Heartbeat,[string]$RequestId,[string]$Muted)
$initial = if ($Muted -eq 'revoked') { 'true' } else { $Muted }
$line = '1 800x600 Windowed ' + [DateTime]::UtcNow.ToString('O') + ' fixture request=' + $RequestId + ' outputsMuted=' + $initial
[IO.File]::WriteAllText($Heartbeat,$line)
if ($Muted -eq 'revoked') {
    Start-Sleep -Milliseconds 1200
    [IO.File]::WriteAllText($Heartbeat, ('2 800x600 Windowed ' + [DateTime]::UtcNow.ToString('O') + ' fixture request=' + $RequestId + ' outputsMuted=false'))
}
Start-Sleep -Seconds 30
'@ | Set-Content $childScript -Encoding utf8
foreach ($muted in @('false','true','revoked')) {
    $heartbeat = Join-Path $Results "$muted-heartbeat.txt"
    $log = Join-Path $Results "$muted-watch.log"
    $child = Start-Process $hostExe -WindowStyle Hidden -PassThru -ArgumentList ('-NoProfile -File "' + $childScript + '" -Heartbeat "' + $heartbeat + '" -RequestId ' + $id + ' -Muted ' + $muted)
    try {
        $null = $child.SafeHandle
        $ticks = $child.StartTime.ToUniversalTime().Ticks
        & $hostExe -NoProfile -File $watch -OwnedProcessId $child.Id -StartedUtcTicks $ticks -Executable $hostExe -RequestId $id -Heartbeat $heartbeat -Log $log -StopFile (Join-Path $Results 'stop') -StallSeconds 2 -StartupSeconds 5 -MaximumSeconds 10 -PollMilliseconds 100 -SkipDisplayEvents
        if ($LASTEXITCODE -ne 1) { throw "Expected stall exit, got $LASTEXITCODE" }
        $child.Refresh()
        if ($child.HasExited -ne ($muted -eq 'true')) { throw 'Wrong forced-termination policy' }
        if ($muted -eq 'revoked' -and !(Select-String -LiteralPath $log -Pattern 'output muting revoked')) { throw 'Revocation was not observed' }
        Write-Host "PASS stalled owned process: outputsMuted=$muted; terminated=$($child.HasExited)"
    } finally {
        # Only this test's headless child; never a game or user process.
        if (!$child.HasExited) { $child.Kill(); $null = $child.WaitForExit(5000) }
        $child.Dispose()
    }
}
Write-Host "PASS stage-watchdog integration. Evidence: $Results"
