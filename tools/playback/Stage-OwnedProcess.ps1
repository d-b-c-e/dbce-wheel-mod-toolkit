#Requires -Version 7.0
# DB12 successor: same runner entry points and caller variables; no legacy edits.
# Dot-sourcing imports pure identity functions and initializes tracking only.
# Pin this file AND the sibling Stage-ProcessIdentity.ps1 in new runner recipes.
. (Join-Path $PSScriptRoot 'Stage-ProcessIdentity.ps1')

function Update-StageOwnedProcessLineage {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()]
        [Collections.Generic.Dictionary[uint32,DateTime]]$Owned,
        [Parameter(Mandatory)][AllowNull()][object]$Snapshot
    )
    # Work against one complete buffered census. Commit additions only after all
    # tracked/candidate identities have been checked; unknown never means absent.
    if ($null -eq $Snapshot -or $Snapshot.querySucceeded -isnot [bool] -or
        !$Snapshot.querySucceeded -or $null -eq $Snapshot.rows) {
        throw 'Complete process census unavailable for descendant adoption.'
    }
    $rows = @($Snapshot.rows)
    $byId = [Collections.Generic.Dictionary[uint32,object]]::new()
    foreach ($row in $rows) {
        $rowId = 0L; $parentId = 0L
        if ($null -eq $row -or ![long]::TryParse([string]$row.processId, [ref]$rowId) -or
            $rowId -lt 0 -or $rowId -gt [uint32]::MaxValue -or
            ![long]::TryParse([string]$row.parentProcessId, [ref]$parentId) -or
            $parentId -lt 0 -or $parentId -gt [uint32]::MaxValue) {
            throw 'Process census has an invalid PID or parent PID.'
        }
        if ($byId.ContainsKey([uint32]$rowId)) { throw 'Duplicate process census PID.' }
        $byId.Add([uint32]$rowId, $row)
    }
    $working = [Collections.Generic.Dictionary[uint32,DateTime]]::new()
    $states = [Collections.Generic.Dictionary[uint32,string]]::new()
    foreach ($entry in $Owned.GetEnumerator()) { $working.Add($entry.Key, $entry.Value) }
    foreach ($identityId in @($working.Keys)) {
        $record = [pscustomobject]@{ processId=$identityId; createdUtc=$working[$identityId]; precision='Tick100ns' }
        $disposition = Get-StageProcessIdentityDisposition -Recorded $record -Snapshot $Snapshot
        if ($disposition.state -ceq 'Unknown') { throw ('Tracked identity unknown: ' + $disposition.reason) }
        $states.Add($identityId, $disposition.state)
    }
    $adopted = [Collections.Generic.List[uint32]]::new()
    do {
        $added = $false
        foreach ($row in $rows) {
            $rowId = [uint32]$row.processId; $parentId = [uint32]$row.parentProcessId
            if ($working.ContainsKey($rowId) -or !$states.ContainsKey($parentId) -or
                $states[$parentId] -cne 'OriginalAlive') { continue }
            # OriginalAlive was established against THIS snapshot, not just a
            # remembered PID. An absent/reused/unknown parent cannot admit a child.
            $childStamp = ConvertTo-StageCreationStamp $row.createdUtc $row.precision
            $parentStamp = ConvertTo-StageCreationStamp $working[$parentId] 'Tick100ns'
            if ($null -eq $childStamp -or $null -eq $parentStamp) {
                throw 'Candidate descendant creation identity unavailable.'
            }
            $quantum = [Math]::Max($childStamp.quantum, $parentStamp.quantum)
            $childBucket = $childStamp.ticks - ($childStamp.ticks % $quantum)
            $parentBucket = $parentStamp.ticks - ($parentStamp.ticks % $quantum)
            if ($childBucket -lt $parentBucket) { continue }
            $record = [pscustomobject]@{ processId=$rowId; createdUtc=$row.createdUtc; precision=$row.precision }
            $disposition = Get-StageProcessIdentityDisposition -Recorded $record -Snapshot $Snapshot
            if ($disposition.state -cne 'OriginalAlive') { throw 'Candidate descendant identity ambiguous.' }
            $working.Add($rowId, [DateTime]::new($childStamp.ticks, [DateTimeKind]::Utc))
            $states.Add($rowId, 'OriginalAlive')
            $adopted.Add($rowId); $added = $true
        }
    } while ($added)
    foreach ($identityId in $adopted) { $Owned.Add($identityId, $working[$identityId]) }
    [pscustomobject]@{
        originalAliveProcessIds=@($states.Keys | Where-Object { $states[$_] -ceq 'OriginalAlive' })
        adoptedProcessIds=@($adopted)
    }
}

# Reuse of Livo's qualified Invoke-AffectedChecks.ps1 (1B9634278C2703693D78405C4E8EDD93A04FD6DF97086053EFF2776D1B581D2A):
# Observe-Owned, Invoke-OwnedProcess, immediate identity receipts and retained lineage.
# Narrow adaptations: caller timeout, no forced termination (Lola's bounded-wait policy),
# latch timeout/unknown identity/streams as unsafe, optional exact Steam-game observer.
# Caller supplies sourceRoot, outputRoot, result and observeExternal (or null).
function Get-StageRunnerAncestorIds {
    param([Parameter(Mandatory)]$Runner, [Parameter(Mandatory)]$Snapshot)
    $same=Get-StageProcessIdentityDisposition -Recorded $Runner -Snapshot $Snapshot
    if($same.state -cne 'OriginalAlive'){throw 'Runner identity unavailable for supplemental scope.'}
    $byId=@{}
    foreach($row in $Snapshot.rows){
        if($byId.ContainsKey([uint32]$row.processId)){throw 'Duplicate runner ancestry PID.'}
        $byId[[uint32]$row.processId]=$row
    }
    $visited=[Collections.Generic.HashSet[uint32]]::new()
    $ancestors=[Collections.Generic.List[uint32]]::new()
    $child=$byId[[uint32]$Runner.processId]
    [void]$visited.Add([uint32]$Runner.processId)
    while([uint32]$child.parentProcessId -ne 0){
        $parentId=[uint32]$child.parentProcessId
        if(!$visited.Add($parentId)){throw 'Cyclic runner ancestry.'}
        if(!$byId.ContainsKey($parentId)){break} # An exited ancestor cannot be a live supplemental match.
        $parent=$byId[$parentId]
        $before=ConvertTo-StageCreationStamp $parent.createdUtc $parent.precision
        $after=ConvertTo-StageCreationStamp $child.createdUtc $child.precision
        if($null -eq $before -or $null -eq $after){throw 'Runner ancestor creation identity unavailable.'}
        $quantum=[Math]::Max($before.quantum,$after.quantum)
        if(($before.ticks-($before.ticks%$quantum)) -gt ($after.ticks-($after.ticks%$quantum))){break} # Reused parent PID.
        $ancestors.Add($parentId)
        $child=$parent
    }
    return $ancestors.ToArray()
}

$owned = [Collections.Generic.Dictionary[uint32,DateTime]]::new()
$ownedRoots = [Collections.Generic.List[Diagnostics.Process]]::new()
$processQuerySafe = $true
$ownedRunnerIdentity=$null
function Save-Result {
    [IO.File]::WriteAllText((Join-Path $outputRoot 'result.json'), ($script:result | ConvertTo-Json -Depth 12))
}
function Observe-Owned {
    try {
        if ($observeExternal) { & $observeExternal }
        $rows = @(Get-CimInstance Win32_Process -ErrorAction Stop)
        $snapshot = [pscustomobject]@{
            querySucceeded=$true
            rows=@(foreach ($row in $rows) {
                [pscustomobject]@{
                    processId=$row.ProcessId; parentProcessId=$row.ParentProcessId
                    createdUtc=$row.CreationDate; precision='CimMicrosecond'
                }
            })
        }
        if($null -eq $ownedRunnerIdentity){
            $self=[Diagnostics.Process]::GetCurrentProcess()
            try{$script:ownedRunnerIdentity=[pscustomobject]@{processId=$PID;createdUtc=$self.StartTime.ToUniversalTime();precision='Tick100ns'}}
            finally{$self.Dispose()}
        }
        $ancestors=@(Get-StageRunnerAncestorIds -Runner $ownedRunnerIdentity -Snapshot $snapshot)
        $observation = Update-StageOwnedProcessLineage -Owned $owned -Snapshot $snapshot
    } catch { $script:processQuerySafe = $false; throw }
    @($rows | Where-Object {
        ($observation.originalAliveProcessIds -contains [uint32]$_.ProcessId) -or
        ($_.ProcessId -ne $PID -and $_.ProcessId -notin $ancestors -and $_.CommandLine -and
            ($_.CommandLine.IndexOf($sourceRoot,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
             $_.CommandLine.IndexOf($outputRoot,[StringComparison]::OrdinalIgnoreCase) -ge 0))
    })
}
function Invoke-OwnedProcess([string]$Name, [string]$Executable, [string[]]$Arguments, [int]$TimeoutSeconds) {
    $start = [Diagnostics.ProcessStartInfo]::new($Executable)
    $start.WorkingDirectory = $sourceRoot; $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.Environment['MSBUILDDISABLENODEREUSE'] = '1'
    $start.Environment['DOTNET_CLI_USE_MSBUILD_SERVER'] = '0'
    foreach ($arg in $Arguments) { $start.ArgumentList.Add($arg) }
    $child = [Diagnostics.Process]::Start($start)
    $ownedRoots.Add($child)
    try {
        $script:result.steps[-1].process = [ordered]@{pid=$child.Id; createdUtc=$null; executable=$Executable; arguments=$Arguments; timeoutSeconds=$TimeoutSeconds}
        Save-Result
        $owned[[uint32]$child.Id] = $child.StartTime.ToUniversalTime()
        $script:result.steps[-1].process = [ordered]@{pid=$child.Id; createdUtc=$owned[[uint32]$child.Id].ToString('O'); executable=$Executable; arguments=$Arguments; timeoutSeconds=$TimeoutSeconds}
        Save-Result
    } catch { $script:processQuerySafe = $false; throw }
    $stdout = $child.StandardOutput.ReadToEndAsync(); $stderr = $child.StandardError.ReadToEndAsync()
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (!$child.WaitForExit(100)) {
        [void]@(Observe-Owned)
        if ([DateTime]::UtcNow -gt $deadline) {
            $script:processQuerySafe = $false
            throw "Owned $Name timed out; no forced termination; retain lease and recorded process identities"
        }
    }
    [void]@(Observe-Owned)
    if (!$stdout.Wait(5000) -or !$stderr.Wait(5000)) {
        $script:processQuerySafe = $false
        throw "Owned $Name streams remain open; retain lease"
    }
    [IO.File]::WriteAllText((Join-Path $outputRoot ($Name+'.stdout.log')), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $outputRoot ($Name+'.stderr.log')), $stderr.GetAwaiter().GetResult())
    $script:result.steps[-1].exitCode = $child.ExitCode
    $script:result.steps[-1].state = if ($child.ExitCode -eq 0) {'pass'} else {'failed'}
    Save-Result
    if ($child.ExitCode -ne 0) { throw "Owned $Name exited $($child.ExitCode)" }
}
function Complete-Owned {
    try {
        $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(5)
        do { $remaining = @(Observe-Owned) } while ($remaining.Count -gt 0 -and [DateTime]::UtcNow -lt $cleanupDeadline)
    } catch { $script:processQuerySafe = $false; $remaining = @(); $script:result.processQueryError = $_.Exception.Message }
    $script:result.ownedLineage = @($owned.GetEnumerator() | ForEach-Object { [ordered]@{pid=$_.Key; createdUtc=$_.Value.ToString('O')} })
    $script:result.remainingScopedChildren = @($remaining | Select-Object ProcessId,ParentProcessId,CreationDate,Name,ExecutablePath,CommandLine)
    $script:result.processQuerySafe = $processQuerySafe
    $script:result.childrenQuiescent = $processQuerySafe -and $remaining.Count -eq 0
    foreach ($child in $ownedRoots) {
        try {
            if ($child.HasExited) { $child.Dispose() }
            else { $script:result.childrenQuiescent = $false; $script:result.processQueryError = 'A retained process handle still reports alive' }
        }
        catch { $script:result.childrenQuiescent = $false; $script:result.processQuerySafe = $false; $script:result.processQueryError = $_.Exception.Message }
    }
}
