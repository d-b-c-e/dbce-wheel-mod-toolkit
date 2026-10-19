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
        [Parameter(Mandatory)][AllowNull()][object]$Snapshot,
        # Producer precision per tracked PID; a PID without an entry is a Process.StartTime root (Tick100ns).
        # Adopted descendants are recorded as CimMicrosecond, so later live-handle checks compare correctly.
        [Collections.Generic.Dictionary[uint32,string]]$Precision
    )
    # Work against one complete buffered census. Commit additions only after all
    # tracked/candidate identities have been checked; unknown never means absent.
    $precisionOf = { param([uint32]$Id) $p=$null; if ($null -ne $Precision -and $Precision.TryGetValue($Id, [ref]$p)) { $p } else { 'Tick100ns' } }
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
        $record = [pscustomobject]@{ processId=$identityId; createdUtc=$working[$identityId]; precision=(& $precisionOf $identityId) }
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
            $parentStamp = ConvertTo-StageCreationStamp $working[$parentId] (& $precisionOf $parentId)
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
    foreach ($identityId in $adopted) {
        $Owned.Add($identityId, $working[$identityId])
        if ($null -ne $Precision) { $Precision[$identityId] = [string]$byId[$identityId].precision }
    }
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

# Dot-sourcing shares the caller's script scope. Keep persistent tracking data
# behind a helper-specific name so ordinary caller variables (notably $owned)
# cannot replace dictionaries or shadow reads through PowerShell's dynamic scope.
$script:StageOwnedProcessState = [pscustomobject]@{
    Identities = [Collections.Generic.Dictionary[uint32,DateTime]]::new()
    Precision = [Collections.Generic.Dictionary[uint32,string]]::new()
    Roots = [Collections.Generic.List[Diagnostics.Process]]::new()
    # Synthetic fixtures replace this seam; production opens only a verified handle.
    OpenProcess = { param([uint32]$Id) [Diagnostics.Process]::GetProcessById([int]$Id) }
    QuerySafe = $true
    RunnerIdentity = $null
    ObservationStarted = $false
    PersistentConfigured = $false
    PersistentServices = @()
    AdmittedPersistentServices = @()
    PersistentClassificationSafe = $true
}

function ConvertTo-StageOwnedPersistentServices {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object]$EnteredUtc,
          [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Services)
    $entry=ConvertTo-StageCreationStamp $EnteredUtc 'Tick100ns'
    if($null -eq $entry -or $Services.Count -gt 8){throw 'A valid entry timestamp and at most eight persistent services are required.'}
    $seen=[Collections.Generic.HashSet[uint32]]::new()
    $accepted=@(foreach($service in $Services){
        $identityId=0L
        if($null -eq $service -or ![long]::TryParse([string]$service.processId,[ref]$identityId) -or
           $identityId -le 0 -or $identityId -gt [uint32]::MaxValue -or !$seen.Add([uint32]$identityId)){
            throw 'Persistent service PID is invalid or duplicated.'
        }
        $created=ConvertTo-StageCreationStamp $service.createdUtc $service.precision
        $admitted=ConvertTo-StageCreationStamp $service.admittedAtUtc 'Tick100ns'
        if($null -eq $created -or $null -eq $admitted){throw 'Persistent service creation, precision and admission timestamp are required.'}
        $quantum=[Math]::Max($created.quantum,$entry.quantum)
        if(($created.ticks-($created.ticks%$quantum)) -ge ($entry.ticks-($entry.ticks%$quantum)) -or
           $admitted.ticks -ge $entry.ticks -or $admitted.ticks -lt $created.ticks){
            throw 'Persistent service creation and explicit admission must precede entry without an ambiguous precision bucket.'
        }
        foreach($field in @('coordinator','role')){
            if($service.$field -isnot [string] -or !($service.$field).Trim() -or ($service.$field).Length -gt 128 -or
               $service.$field -match '[\r\n]'){throw 'Persistent service coordinator and role are required bounded names.'}
        }
        $receipt=$service.custodyReceipt
        if($null -eq $receipt -or $receipt.path -isnot [string] -or $receipt.path.Length -gt 4096 -or
           ![IO.Path]::IsPathFullyQualified($receipt.path) -or $receipt.sha256 -isnot [string] -or
           $receipt.sha256 -cnotmatch '^[a-fA-F0-9]{64}$'){throw 'A pinned absolute custody receipt path and SHA256 are required.'}
        [pscustomobject]@{processId=[uint32]$identityId;createdUtc=[DateTime]::new($created.ticks,[DateTimeKind]::Utc).ToString('O');
            precision=[string]$service.precision;admittedAtUtc=[DateTime]::new($admitted.ticks,[DateTimeKind]::Utc).ToString('O');
            enteredUtc=[DateTime]::new($entry.ticks,[DateTimeKind]::Utc).ToString('O');coordinator=$service.coordinator;role=$service.role;
            custodyReceipt=[pscustomobject]@{path=$receipt.path;sha256=$receipt.sha256.ToUpperInvariant()}}
    })
    $accepted
}
function Set-StageOwnedPersistentServices {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object]$EnteredUtc,
          [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Services)
    # Explicit preflight only. The caller verifies custody receipt contents/hash and coordinator authorization;
    # this helper copies the pinned reference as evidence, never discovers or infers custody from a name/path.
    if($script:StageOwnedProcessState.ObservationStarted -or $script:StageOwnedProcessState.Identities.Count -or
       $script:StageOwnedProcessState.Roots.Count -or !$script:StageOwnedProcessState.QuerySafe -or
       $script:StageOwnedProcessState.PersistentConfigured){throw 'Persistent services must be configured once before observation or child launch.'}
    $accepted=@(ConvertTo-StageOwnedPersistentServices -EnteredUtc $EnteredUtc -Services $Services)
    # Publish only after the entire list validates; later mutation of caller objects cannot widen this copy.
    $script:StageOwnedProcessState.PersistentServices=$accepted
    $script:StageOwnedProcessState.PersistentConfigured=$true
}

function Get-StageOwnedPersistentClassification {
    [CmdletBinding()]
    param([Parameter(Mandatory)]$Snapshot,
          [Parameter(Mandatory)][AllowEmptyCollection()][uint32[]]$SupplementalProcessIds,
          [Parameter(Mandatory)][AllowEmptyCollection()][uint32[]]$TrackedProcessIds,
          [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Services)
    if($Snapshot.querySucceeded -isnot [bool] -or !$Snapshot.querySucceeded -or $null -eq $Snapshot.rows){throw 'Complete process census required for persistent classification.'}
    $records=@()
    if($Services.Count){$records=@(ConvertTo-StageOwnedPersistentServices -EnteredUtc $Services[0].enteredUtc -Services $Services)}
    $admitted=@(foreach($service in $records){
        # Validate even a nonmatching configured service; unknown evidence cannot support a cleanup PASS.
        $identity=Get-StageProcessIdentityDisposition -Recorded $service -Snapshot $Snapshot
        if($identity.state -ceq 'Unknown'){throw ('Persistent service identity unknown: '+$identity.reason)}
        if($identity.state -ceq 'OriginalAlive' -and $service.processId -in $SupplementalProcessIds -and
           $service.processId -notin $TrackedProcessIds){
            [pscustomobject]@{processId=$service.processId;createdUtc=$service.createdUtc;precision=$service.precision;
                admittedAtUtc=$service.admittedAtUtc;enteredUtc=$service.enteredUtc;coordinator=$service.coordinator;role=$service.role;
                custodyReceipt=$service.custodyReceipt;observedCreatedUtc=$identity.observedCreatedUtc;
                comparisonQuantumTicks=$identity.comparisonQuantumTicks}
        }
    })
    [pscustomobject]@{admittedPersistentServices=$admitted;retainedSupplementalProcessIds=@($SupplementalProcessIds|Where-Object{$_ -notin @($admitted.processId)})}
}
function Save-Result {
    [IO.File]::WriteAllText((Join-Path $outputRoot 'result.json'), ($script:result | ConvertTo-Json -Depth 12))
}
function Observe-Owned {
    $script:StageOwnedProcessState.ObservationStarted=$true
    $script:StageOwnedProcessState.AdmittedPersistentServices=@()
    $script:StageOwnedProcessState.PersistentClassificationSafe=$false
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
        if($null -eq $script:StageOwnedProcessState.RunnerIdentity){
            $self=[Diagnostics.Process]::GetCurrentProcess()
            try{$script:StageOwnedProcessState.RunnerIdentity=[pscustomobject]@{processId=$PID;createdUtc=$self.StartTime.ToUniversalTime();precision='Tick100ns'}}
            finally{$self.Dispose()}
        }
        $ancestors=@(Get-StageRunnerAncestorIds -Runner $script:StageOwnedProcessState.RunnerIdentity -Snapshot $snapshot)
        $observation = Update-StageOwnedProcessLineage -Owned $script:StageOwnedProcessState.Identities -Snapshot $snapshot -Precision $script:StageOwnedProcessState.Precision
        $supplemental=@($rows | Where-Object {
            $_.ProcessId -ne $PID -and $_.ProcessId -notin $ancestors -and $_.CommandLine -and
            ($_.CommandLine.IndexOf($sourceRoot,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
             $_.CommandLine.IndexOf($outputRoot,[StringComparison]::OrdinalIgnoreCase) -ge 0)
        } | ForEach-Object{[uint32]$_.ProcessId})
        $classification=Get-StageOwnedPersistentClassification -Snapshot $snapshot -SupplementalProcessIds $supplemental `
            -TrackedProcessIds @($script:StageOwnedProcessState.Identities.Keys) -Services $script:StageOwnedProcessState.PersistentServices
        $script:StageOwnedProcessState.AdmittedPersistentServices=@($classification.admittedPersistentServices)
        $script:StageOwnedProcessState.PersistentClassificationSafe=$true
    } catch { $script:StageOwnedProcessState.QuerySafe = $false; throw }
    @($rows | Where-Object {
        ($observation.originalAliveProcessIds -contains [uint32]$_.ProcessId) -or
        ([uint32]$_.ProcessId -in $classification.retainedSupplementalProcessIds)
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
    $script:StageOwnedProcessState.Roots.Add($child)
    try {
        $script:result.steps[-1].process = [ordered]@{pid=$child.Id; createdUtc=$null; executable=$Executable; arguments=$Arguments; timeoutSeconds=$TimeoutSeconds}
        Save-Result
        $script:StageOwnedProcessState.Identities[[uint32]$child.Id] = $child.StartTime.ToUniversalTime()
        $script:StageOwnedProcessState.Precision[[uint32]$child.Id] = 'Tick100ns'
        $script:result.steps[-1].process = [ordered]@{pid=$child.Id; createdUtc=$script:StageOwnedProcessState.Identities[[uint32]$child.Id].ToString('O'); executable=$Executable; arguments=$Arguments; timeoutSeconds=$TimeoutSeconds}
        Save-Result
    } catch { $script:StageOwnedProcessState.QuerySafe = $false; throw }
    $stdout = $child.StandardOutput.ReadToEndAsync(); $stderr = $child.StandardError.ReadToEndAsync()
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (!$child.WaitForExit(100)) {
        [void]@(Observe-Owned)
        if ([DateTime]::UtcNow -gt $deadline) {
            $script:StageOwnedProcessState.QuerySafe = $false
            throw "Owned $Name timed out; no forced termination; retain lease and recorded process identities"
        }
    }
    [void]@(Observe-Owned)
    if (!$stdout.Wait(5000) -or !$stderr.Wait(5000)) {
        $script:StageOwnedProcessState.QuerySafe = $false
        throw "Owned $Name streams remain open; retain lease"
    }
    [IO.File]::WriteAllText((Join-Path $outputRoot ($Name+'.stdout.log')), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $outputRoot ($Name+'.stderr.log')), $stderr.GetAwaiter().GetResult())
    $script:result.steps[-1].exitCode = $child.ExitCode
    $script:result.steps[-1].state = if ($child.ExitCode -eq 0) {'pass'} else {'failed'}
    Save-Result
    if ($child.ExitCode -ne 0) { throw "Owned $Name exited $($child.ExitCode)" }
}
# MSVC cl.exe starts VCTIP.EXE (compiler telemetry), which can outlive the compiler and hold the lease.
# Only an owned lineage VCTIP from an MSVC bin directory is stopped, after its own handle's start time
# matches the recorded identity at the recorded precision. Nothing else is terminated.
function Stop-StageOwnedCompilerTelemetry([object[]]$Remaining) {
    foreach ($row in $Remaining) {
        $id = [uint32]$row.ProcessId
        if (!$script:StageOwnedProcessState.Identities.ContainsKey($id) -or [string]$row.Name -ine 'vctip.exe' -or
            [string]$row.ExecutablePath -notmatch '\\VC\\Tools\\MSVC\\[^\\]+\\bin\\Host[^\\]+\\[^\\]+\\VCTIP\.EXE$') { continue }
        $precision = $null; if (!$script:StageOwnedProcessState.Precision.TryGetValue($id, [ref]$precision)) { $precision = 'Tick100ns' }
        $recorded = [pscustomobject]@{ processId=$id; createdUtc=$script:StageOwnedProcessState.Identities[$id]; precision=$precision }
        $process = $null
        try {
            $process = & $script:StageOwnedProcessState.OpenProcess $id
            $live = [pscustomobject]@{ querySucceeded=$true; rows=@([pscustomobject]@{
                processId=$id; createdUtc=$process.StartTime.ToUniversalTime(); precision='Tick100ns' }) }
            $identity = Get-StageProcessIdentityDisposition -Recorded $recorded -Snapshot $live
            if ($identity.state -cne 'OriginalAlive') {
                [ordered]@{ pid=$id; stopped=$false; reason=$identity.reason }; continue
            }
            $process.Kill()
            [ordered]@{ pid=$id; stopped=$true; createdUtc=$identity.recordedCreatedUtc; precision=$precision; exited=$process.WaitForExit(5000) }
        } catch { [ordered]@{ pid=$id; stopped=$false; reason=$_.Exception.Message } }
        finally { if ($null -ne $process) { $process.Dispose() } }
    }
}
function Complete-Owned {
    try {
        $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(5)
        do { $remaining = @(Observe-Owned) } while ($remaining.Count -gt 0 -and [DateTime]::UtcNow -lt $cleanupDeadline)
        if ($remaining.Count -gt 0) {
            $telemetry = @(Stop-StageOwnedCompilerTelemetry $remaining)
            if ($telemetry.Count -gt 0) {
                $script:result.compilerTelemetry = $telemetry
                # A fresh complete census decides quiescence; a stop request alone proves nothing.
                $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(5)
                do { $remaining = @(Observe-Owned) } while ($remaining.Count -gt 0 -and [DateTime]::UtcNow -lt $cleanupDeadline)
            }
        }
    } catch { $script:StageOwnedProcessState.QuerySafe = $false; $remaining = @(); $script:result.processQueryError = $_.Exception.Message }
    $script:result.ownedLineage = @($script:StageOwnedProcessState.Identities.GetEnumerator() | ForEach-Object {
        $precision = $null; if (!$script:StageOwnedProcessState.Precision.TryGetValue($_.Key, [ref]$precision)) { $precision = 'Tick100ns' }
        [ordered]@{pid=$_.Key; createdUtc=$_.Value.ToString('O'); precision=$precision} })
    $script:result.remainingScopedChildren = @($remaining | Select-Object ProcessId,ParentProcessId,CreationDate,Name,ExecutablePath,CommandLine)
    $script:result.processQuerySafe = $script:StageOwnedProcessState.QuerySafe
    $script:result.admittedPersistentServices = @($script:StageOwnedProcessState.AdmittedPersistentServices)
    $script:result.persistentServiceClassificationSafe = $script:StageOwnedProcessState.PersistentClassificationSafe
    $script:result.childrenQuiescent = $script:StageOwnedProcessState.QuerySafe -and $remaining.Count -eq 0
    foreach ($child in $script:StageOwnedProcessState.Roots) {
        try {
            if ($child.HasExited) { $child.Dispose() }
            else { $script:result.childrenQuiescent = $false; $script:result.processQueryError = 'A retained process handle still reports alive' }
        }
        catch { $script:result.childrenQuiescent = $false; $script:result.processQuerySafe = $false; $script:result.processQueryError = $_.Exception.Message }
    }
}
