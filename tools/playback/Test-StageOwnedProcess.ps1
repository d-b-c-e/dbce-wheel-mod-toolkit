#Requires -Version 7.0
# Synthetic snapshots only. No live process census, launch, wait, kill or files.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Stage-OwnedProcess.ps1')
$checks=0
function Assert([bool]$Ok, [string]$Why) {
    if (!$Ok) { throw $Why }
    $script:checks++
}
function Stamp([string]$Value) { [DateTimeOffset]::Parse($Value).UtcDateTime }
function Row([uint32]$ProcessId, [uint32]$ParentProcessId, $CreatedUtc,
    [string]$Precision='CimMicrosecond') {
    [pscustomobject]@{ processId=$ProcessId; parentProcessId=$ParentProcessId;
        createdUtc=$CreatedUtc; precision=$Precision }
}
function Snapshot([object[]]$Rows=@(), [bool]$Success=$true) {
    [pscustomobject]@{ querySucceeded=$Success; rows=$Rows }
}
function Tracked([hashtable]$Values) {
    $identities=[Collections.Generic.Dictionary[uint32,DateTime]]::new()
    foreach ($entry in $Values.GetEnumerator()) { $identities.Add([uint32]$entry.Key, (Stamp $entry.Value)) }
    return ,$identities
}
function Assert-Ids($Actual, $Expected, [string]$Why) {
    Assert ((@($Actual | Sort-Object) -join ',') -ceq (@($Expected | Sort-Object) -join ',')) $Why
}
function Describe-Tracked($Identities) {
    (@(foreach ($identityId in @($Identities.Keys | Sort-Object)) {
        [string]$identityId + '=' + $Identities[$identityId].ToString('o')
    }) -join ';')
}
function Assert-Refuses($Identities, $Census, [string]$Reason, [string]$Why) {
    $before=Describe-Tracked $Identities
    $refusal=''
    try { $null=Update-StageOwnedProcessLineage -Owned $Identities -Snapshot $Census }
    catch { $refusal=$_.Exception.Message }
    Assert ($refusal.Contains($Reason)) $Why
    Assert ((Describe-Tracked $Identities) -ceq $before) ($Why + ' Tracking must remain unchanged.')
}

$root='2026-10-15T23:00:00.1234567Z'
$rootCim='2026-10-15T23:00:00.1234560Z'
$child='2026-10-15T23:00:01.0000000Z'
$grandchild='2026-10-15T23:00:02.0000000Z'
$replacement='2026-10-15T23:01:00.0000000Z'
$foreignChild='2026-10-15T23:01:01.0000000Z'
$foreignGrandchild='2026-10-15T23:01:02.0000000Z'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 $rootCim), (Row 200 100 $child)))
Assert-Ids $r.adoptedProcessIds @(200) 'A same-snapshot original parent admits its true child.'
Assert-Ids $r.originalAliveProcessIds @(100,200) 'CIM truncation retains the original root and child.'
Assert ($identities[200] -eq (Stamp $child)) 'Adoption records the exact observed child creation.'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 300 200 $grandchild), (Row 200 100 $child), (Row 100 0 $rootCim)))
Assert-Ids $r.adoptedProcessIds @(200,300) 'Unordered true child/grandchild rows converge.'
Assert-Ids $identities.Keys @(100,200,300) 'All true descendants remain in retained lineage.'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 $replacement), (Row 200 100 $foreignChild), (Row 300 200 $foreignGrandchild)))
Assert-Ids $r.adoptedProcessIds @() 'A foreign shell reusing a tracked PID admits no children.'
Assert-Ids $r.originalAliveProcessIds @() 'The replacement and foreign descendants are not owned.'
Assert-Ids $identities.Keys @(100) 'Foreign reuse cannot contaminate retained lineage.'
Assert ($identities[100] -eq (Stamp $root)) 'Reuse never replaces the original recorded identity.'

$identities=Tracked @{100=$root; 200=$child}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 $replacement), (Row 200 100 $child), (Row 300 200 $grandchild),
    (Row 400 100 $foreignChild), (Row 500 400 $foreignGrandchild)))
Assert-Ids $r.adoptedProcessIds @(300) 'An already-recorded true child can still admit its descendant after root reuse.'
Assert-Ids $r.originalAliveProcessIds @(200,300) 'True child/grandchild stay owned independently of root replacement.'
Assert-Ids $identities.Keys @(100,200,300) 'Foreign siblings and their descendants stay untracked.'

$identities=Tracked @{100=$root; 200=$child}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 300 200 $grandchild), (Row 200 100 $child), (Row 400 100 $foreignChild)))
Assert-Ids $r.originalAliveProcessIds @(200,300) 'True retained descendants survive an absent root.'
Assert-Ids $r.adoptedProcessIds @(300) 'An absent root cannot admit a new apparent child.'
Assert-Ids $identities.Keys @(100,200,300) 'Exiting roots remain recorded without speculative adoption.'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 200 100 $child), (Row 300 200 $grandchild)))
Assert-Ids $r.adoptedProcessIds @() 'A remembered parent PID absent from the snapshot cannot admit new descendants.'
Assert-Ids $r.originalAliveProcessIds @() 'Unobserved ancestry is not inferred from child timestamps.'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 $rootCim), (Row 200 100 '2026-10-15T22:00:00.0000000Z')))
Assert-Ids $r.adoptedProcessIds @() 'A child older than its current parent belongs to no proven owned lineage.'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 $rootCim), (Row 200 100 $rootCim)))
Assert-Ids $r.adoptedProcessIds @(200) 'Same producer bucket is not rejected due to root sub-microsecond precision.'

$identities=Tracked @{100=$root}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 '2026-10-15T23:00:00.1234570Z'), (Row 200 100 $child)))
Assert-Ids $r.originalAliveProcessIds @() 'Next-microsecond reuse is not hidden by a one-millisecond tolerance.'
Assert-Ids $r.adoptedProcessIds @() 'Near-time parent reuse also blocks descendants.'

$identities=Tracked @{100=$root; 200=$child}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 100 0 $rootCim), (Row 200 100 $foreignChild), (Row 300 200 $foreignGrandchild)))
Assert-Ids $r.originalAliveProcessIds @(100) 'A reused tracked child and its descendants are excluded.'
Assert-Ids $r.adoptedProcessIds @() 'A reused child is not re-adopted through the original root.'
Assert ($identities[200] -eq (Stamp $child)) 'A recorded child identity remains immutable on reuse.'

$identities=Tracked @{100=$root; 200=$child}
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot)
Assert-Ids $r.originalAliveProcessIds @() 'A complete empty snapshot reports no original identities alive.'
Assert-Ids $identities.Keys @(100,200) 'An empty snapshot does not discard retained identities.'

Assert-Refuses (Tracked @{100=$root}) (Snapshot @() $false) 'Complete process census' 'A failed query is not an empty census.'
Assert-Refuses (Tracked @{100=$root}) (Snapshot @((Row 100 0 $null))) 'Tracked identity unknown' 'Missing tracked creation refuses.'
Assert-Refuses (Tracked @{100=$root}) (Snapshot @((Row 100 0 $rootCim),(Row 100 0 $rootCim))) 'Duplicate' 'Duplicate PID rows refuse.'
Assert-Refuses (Tracked @{100=$root}) (Snapshot @((Row 100 0 '2026-10-15T22:59:00Z'))) 'Tracked identity unknown' 'Earlier parent creation is unknown, never eligible.'
Assert-Refuses (Tracked @{100=$root}) (Snapshot @(
    (Row 100 0 $rootCim), (Row 200 100 $child), (Row 300 100 $null))) 'Candidate descendant' 'A later malformed candidate leaves no partial adoption.'
Assert-Refuses (Tracked @{100=$root}) (Snapshot @((Row 100 0 $rootCim 'UnknownPrecision'))) 'Tracked identity unknown' 'Unknown producer precision refuses.'

# Exercise the unchanged Observe-Owned entry point with a synthetic CIM provider.
# This shadows the live cmdlet; an incorrect class or any unexpected query fails.
function Get-CimInstance {
    param([string]$ClassName, [string]$ErrorAction)
    if ($ClassName -cne 'Win32_Process') { throw 'Unexpected census class.' }
    $script:fixtureCimRows
    $self=[Diagnostics.Process]::GetCurrentProcess()
    try{$stamp=$self.StartTime.ToUniversalTime();$stamp=[DateTime]::new($stamp.Ticks-($stamp.Ticks%10),[DateTimeKind]::Utc)}finally{$self.Dispose()}
    [pscustomobject]@{ProcessId=$PID;ParentProcessId=$script:fixtureRunnerParent;CreationDate=$stamp;CommandLine='runner'}
}
$fixtureRunnerParent=0
$owned=Tracked @{100=$root}
$observeExternal=$null
$sourceRoot='Z:\synthetic-owned-source'
$outputRoot='Z:\synthetic-owned-output'
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $replacement);CommandLine='foreign-shell'},
    [pscustomobject]@{ProcessId=200;ParentProcessId=100;CreationDate=(Stamp $foreignChild);CommandLine='foreign-child'})
$observed=@(Observe-Owned)
Assert ($observed.Count -eq 0) 'The adapter excludes reused root and foreign child using the same identity classifier.'
Assert-Ids $owned.Keys @(100) 'Adapter does not adopt a foreign child.'
Assert $processQuerySafe 'Known PID reuse remains a safe query, not an unknown identity.'

$owned=Tracked @{100=$root}
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine='owned-root'},
    [pscustomobject]@{ProcessId=200;ParentProcessId=100;CreationDate=(Stamp $child);CommandLine='owned-child'})
$observed=@(Observe-Owned)
Assert-Ids @($observed.ProcessId) @(100,200) 'Adapter retains the original true-child contract.'

$fixtureRunnerParent=700
$owned=Tracked @{100=$root;700=$root}
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine=$sourceRoot},
    [pscustomobject]@{ProcessId=700;ParentProcessId=600;CreationDate=(Stamp $rootCim);CommandLine=$sourceRoot},
    [pscustomobject]@{ProcessId=600;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine=$outputRoot},
    [pscustomobject]@{ProcessId=900;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine=$sourceRoot})
$observed=@(Observe-Owned)
Assert-Ids @($observed.ProcessId) @(100,700,900,$PID) 'Only verified runner ancestors are excluded from supplemental text scope; owned ancestry, their retained runner descendant and unrelated matches remain.'
$fixtureRunnerParent=0

$runner=[pscustomobject]@{processId=300;createdUtc=$grandchild;precision='Tick100ns'}
$ancestorRows=@((Row 300 200 $grandchild),(Row 200 100 $child),(Row 100 0 $rootCim))
Assert-Ids @(Get-StageRunnerAncestorIds $runner (Snapshot $ancestorRows)) @(100,200) 'A valid runner chain is verified from the same snapshot.'
Assert-Ids @(Get-StageRunnerAncestorIds $runner (Snapshot @((Row 300 200 $grandchild),(Row 200 0 $replacement)))) @() 'A parent PID newer than its child is not excluded.'
Assert-Ids @(Get-StageRunnerAncestorIds $runner (Snapshot @((Row 300 200 $grandchild)))) @() 'An absent parent adds no exclusion.'
foreach($invalid in @(
    (Snapshot @((Row 300 200 $grandchild),(Row 200 0 $null))),
    (Snapshot @((Row 300 200 $grandchild),(Row 200 300 $child))),
    (Snapshot @((Row 300 0 $grandchild),(Row 300 0 $grandchild))),
    (Snapshot @((Row 300 0 $null))))) {
    $refused=$false
    try{$null=Get-StageRunnerAncestorIds $runner $invalid}catch{$refused=$true}
    Assert $refused 'Unknown, cyclic or duplicate runner ancestry refuses supplemental exclusion.'
}

$owned=Tracked @{100=$root}
$fixtureCimRows=@([pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=$null;CommandLine='unknown'})
$refused=$false
try { $null=Observe-Owned } catch { $refused=$true }
Assert $refused 'Adapter refuses unknown tracked identity.'
Assert (!$processQuerySafe) 'Adapter latches unknown identity as unsafe.'

"Test-StageOwnedProcess: $checks checks passed; synthetic snapshots only"
