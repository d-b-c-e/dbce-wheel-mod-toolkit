#Requires -Version 7.0
<# Supervises exactly one already-launched process. A matching, fresh plugin
   heartbeat must confirm process-latched output muting before forced termination.
   No input, resolution, window, monitor-profile or other-process changes. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$OwnedProcessId,
    [Parameter(Mandatory)][long]$StartedUtcTicks,
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{32}$')][string]$RequestId,
    [Parameter(Mandatory)][string]$Heartbeat,
    [Parameter(Mandatory)][string]$Log,
    [Parameter(Mandatory)][string]$StopFile,
    [ValidateRange(2,600)][int]$StallSeconds = 30,
    [ValidateRange(5,600)][int]$StartupSeconds = 180,
    [ValidateRange(10,3600)][int]$MaximumSeconds = 720,
    [ValidateRange(100,5000)][int]$PollMilliseconds = 1000,
    [switch]$SkipDisplayEvents
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage-WatchdogPolicy.ps1')
$clock = [Diagnostics.Stopwatch]::StartNew()
$lastFrame = -1L; $lastMove = 0.0; $muteConfirmed = $false; $nextEventCheck = 0.0
$owned = $null
function Write-Log([string]$Text) { Add-Content -LiteralPath $Log -Value "$([DateTime]::UtcNow.ToString('O')) $Text" -Encoding utf8 }
function Trip([string]$Reason) {
    Write-Log "TRIP: $Reason"
    if ($owned.HasExited) { Write-Log 'owned process already exited'; return }
    if (!$muteConfirmed) { Write-Log 'No forced termination: output muting was not confirmed for this request/process.'; return }
    # The retained Process/SafeHandle identifies the original process even if a
    # later process reuses its numeric PID. Never enumerate/kill by process name.
    if (!(Test-StageProcessIdentity $owned.StartTime.ToUniversalTime().Ticks $owned.MainModule.FileName $StartedUtcTicks $Executable)) {
        throw 'Process identity changed; forced termination refused'
    }
    $owned.Kill()
    $null = $owned.WaitForExit(5000)
    Write-Log 'Terminated only the identified process after confirmed output muting; display untouched.'
}
try {
    [IO.Directory]::CreateDirectory((Split-Path ([IO.Path]::GetFullPath($Log)) -Parent)) | Out-Null
    try { $owned = [Diagnostics.Process]::GetProcessById($OwnedProcessId); $null = $owned.SafeHandle }
    catch [ArgumentException] { Write-Log 'process exited before supervisor startup'; exit 0 }
    $started = $owned.StartTime.ToUniversalTime()
    if (!(Test-StageProcessIdentity $started.Ticks $owned.MainModule.FileName $StartedUtcTicks $Executable)) {
        throw 'Wrong process image or start time; supervision refused'
    }
    Write-Log "Watching PID $OwnedProcessId start $($started.ToString('O')) request $RequestId"
    while ($clock.Elapsed.TotalSeconds -lt $MaximumSeconds) {
        if ((Test-Path -LiteralPath $StopFile) -or $owned.HasExited) { Write-Log 'watch stopped or owned process closed'; exit 0 }
        $now = $clock.Elapsed.TotalSeconds
        $content = ''
        try { $content = [IO.File]::ReadAllText($Heartbeat) } catch [IO.IOException] { }
        $sample = Read-StageHeartbeat $content $RequestId $started ([DateTime]::UtcNow)
        if ($sample) {
            if ($muteConfirmed -and !$sample.outputsMuted) { $muteConfirmed = $false; Trip 'output muting revoked'; exit 1 }
            if ($sample.outputsMuted -and !$muteConfirmed) { $muteConfirmed = $true; Write-Log 'Output mute confirmed by current request heartbeat' }
            if ($lastFrame -ge 0 -and $sample.frame -lt $lastFrame) { Trip 'heartbeat frame counter moved backwards'; exit 1 }
            if ($sample.frame -ne $lastFrame) { $lastFrame = $sample.frame; $lastMove = $now }
        }
        if (!$SkipDisplayEvents -and $now -ge $nextEventCheck) {
            $nextEventCheck = $now + 2
            # Display 4101 records a recovered driver timeout. NVIDIA critical/
            # error events are also grounds to stop this diagnostic; informational
            # and warning events alone do not trigger a process/profile change.
            $events = @(Get-WinEvent -FilterHashtable @{LogName='System';ProviderName='Display';Id=4101;StartTime=$started} -ErrorAction SilentlyContinue)
            $events += @(Get-WinEvent -FilterHashtable @{LogName='System';ProviderName='nvlddmkm';Level=1,2;StartTime=$started} -ErrorAction SilentlyContinue)
            if ($events.Count) { Trip "display driver event $($events[0].ProviderName)/$($events[0].Id)"; exit 1 }
        }
        $limit = if ($lastFrame -lt 0) { $StartupSeconds } else { $StallSeconds }
        if ($now - $lastMove -gt $limit) { Trip "no current frames for $limit seconds; last frame $lastFrame"; exit 1 }
        Start-Sleep -Milliseconds $PollMilliseconds
    }
    Write-Log 'supervision deadline reached; no process or display action'
    exit 2
} catch {
    Write-Log ('ERROR: ' + $_.Exception.Message)
    exit 2
} finally {
    if ($owned) { $owned.Dispose() }
}
