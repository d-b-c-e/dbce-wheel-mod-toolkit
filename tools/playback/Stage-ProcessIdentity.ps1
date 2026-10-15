#Requires -Version 7.0
# Read-only process census and pure PID/creation comparison. No termination,
# lease changes, file writes or game/device calls. Dot-sourcing performs no query.

function Get-StageProcessIdentitySnapshot {
    [CmdletBinding()]
    param([scriptblock]$QueryProvider = { Get-CimInstance Win32_Process -ErrorAction Stop })
    $ErrorActionPreference='Stop'
    try {
        # Buffer the entire query: partial output followed by an error is unknown.
        $raw = @(& $QueryProvider)
        $rows = @(foreach ($row in $raw) {
            if ($null -eq $row.ProcessId) { throw 'Process census row lacks a PID.' }
            [pscustomobject]@{ processId=$row.ProcessId; createdUtc=$row.CreationDate; precision='CimMicrosecond' }
        })
        [pscustomobject]@{ querySucceeded=$true; rows=$rows; error='' }
    } catch {
        [pscustomobject]@{ querySucceeded=$false; rows=@(); error=$_.Exception.Message }
    }
}

function ConvertTo-StageCreationStamp {
    param([AllowNull()][object]$Value, [AllowNull()][object]$Precision)
    $quantum = switch -CaseSensitive ($Precision) {
        'Tick100ns' { 1L }
        'CimMicrosecond' { 10L }
        default { return $null }
    }
    try {
        if ($Value -is [DateTimeOffset]) { $ticks=$Value.UtcDateTime.Ticks }
        elseif ($Value -is [DateTime] -and $Value.Kind -ne [DateTimeKind]::Unspecified) {
            $ticks=$Value.ToUniversalTime().Ticks
        } elseif ($Value -is [string] -and
            $Value -cmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,7})?(?:Z|[+-]\d{2}:\d{2})$') {
            $ticks=[DateTimeOffset]::Parse($Value, [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::None).UtcDateTime.Ticks
        } else { return $null }
        # Reject a claimed CIM timestamp carrying unsupported sub-microsecond data.
        if ($ticks -le 0 -or $ticks % $quantum -ne 0) { return $null }
        [pscustomobject]@{ ticks=[long]$ticks; quantum=[long]$quantum }
    } catch { return $null }
}

function Get-StageProcessIdentityDisposition {
    [CmdletBinding()]
    param(
        # Record shape: processId, createdUtc (ISO offset/Z or zoned DateTime),
        # precision ('Tick100ns' for Process.StartTime; 'CimMicrosecond' for CIM).
        [Parameter(Mandatory)][AllowNull()][object]$Recorded,
        [Parameter(Mandatory)][AllowNull()][object]$Snapshot
    )
    $result=[ordered]@{ state='Unknown'; originalAbsent=$false; processId=$null;
        recordedCreatedUtc=$null; observedCreatedUtc=$null; comparisonQuantumTicks=$null; reason='' }
    try {
        $id=0L
        if ($null -eq $Recorded -or ![long]::TryParse([string]$Recorded.processId, [ref]$id) -or
            $id -le 0 -or $id -gt [uint32]::MaxValue) { throw 'Recorded PID unavailable or invalid.' }
        $result.processId=$id
        $before=ConvertTo-StageCreationStamp $Recorded.createdUtc $Recorded.precision
        if ($null -eq $before) { throw 'Recorded creation timestamp or producer precision unavailable.' }
        $result.recordedCreatedUtc=[DateTime]::new($before.ticks, [DateTimeKind]::Utc).ToString('o')
        if ($null -eq $Snapshot -or $Snapshot.querySucceeded -isnot [bool] -or !$Snapshot.querySucceeded -or
            $null -eq $Snapshot.rows) { throw 'Complete process census unavailable.' }
        $matches=@()
        foreach ($row in @($Snapshot.rows)) {
            $rowId=0L
            if ($null -eq $row -or ![long]::TryParse([string]$row.processId, [ref]$rowId) -or
                $rowId -lt 0 -or $rowId -gt [uint32]::MaxValue) { throw 'Process census has an invalid PID.' }
            if ($rowId -eq $id) { $matches+=@($row) }
        }
        if ($matches.Count -eq 0) {
            $result.state='Absent'; $result.originalAbsent=$true; $result.reason='Recorded PID absent from complete census.'
        } elseif ($matches.Count -ne 1) { throw 'Multiple census rows have the recorded PID.' }
        else {
            $after=ConvertTo-StageCreationStamp $matches[0].createdUtc $matches[0].precision
            if ($null -eq $after) { throw 'Observed creation timestamp or producer precision unavailable.' }
            $result.observedCreatedUtc=[DateTime]::new($after.ticks, [DateTimeKind]::Utc).ToString('o')
            $quantum=[Math]::Max($before.quantum, $after.quantum)
            $result.comparisonQuantumTicks=$quantum
            # CIM carries microseconds (10 DateTime ticks). Project each stamp to
            # the coarser producer's bucket; never apply an elapsed-time tolerance.
            $a=$before.ticks-($before.ticks % $quantum)
            $b=$after.ticks-($after.ticks % $quantum)
            if ($b -eq $a) {
                $result.state='OriginalAlive'; $result.reason='Same PID and creation bucket at supported precision.'
            } elseif ($b -gt $a) {
                $result.state='Reused'; $result.originalAbsent=$true
                $result.reason='Same PID with a later creation bucket; replacement is never a cleanup target.'
            } else { throw 'Observed creation precedes recorded creation; identity is ambiguous.' }
        }
    } catch { $result.reason=$_.Exception.Message }
    [pscustomobject]$result
}
