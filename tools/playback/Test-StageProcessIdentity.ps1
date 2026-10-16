#Requires -Version 7.0
# Pure fixtures and injected query errors only. No actual process census or launch.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Stage-ProcessIdentity.ps1')
$checks=0
function Assert($ok, [string]$why) { if (!$ok) { throw $why }; $script:checks++ }
function Record($stamp='2026-10-15T20:43:27.7891977Z', $precision='Tick100ns', $processId=50812) {
    [pscustomobject]@{ processId=$processId; createdUtc=$stamp; precision=$precision }
}
function Snapshot([object[]]$rows=@(), [bool]$success=$true) {
    [pscustomobject]@{ querySucceeded=$success; rows=$rows; error='' }
}
function Check-State($recorded, $snapshot, [string]$state, [bool]$absent, [string]$why) {
    $r=Get-StageProcessIdentityDisposition -Recorded $recorded -Snapshot $snapshot
    Assert ($r.state -ceq $state -and $r.originalAbsent -ceq $absent) $why
    $r
}
$recorded=Record
$null=Check-State $recorded (Snapshot) 'Absent' $true 'Complete empty census must establish absence.'
$observed=Record '2026-10-15T20:43:27.7891970Z' 'CimMicrosecond'
$r=Check-State $recorded (Snapshot @($observed)) 'OriginalAlive' $false 'CIM truncation must not invent PID reuse.'
Assert ($r.comparisonQuantumTicks -eq 10) 'CIM comparison must use microseconds, not milliseconds.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T20:43:27.7891980Z' 'CimMicrosecond'))) 'Reused' $true 'Next microsecond bucket must identify reuse without 1ms widening.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T20:43:27.7891960Z' 'CimMicrosecond'))) 'Unknown' $false 'Earlier observed creation is ambiguous.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T20:43:27.7891978Z'))) 'Reused' $true 'Two full-precision producers distinguish adjacent 100ns ticks.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T20:45:31.8258800Z' 'CimMicrosecond'))) 'Reused' $true 'Preserved checker PID reuse must not block original identity absence.'
$null=Check-State $recorded (Snapshot @($observed,$observed)) 'Unknown' $false 'Duplicate matching census rows refuse.'
$null=Check-State $recorded (Snapshot @() $false) 'Unknown' $false 'Failed query must not become empty-census absence.'
$null=Check-State $recorded (Snapshot @((Record $null 'CimMicrosecond'))) 'Unknown' $false 'Missing live creation cannot be inferred absent.'
$null=Check-State (Record $null) (Snapshot) 'Unknown' $false 'Invalid original identity cannot establish closure.'
$null=Check-State (Record 'not-a-date') (Snapshot) 'Unknown' $false 'Malformed original timestamp refuses.'
$null=Check-State (Record '2026-10-15T20:43:27.7891977') (Snapshot) 'Unknown' $false 'Unzoned string must not assume local time.'
$null=Check-State (Record '2026-10-15T20:43:27.7891977Z' 'CimMicrosecond') (Snapshot) 'Unknown' $false 'False claim of microsecond producer precision refuses.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T20:43:27.7891971Z' 'CimMicrosecond'))) 'Unknown' $false 'Unsupported observed sub-microsecond data refuses.'
$null=Check-State (Record '2026-10-15T20:43:27Z' 'Unspecified') (Snapshot) 'Unknown' $false 'Unknown producer precision refuses.'
$null=Check-State (Record '2026-10-15T20:43:27Z' 'Tick100ns' 0) (Snapshot) 'Unknown' $false 'Invalid original PID refuses.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T15:43:27.7891970-05:00' 'CimMicrosecond'))) 'OriginalAlive' $false 'Offset timestamps normalize to UTC.'
$null=Check-State $recorded (Snapshot @((Record ([DateTime]::SpecifyKind([DateTime]::new(2026,10,15), [DateTimeKind]::Unspecified))))) 'Unknown' $false 'Unspecified DateTime must not assume a timezone.'
$null=Check-State $recorded ([pscustomobject]@{ querySucceeded='true'; rows=@() }) 'Unknown' $false 'String query success is not positive query evidence.'
$null=Check-State $recorded (Snapshot @((Record '2026-10-15T20:43:27Z' 'Tick100ns' 'bad-pid'))) 'Unknown' $false 'Malformed census PID cannot hide a matching process.'
$query=Get-StageProcessIdentitySnapshot -QueryProvider { throw 'fixture query unavailable' }
Assert (!$query.querySucceeded -and @($query.rows).Count -eq 0) 'Query exception must produce unknown census.'
$query=Get-StageProcessIdentitySnapshot -QueryProvider { Write-Error 'fixture query error' }
Assert (!$query.querySucceeded -and @($query.rows).Count -eq 0) 'Nonterminating query error must not become successful absence.'
$query=Get-StageProcessIdentitySnapshot -QueryProvider { [pscustomobject]@{ ProcessId=50812; CreationDate=[DateTimeOffset]::Parse('2026-10-15T20:43:27.789197Z').UtcDateTime }; throw 'partial failure' }
Assert (!$query.querySucceeded -and @($query.rows).Count -eq 0) 'Partial output before query failure must be discarded.'
$query=Get-StageProcessIdentitySnapshot -QueryProvider { [pscustomobject]@{ ProcessId=50812; CreationDate=[DateTimeOffset]::Parse('2026-10-15T20:43:27.789197Z').UtcDateTime } }
$null=Check-State $recorded $query 'OriginalAlive' $false 'Injected successful CIM observation must preserve producer precision.'
"Test-StageProcessIdentity: $checks checks passed; pure fixtures only"
