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
$script:StageOwnedProcessState.Identities=Tracked @{100=$root}
$observeExternal=$null
$sourceRoot='Z:\synthetic-owned-source'
$outputRoot='Z:\synthetic-owned-output'
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $replacement);CommandLine='foreign-shell'},
    [pscustomobject]@{ProcessId=200;ParentProcessId=100;CreationDate=(Stamp $foreignChild);CommandLine='foreign-child'})
$observed=@(Observe-Owned)
Assert ($observed.Count -eq 0) 'The adapter excludes reused root and foreign child using the same identity classifier.'
Assert-Ids $script:StageOwnedProcessState.Identities.Keys @(100) 'Adapter does not adopt a foreign child.'
Assert $script:StageOwnedProcessState.QuerySafe 'Known PID reuse remains a safe query, not an unknown identity.'

$script:StageOwnedProcessState.Identities=Tracked @{100=$root}
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine='owned-root'},
    [pscustomobject]@{ProcessId=200;ParentProcessId=100;CreationDate=(Stamp $child);CommandLine='owned-child'})
$observed=@(Observe-Owned)
Assert-Ids @($observed.ProcessId) @(100,200) 'Adapter retains the original true-child contract.'

$fixtureRunnerParent=700
$script:StageOwnedProcessState.Identities=Tracked @{100=$root;700=$root}
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

$script:StageOwnedProcessState.Identities=Tracked @{100=$root}
$fixtureCimRows=@([pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=$null;CommandLine='unknown'})
$refused=$false
try { $null=Observe-Owned } catch { $refused=$true }
Assert $refused 'Adapter refuses unknown tracked identity.'
Assert (!$script:StageOwnedProcessState.QuerySafe) 'Adapter latches unknown identity as unsafe.'

# Owned MSVC telemetry (haru shared run-01): VCTIP 32232 outlived cl.exe and held the lease. Its lineage
# stamp came from CIM; the live handle carries 100 ns ticks. Fake handles only; nothing is terminated.
$vctipPath='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\VCTIP.EXE'
$compiler='2026-10-17T06:17:07.6184430Z'
$vctipCim='2026-10-17T06:17:07.9842080Z'
$vctipLive='2026-10-17T06:17:07.9842087Z'
$liveCensus=Snapshot @((Row 32232 0 (Stamp $vctipLive) 'Tick100ns'))
$mislabeled=[pscustomobject]@{processId=32232;createdUtc=(Stamp $vctipCim);precision='Tick100ns'}
Assert ((Get-StageProcessIdentityDisposition $mislabeled $liveCensus).state -ceq 'Reused') 'Run-01 cause: a CIM stamp labeled Tick100ns reads as reused against the live handle.'
$identities=Tracked @{35916=$compiler}
$precisions=[Collections.Generic.Dictionary[uint32,string]]::new()
$r=Update-StageOwnedProcessLineage -Owned $identities -Snapshot (Snapshot @(
    (Row 35916 0 $compiler), (Row 32232 35916 $vctipCim))) -Precision $precisions
Assert-Ids $r.adoptedProcessIds @(32232) 'The compiler admits its telemetry child.'
Assert ($precisions[32232] -ceq 'CimMicrosecond' -and !$precisions.ContainsKey(35916)) 'Adoption records CIM precision; a root without an entry stays Tick100ns.'
$recorded=[pscustomobject]@{processId=32232;createdUtc=$identities[32232];precision=$precisions[32232]}
Assert ((Get-StageProcessIdentityDisposition $recorded $liveCensus).state -ceq 'OriginalAlive') 'The carried precision matches the same process through its live handle.'

function New-FakeHandle([string]$Start) {
    $handle=[pscustomobject]@{StartTime=(Stamp $Start).ToLocalTime();Killed=$false;Disposed=$false}
    $handle|Add-Member ScriptMethod Kill { $this.Killed=$true; $script:fixtureCimRows=@($script:fixtureCimRows|Where-Object ProcessId -ne 32232) }
    $handle|Add-Member ScriptMethod WaitForExit { param($Milliseconds) $true }
    $handle|Add-Member ScriptMethod Dispose { $this.Disposed=$true }
    $handle
}
$script:StageOwnedProcessState.OpenProcess={ param([uint32]$Id) if($Id -ne 32232){throw 'Unexpected handle.'}; $script:handle }
$script:StageOwnedProcessState.Identities=Tracked @{35916=$compiler}
$script:StageOwnedProcessState.Precision=[Collections.Generic.Dictionary[uint32,string]]::new()
$script:StageOwnedProcessState.Roots=[Collections.Generic.List[Diagnostics.Process]]::new()
$script:StageOwnedProcessState.QuerySafe=$true
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=35916;ParentProcessId=0;CreationDate=(Stamp $compiler);CommandLine='cl';Name='cl.exe';ExecutablePath=($vctipPath -replace 'VCTIP','cl')},
    [pscustomobject]@{ProcessId=32232;ParentProcessId=35916;CreationDate=(Stamp $vctipCim);CommandLine='vctip';Name='VCTIP.EXE';ExecutablePath=$vctipPath})
$null=@(Observe-Owned)
$fixtureCimRows=@($fixtureCimRows|Where-Object ProcessId -ne 35916) # the compiler exits; its telemetry child stays
$handle=New-FakeHandle $vctipLive
$result=[ordered]@{}
Complete-Owned
Assert ($handle.Killed -and $handle.Disposed) 'The owned VCTIP is stopped through its verified, released handle.'
Assert ($result.childrenQuiescent -and @($result.remainingScopedChildren).Count -eq 0) 'A fresh census after the stop decides quiescence.'
$stop=@($result.compilerTelemetry)
Assert ($stop.Count -eq 1 -and $stop[0].stopped -and $stop[0].precision -ceq 'CimMicrosecond') 'The stop records the CIM identity it verified.'
Assert (@($result.ownedLineage|Where-Object {$_.pid -eq 32232})[0].precision -ceq 'CimMicrosecond' -and
    @($result.ownedLineage|Where-Object {$_.pid -eq 35916})[0].precision -ceq 'Tick100ns') 'ownedLineage carries each producer precision.'

$script:StageOwnedProcessState.Identities=Tracked @{32232=$vctipCim}
$script:StageOwnedProcessState.Precision=[Collections.Generic.Dictionary[uint32,string]]::new(); $script:StageOwnedProcessState.Precision[32232]='CimMicrosecond'
$vctipRow=[pscustomobject]@{ProcessId=32232;Name='VCTIP.EXE';ExecutablePath=$vctipPath}
$handle=New-FakeHandle '2026-10-17T06:19:00.0000001Z'
$stop=@(Stop-StageOwnedCompilerTelemetry @($vctipRow))
Assert (!$handle.Killed -and $handle.Disposed -and $stop.Count -eq 1 -and !$stop[0].stopped) 'A reused PID is never stopped, and its handle is released.'
$handle=New-FakeHandle $vctipLive
$stop=@(Stop-StageOwnedCompilerTelemetry @(
    [pscustomobject]@{ProcessId=4444;Name='VCTIP.EXE';ExecutablePath=$vctipPath},
    [pscustomobject]@{ProcessId=32232;Name='cl.exe';ExecutablePath=($vctipPath -replace 'VCTIP','cl')},
    [pscustomobject]@{ProcessId=32232;Name='VCTIP.EXE';ExecutablePath='C:\Temp\VCTIP.EXE'},
    [pscustomobject]@{ProcessId=32232;Name='VCTIP.EXE';ExecutablePath=$null}))
Assert (!$handle.Killed -and $stop.Count -eq 0) 'Only an owned VCTIP image in an MSVC bin directory qualifies; nothing else is opened.'

# F-Zero run01/326a6969: the caller reused $owned for installed receipt names.
# Both an existing caller value at dot-source time and a later assignment must
# stay independent of tracking. This exercises the real Observe/Complete API;
# census rows are synthetic and no child process or termination is used.
$owned = @('FZeroDeluxe.exe', 'dbce_shim.dll')
$ownedPrecision = 'caller precision note'
$ownedRoots = 'caller receipt roots'
$processQuerySafe = 'caller receipt status'
$ownedRunnerIdentity = 'caller identity note'
$openOwnedProcess = 'caller handle note'
. (Join-Path $PSScriptRoot 'Stage-OwnedProcess.ps1')
Assert (($owned -join ',') -ceq 'FZeroDeluxe.exe,dbce_shim.dll') 'Dot-sourcing does not overwrite the caller receipt names.'
$script:StageOwnedProcessState.Identities = Tracked @{100=$root}
$sourceRoot='Z:\synthetic-owned-source'
$outputRoot='Z:\synthetic-owned-output'
$observeExternal=$null
$fixtureRunnerParent=0
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine='owned-root'},
    [pscustomobject]@{ProcessId=200;ParentProcessId=100;CreationDate=(Stamp $child);CommandLine='owned-child'})
$owned = @('FZeroDeluxe.exe', 'dbce_shim.dll', 'install-receipt.json')
$observed=@(Observe-Owned)
Assert-Ids @($observed.ProcessId) @(100,200) 'Caller $owned reassignment cannot replace the tracker or prevent child adoption.'
Assert ($script:StageOwnedProcessState.Precision[200] -ceq 'CimMicrosecond') 'Caller variables do not replace carried descendant precision.'
$fixtureCimRows=@() # The original synthetic processes exit.
$result=[ordered]@{}
Complete-Owned
Assert ($result.processQuerySafe -and $result.childrenQuiescent -and @($result.remainingScopedChildren).Count -eq 0) 'Cleanup completes after caller $owned reassignment.'
Assert-Ids @($result.ownedLineage.pid) @(100,200) 'Cleanup receipts retain original owned identities after the caller collision.'
Assert (($owned -join ',') -ceq 'FZeroDeluxe.exe,dbce_shim.dll,install-receipt.json' -and
    $ownedPrecision -ceq 'caller precision note' -and $ownedRoots -ceq 'caller receipt roots' -and
    $processQuerySafe -ceq 'caller receipt status' -and $ownedRunnerIdentity -ceq 'caller identity note' -and
    $openOwnedProcess -ceq 'caller handle note') 'The helper leaves ordinary caller bookkeeping untouched.'

# Optional preflight admissions: no receipt file read, real census, live handle or service operation.
$entry='2026-10-15T23:00:10.0000000Z'
function Admission {
    [pscustomobject]@{processId=900;createdUtc=$child;precision='CimMicrosecond';admittedAtUtc='2026-10-15T23:00:05.0000000Z';
        coordinator='fixture-coordinator';role='fixture-keeper';custodyReceipt=[pscustomobject]@{path='Z:\synthetic\custody.json';sha256=('a'*64)}}
}
function Reset-PersistentFixture {
    $script:StageOwnedProcessState.Identities=Tracked @{}
    $script:StageOwnedProcessState.Precision=[Collections.Generic.Dictionary[uint32,string]]::new()
    $script:StageOwnedProcessState.Roots=[Collections.Generic.List[Diagnostics.Process]]::new()
    $script:StageOwnedProcessState.QuerySafe=$true
    $script:StageOwnedProcessState.ObservationStarted=$false
    $script:StageOwnedProcessState.PersistentConfigured=$false
    $script:StageOwnedProcessState.PersistentServices=@()
    $script:StageOwnedProcessState.AdmittedPersistentServices=@()
    $script:StageOwnedProcessState.PersistentClassificationSafe=$true
}
Reset-PersistentFixture
$fixtureCimRows=@([pscustomobject]@{ProcessId=900;ParentProcessId=0;CreationDate=(Stamp $child);CommandLine=$sourceRoot})
Assert-Ids @((Observe-Owned).ProcessId) @(900) 'Default callers retain the original supplemental path scope.'
$late=$false
try{Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @((Admission))}catch{$late=$true}
Assert $late 'Admissions cannot be configured after observation.'

Reset-PersistentFixture
$service=Admission
Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @($service)
$service.processId=901;$service.custodyReceipt.path='Z:\different\custody.json'
Assert (@(Observe-Owned).Count -eq 0) 'Only an exact pre-entry identity is admitted from supplemental scope.'
$result=[ordered]@{}
Complete-Owned
Assert ($result.processQuerySafe -and $result.childrenQuiescent -and $result.persistentServiceClassificationSafe) 'An exact persistent service does not retain transient cleanup.'
Assert-Ids @($result.admittedPersistentServices.processId) @(900) 'Admitted persistent identities are reported separately.'
Assert ($result.admittedPersistentServices[0].custodyReceipt.path -ceq 'Z:\synthetic\custody.json' -and
    $result.admittedPersistentServices[0].coordinator -ceq 'fixture-coordinator') 'The copied original custody reference and role survive caller mutation.'
$again=$false
try{Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @()}catch{$again=$true}
Assert $again 'Admissions cannot be replaced or widened after configuration.'

Reset-PersistentFixture
Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @((Admission))
$script:StageOwnedProcessState.Identities=Tracked @{100=$root}
$fixtureCimRows=@(
    [pscustomobject]@{ProcessId=100;ParentProcessId=0;CreationDate=(Stamp $rootCim);CommandLine='owned'},
    [pscustomobject]@{ProcessId=900;ParentProcessId=100;CreationDate=(Stamp $child);CommandLine=$sourceRoot})
Assert-Ids @((Observe-Owned).ProcessId) @(100,900) 'Actual adopted worker lineage wins over an otherwise exact persistent admission.'
Assert (@($script:StageOwnedProcessState.AdmittedPersistentServices).Count -eq 0) 'A true descendant is never labeled an admitted persistent service.'

Reset-PersistentFixture
Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @((Admission))
$fixtureCimRows=@([pscustomobject]@{ProcessId=900;ParentProcessId=0;CreationDate=(Stamp $replacement);CommandLine=$sourceRoot})
Assert-Ids @((Observe-Owned).ProcessId) @(900) 'A reused admission PID does not exempt its replacement supplemental match.'
Assert (@($script:StageOwnedProcessState.AdmittedPersistentServices).Count -eq 0) 'Reused identity never receives an admission receipt.'
$fixtureCimRows=@()
Assert (@(Observe-Owned).Count -eq 0 -and @($script:StageOwnedProcessState.AdmittedPersistentServices).Count -eq 0) 'A known absent service is not reported as still admitted.'
$fixtureCimRows=@([pscustomobject]@{ProcessId=900;ParentProcessId=0;CreationDate=(Stamp $child);CommandLine='unrelated-service'})
Assert (@(Observe-Owned).Count -eq 0 -and @($script:StageOwnedProcessState.AdmittedPersistentServices).Count -eq 0) 'Admissions classify only supplemental matches, not every process in the census.'
$fixtureCimRows=@([pscustomobject]@{ProcessId=900;ParentProcessId=0;CreationDate=$null;CommandLine=$sourceRoot})
$unknown=$false
try{$null=Observe-Owned}catch{$unknown=$true}
Assert ($unknown -and !$script:StageOwnedProcessState.QuerySafe -and !$script:StageOwnedProcessState.PersistentClassificationSafe) 'Unknown observed creation fails closed and latches unsafe classification.'
Assert (@($script:StageOwnedProcessState.AdmittedPersistentServices).Count -eq 0) 'An unsafe observation cannot retain stale admissions.'

foreach($change in @('missing-creation','invalid-precision','at-entry','ambiguous-entry','late-admission','missing-admission','missing-receipt','bad-hash','relative-receipt','blank-role','blank-coordinator')){
    Reset-PersistentFixture
    $service=Admission
    switch($change){
        'missing-creation' {$service.createdUtc=$null}
        'invalid-precision' {$service.precision='Unknown'}
        'at-entry' {$service.createdUtc=$entry}
        'ambiguous-entry' {$service.createdUtc='2026-10-15T23:00:10.0000000Z';$service.admittedAtUtc='2026-10-15T23:00:10.0000001Z'}
        'late-admission' {$service.admittedAtUtc=$entry}
        'missing-admission' {$service.admittedAtUtc=$null}
        'missing-receipt' {$service.custodyReceipt=$null}
        'bad-hash' {$service.custodyReceipt.sha256='not-a-hash'}
        'relative-receipt' {$service.custodyReceipt.path='relative.json'}
        'blank-role' {$service.role=''}
        'blank-coordinator' {$service.coordinator=''}
    }
    $refused=$false
    $caseEntry=if($change -ceq 'ambiguous-entry'){'2026-10-15T23:00:10.0000007Z'}else{$entry}
    try{Set-StageOwnedPersistentServices -EnteredUtc $caseEntry -Services @($service)}catch{$refused=$true}
    Assert ($refused -and !$script:StageOwnedProcessState.PersistentConfigured -and @($script:StageOwnedProcessState.PersistentServices).Count -eq 0) ('Invalid preflight is atomic and refuses: '+$change)
}
Reset-PersistentFixture
$refused=$false
try{Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @((Admission),(Admission))}catch{$refused=$true}
Assert $refused 'Duplicate admission PIDs refuse.'
$refused=$false
try{Set-StageOwnedPersistentServices -EnteredUtc $entry -Services @((1..9|ForEach-Object{Admission}))}catch{$refused=$true}
Assert $refused 'An admission list is bounded to eight records.'
$refused=$false
try{$null=Get-StageOwnedPersistentClassification -Snapshot (Snapshot @() $false) -SupplementalProcessIds @() -TrackedProcessIds @() -Services @()}catch{$refused=$true}
Assert $refused 'Failed census cannot become a successful empty persistent classification.'
$service=Admission;$service|Add-Member NoteProperty enteredUtc $entry
$refused=$false
try{$null=Get-StageOwnedPersistentClassification -Snapshot (Snapshot @((Row 900 0 $child),(Row 900 0 $child))) -SupplementalProcessIds @(900) -TrackedProcessIds @() -Services @($service)}catch{$refused=$true}
Assert $refused 'Duplicate service rows in the same census refuse classification.'

"Test-StageOwnedProcess: $checks checks passed; synthetic snapshots only"
