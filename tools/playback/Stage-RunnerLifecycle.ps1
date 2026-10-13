#Requires -Version 7.0
# Runner-side filesystem/process observation and bounded cleanup. No game input,
# process termination, device access or display changes. Adapters own state paths.
function Resolve-StageFileSystemPath {
    param([Parameter(Mandatory)][string]$Path)
    $provider = $null; $drive = $null
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path, [ref]$provider, [ref]$drive)
    if ($provider.Name -ne 'FileSystem') { throw 'Stage paths must use the FileSystem provider.' }
    return $full
}

function Get-StageProcessSnapshot {
    param([Parameter(Mandatory)][string]$Name)
    foreach ($process in @(Get-Process -Name $Name -ErrorAction SilentlyContinue)) {
        $identity = $null
        try {
            if ($process.HasExited) { continue }
            $identity = [ordered]@{ state='Running'; processId=$process.Id;
                startedUtcTicks=$process.StartTime.ToUniversalTime().Ticks; executable=$process.MainModule.FileName; error='' }
            # A process can disappear between enumeration and property access.
            # Its partial/default properties must not be called a replacement.
            if ($process.HasExited) { continue }
            if (!$identity.executable -or $identity.startedUtcTicks -le 0) { throw 'Incomplete process identity.' }
            [pscustomobject]$identity
        } catch {
            $problem = $_.Exception.Message
            try { if ($process.HasExited) { continue } } catch { }
            [pscustomobject]@{ state='Unverifiable'; processId=$process.Id;
                startedUtcTicks=$null; executable=$null; error=$problem }
        } finally { $process.Dispose() }
    }
}

function Get-StageProcessDisposition {
    param([AllowEmptyCollection()][object[]]$Snapshot, [int]$OwnedId,
        [long]$StartedUtcTicks, [string]$Executable)
    if (@($Snapshot).Count -eq 0) { return 'Closed' }
    if (@($Snapshot | Where-Object state -NE 'Running').Count) { return 'Unverifiable' }
    if (@($Snapshot).Count -ne 1 -or $Snapshot[0].processId -ne $OwnedId -or
        $Snapshot[0].startedUtcTicks -ne $StartedUtcTicks -or
        ![string]::Equals($Snapshot[0].executable, $Executable, [StringComparison]::OrdinalIgnoreCase)) { return 'Changed' }
    return 'Owned'
}

# Pure evidence construction. The caller retains the Process/Handle while alive and
# supplies its observed ExitCode; a close request or absent process is not exit 0.
function New-StageGameExitEvidence {
    [CmdletBinding()]
    param([bool]$CloseMainWindowRequested, [bool]$Exited,
        [AllowNull()][object]$ExitCode, [int]$ProcessId)
    if ($null -ne $ExitCode -and $ExitCode -isnot [int]) { throw 'Game exit code must be a numeric Int32 or null.' }
    if (!$Exited -and $null -ne $ExitCode) { throw 'A running game cannot have an observed exit code.' }
    $pass = $Exited -and $null -ne $ExitCode -and $ExitCode -eq 0
    [pscustomobject]@{ schema=1; closeMainWindowRequested=$CloseMainWindowRequested;
        exited=$Exited; exitCode=$ExitCode; shutdownPass=$pass; processId=$ProcessId }
}

function Complete-StageSession {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ProcessName,
        [Parameter(Mandatory)][scriptblock]$CollectEvidence,
        [Parameter(Mandatory)][scriptblock]$StopSupervisor,
        [Parameter(Mandatory)][scriptblock]$DisarmRequest,
        [Parameter(Mandatory)][scriptblock]$RestoreOwner,
        [Parameter(Mandatory)][scriptblock]$ReleaseLease,
        [ValidateRange(0,60)][double]$WaitSeconds = 20,
        [ValidateRange(1,1000)][int]$PollMilliseconds = 200,
        [scriptblock]$SnapshotProvider = { param($name) Get-StageProcessSnapshot -Name $name }
    )
    $errors = [Collections.Generic.List[string]]::new()
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $quiet = 0; $closed = $false; $last = @()
    # Two independent empty observations. Unavailable identity is never closure.
    do {
        try { $last = @(& $SnapshotProvider $ProcessName); if (!$last.Count) { $quiet++ } else { $quiet=0 } }
        catch { $quiet=0; $last=@([pscustomobject]@{ state='Unverifiable'; error=$_.Exception.Message }) }
        if ($quiet -ge 2) { $closed=$true; break }
        if ($timer.Elapsed.TotalSeconds -ge $WaitSeconds) { break }
        Start-Sleep -Milliseconds $PollMilliseconds
    } while ($true)
    try { & $CollectEvidence | Out-Null } catch { $errors.Add('evidence: '+$_.Exception.Message) }
    $restored=$false; $disarmed=$false; $released=$false
    if ($closed) {
        # A log/watchdog failure must not bypass restoration or strand the mutex.
        try { & $StopSupervisor | Out-Null } catch { $errors.Add('supervisor: '+$_.Exception.Message) }
        try { & $DisarmRequest | Out-Null; $disarmed=$true } catch { $errors.Add('disarm: '+$_.Exception.Message) }
        try { & $RestoreOwner | Out-Null; $restored=$true } catch { $errors.Add('restore: '+$_.Exception.Message) }
        # Preserve ownership for explicit recovery if owner state/request is unsafe.
        if ($restored -and $disarmed) {
            try { & $ReleaseLease | Out-Null; $released=$true } catch { $errors.Add('lease: '+$_.Exception.Message) }
        }
    }
    [pscustomobject]@{ closed=$closed; restored=$restored; disarmed=$disarmed; leaseReleased=$released;
        elapsedSeconds=$timer.Elapsed.TotalSeconds; lastProcesses=@($last); errors=@($errors) }
}
