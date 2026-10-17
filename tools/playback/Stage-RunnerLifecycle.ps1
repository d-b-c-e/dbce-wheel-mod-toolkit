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

# A shared append log is not an atomic reply file. Only LF-terminated records
# are published; the final unterminated tail remains pending in the caller's
# existing bounded wait. Completed malformed JSON still fails without retry.
function Read-StageJsonLogReply {
    param([AllowEmptyString()][string]$Text,
        [Parameter(Mandatory)][string]$Marker,
        [Parameter(Mandatory)][string]$RequestId,
        [Parameter(Mandatory)][string]$StepId)
    $lastLf = $Text.LastIndexOf([char]10)
    if ($lastLf -lt 0) { return $null }
    foreach ($line in $Text.Substring(0, $lastLf).Split([char]10)) {
        $offset = $line.IndexOf($Marker, [StringComparison]::Ordinal)
        if ($offset -lt 0) { continue }
        $reply = $line.Substring($offset + $Marker.Length).Trim() | ConvertFrom-Json -ErrorAction Stop
        if ($reply.requestId -ceq $RequestId -and $reply.stepId -ceq $StepId) { return $reply }
    }
    return $null
}

# A live status publisher replaces its file. Readers must permit deletion of
# their old handle; StreamReader disposal closes that handle on every path.
function Read-StageSharedText {
    param([Parameter(Mandatory)][string]$Path, [ValidateRange(1,16777216)][int]$MaximumBytes=1048576)
    # .NET invocation errors are otherwise nonterminating under a caller's
    # Continue/SilentlyContinue preference, leaving null handles and masking
    # the original IO failure with constructor/read/disposal errors. This
    # function-local preference unwinds immediately and does not alter callers.
    $ErrorActionPreference='Stop'
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    try {
        if($stream.Length -gt $MaximumBytes){throw [IO.InvalidDataException]::new('Shared text exceeds its size limit.')}
        $reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true,1024,$true)
        try {
            $text=$reader.ReadToEnd()
            if([Text.Encoding]::UTF8.GetByteCount($text) -gt $MaximumBytes){throw [IO.InvalidDataException]::new('Shared text grew past its size limit.')}
            return $text
        } finally {$reader.Dispose()}
    } finally {$stream.Dispose()}
}

# One publisher, one owned status path. Retry only Windows sharing/lock errors
# (Windows may map a reader denying delete to access-denied/5), within BOTH the
# caller's existing phase deadline and this short per-publication retry cap.
# A persistent access denial still fails; there is no alternate replace route.
function Write-StageAtomicText {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory)][DateTime]$DeadlineUtc,
        [ValidateRange(0,1000)][int]$RetryMilliseconds=250)
    $fullPath=[IO.Path]::GetFullPath($Path)
    if((Test-Path -LiteralPath $fullPath) -and ((Get-Item -LiteralPath $fullPath -Force).Attributes -band ([IO.FileAttributes]::ReparsePoint -bor [IO.FileAttributes]::Directory))){throw 'Atomic text target must be an ordinary file.'}
    $temp=$fullPath+'.tmp.'+[guid]::NewGuid().ToString('N')
    $timer=[Diagnostics.Stopwatch]::StartNew();$attempts=0;$lastFailure=$null;$created=$false
    try {
        $bytes=[Text.UTF8Encoding]::new($false).GetBytes($Text)
        $stream=[IO.File]::Open($temp,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        $created=$true
        try {$stream.Write($bytes,0,$bytes.Length)} finally {$stream.Dispose()}
        do {
            if($attempts -gt 0 -and ([DateTime]::UtcNow -ge $DeadlineUtc.ToUniversalTime() -or $timer.ElapsedMilliseconds -ge $RetryMilliseconds)){throw $lastFailure}
            $attempts++
            try {
                # Move-overwrite can fail even with delete-sharing readers.
                # Replace is the existing-file operation; first publication
                # uses create-only Move. Neither falls back after a refusal.
                if([IO.File]::Exists($fullPath)){[IO.File]::Replace($temp,$fullPath,[NullString]::Value)}
                else {[IO.File]::Move($temp,$fullPath)}
                return [pscustomobject]@{attempts=$attempts;retries=$attempts-1;elapsedMilliseconds=$timer.ElapsedMilliseconds}
            } catch {
                $lastFailure=$_;$cause=$_.Exception.GetBaseException();$code=$cause.HResult -band 65535
                $remaining=[Math]::Min($RetryMilliseconds-$timer.ElapsedMilliseconds,($DeadlineUtc.ToUniversalTime()-[DateTime]::UtcNow).TotalMilliseconds)
                if(($cause -isnot [IO.IOException] -and $cause -isnot [UnauthorizedAccessException]) -or $code -notin @(5,32,33) -or $remaining -lt 1){throw}
                Start-Sleep -Milliseconds ([int][Math]::Min(10,[Math]::Floor($remaining)))
            }
        } while($true)
    } finally {
        # Only this invocation's unique CreateNew temporary file is owned.
        if($created -and [IO.File]::Exists($temp)){[IO.File]::Delete($temp)}
    }
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
