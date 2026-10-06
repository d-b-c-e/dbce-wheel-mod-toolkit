#Requires -Version 7.0
# Disposable files/fake process observations only; no games, registry or devices.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Stage-RunnerLifecycle.ps1')
$checks=0
function Assert($ok,[string]$message) { if (!$ok) { throw $message }; $script:checks++ }
$scratch=Join-Path ([IO.Path]::GetTempPath()) ('stage-runner-tests-'+[guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $scratch
$oldLocation=Get-Location
$oldCwd=[Environment]::CurrentDirectory
try {
    $location=Join-Path $scratch 'PowerShell location'; $cwd=Join-Path $scratch 'process cwd'
    $null=New-Item -ItemType Directory -Path $location,$cwd
    Set-Location $location; [Environment]::CurrentDirectory=$cwd
    Assert ((Resolve-StageFileSystemPath 'new result') -eq (Join-Path $location 'new result')) 'Relative path followed process CWD'
    Assert ((Resolve-StageFileSystemPath (Join-Path $cwd 'absolute')) -eq (Join-Path $cwd 'absolute')) 'Absolute path changed'
    Assert ((Resolve-StageFileSystemPath './literal[1]') -eq (Join-Path $location 'literal[1]')) 'Literal path expanded'
    $refused=$false; try { Resolve-StageFileSystemPath 'Env:PATH' | Out-Null } catch { $refused=$true }
    Assert $refused 'Non-filesystem path accepted'
    $helper=Join-Path $PSScriptRoot 'Stage-RunnerLifecycle.ps1'
    $job=Start-Job -ArgumentList $helper,$location,$cwd -ScriptBlock {
        param($helper,$location,$cwd)
        . $helper; Set-Location $location; [Environment]::CurrentDirectory=$cwd
        Resolve-StageFileSystemPath 'job result'
    }
    try { $answer=@(Receive-Job $job -Wait -ErrorAction Stop); Assert ($job.State -eq 'Completed' -and $answer.Count -eq 1 -and $answer[0] -eq (Join-Path $location 'job result')) 'Start-Job path follows wrong directory' }
    finally { Remove-Job $job -Force }

    function New-FakeProcess([bool]$exitDuringRead=$false,[bool]$unreadable=$false) {
        $p=[pscustomobject]@{Id=42;Exited=$false;ExitDuringRead=$exitDuringRead;Unreadable=$unreadable;Disposed=$false}
        $p | Add-Member ScriptProperty HasExited { $this.Exited }
        $p | Add-Member ScriptProperty StartTime { if ($this.ExitDuringRead) { $this.Exited=$true; throw 'closed during property read' }; [DateTime]'2026-10-06T00:00:00Z' }
        $p | Add-Member ScriptProperty MainModule { if ($this.Unreadable) { throw 'access unavailable' }; [pscustomobject]@{FileName='C:\fixture\game.exe'} }
        $p | Add-Member ScriptMethod Dispose { $this.Disposed=$true }
        $p
    }
    # Shadows enumeration only inside this test script; production code is unchanged.
    function Get-Process { param($Name,$ErrorAction) $script:fakeProcess }
    $script:fakeProcess=New-FakeProcess $true
    $snapshot=@(Get-StageProcessSnapshot game)
    Assert ($snapshot.Count -eq 0 -and $fakeProcess.Disposed) 'Exit during identity read misclassified as replacement'
    $script:fakeProcess=New-FakeProcess $false $true
    $snapshot=@(Get-StageProcessSnapshot game)
    Assert ($snapshot.Count -eq 1 -and $snapshot[0].state -eq 'Unverifiable') 'Unavailable identity treated as closure'
    $script:fakeProcess=New-FakeProcess
    $snapshot=@(Get-StageProcessSnapshot game)
    $ticks=$snapshot[0].startedUtcTicks
    Assert ((Get-StageProcessDisposition $snapshot 42 $ticks 'c:\FIXTURE\game.exe') -eq 'Owned') 'Exact identity rejected'
    Assert ((Get-StageProcessDisposition $snapshot 42 ($ticks+1) 'C:\fixture\game.exe') -eq 'Changed') 'PID reuse accepted'
    Assert ((Get-StageProcessDisposition $snapshot 43 $ticks 'C:\fixture\game.exe') -eq 'Changed') 'Other PID accepted'
    Assert ((Get-StageProcessDisposition $snapshot 42 $ticks 'C:\other\game.exe') -eq 'Changed') 'Other image accepted'
    Assert ((Get-StageProcessDisposition @($snapshot[0],$snapshot[0]) 42 $ticks 'C:\fixture\game.exe') -eq 'Changed') 'Multiple processes accepted'
    Assert ((Get-StageProcessDisposition @() 42 $ticks 'C:\fixture\game.exe') -eq 'Closed') 'Empty snapshot not closed'
    Remove-Item Function:Get-Process

    $owner=Join-Path $scratch 'owner.cfg'
    $original=[Text.UTF8Encoding]::new($true).GetBytes("[Triple]`r`nMode = Auto`r`n")
    function Run-CleanupFixture([string]$mode,[string]$failure='') {
        [IO.File]::WriteAllText($owner,"[Triple]`nSpanSeparateMonitors = true`nSpanApplied = 7680x1440")
        $state=[pscustomobject]@{polls=0;calls=[Collections.Generic.List[string]]::new()}
        $wait=if ($mode -in @('alive','unknown')) { 0 } else { 1 }
        $result=Complete-StageSession -ProcessName fixture -WaitSeconds $wait -PollMilliseconds 1 -SnapshotProvider {
            param($name)
            $state.polls++
            if ($mode -eq 'unknown') { throw 'enumeration unavailable' }
            if ($mode -eq 'alive' -or ($mode -eq 'closing' -and $state.polls -lt 3)) { [pscustomobject]@{state='Running';processId=999} }
        } -CollectEvidence { $state.calls.Add('evidence'); if ($failure -eq 'evidence') { throw 'disk failure' } } `
          -StopSupervisor { $state.calls.Add('supervisor'); if ($failure -eq 'supervisor') { throw 'watchdog failure' } } `
          -DisarmRequest { $state.calls.Add('disarm'); if ($failure -eq 'disarm') { throw 'command changed' } } `
          -RestoreOwner { $state.calls.Add('restore'); if ($failure -eq 'restore') { throw 'backup mismatch' }; [IO.File]::WriteAllBytes($owner,$original) } `
          -ReleaseLease { $state.calls.Add('lease') }
        [pscustomobject]@{result=$result;calls=@($state.calls);polls=$state.polls}
    }
    $r=Run-CleanupFixture closing
    Assert ($r.result.closed -and $r.result.restored -and $r.result.leaseReleased -and $r.polls -eq 4) 'Closing process stranded owner state'
    Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes($owner)) -ceq [Convert]::ToBase64String($original)) 'Span bytes not restored exactly'
    Assert (($r.calls -join ',') -eq 'evidence,supervisor,disarm,restore,lease') 'Cleanup order differs'
    foreach ($mode in 'alive','unknown') {
        $r=Run-CleanupFixture $mode
        Assert (!$r.result.closed -and !$r.result.restored -and !$r.result.leaseReleased -and ($r.calls -join ',') -eq 'evidence') 'Live/replacement/unknown process caused state mutation'
    }
    foreach ($failure in 'evidence','supervisor') {
        $r=Run-CleanupFixture closed $failure
        Assert ($r.result.restored -and $r.result.leaseReleased -and $r.result.errors.Count -eq 1) 'Ancillary failure bypassed restoration'
    }
    foreach ($failure in 'restore','disarm') {
        $r=Run-CleanupFixture closed $failure
        Assert (!$r.result.leaseReleased -and $r.result.errors.Count -eq 1 -and 'lease' -notin $r.calls) 'Failed recovery released ownership'
    }
    # Exercise the production finalizer from an exceptional path, not only success.
    $exceptionSeen=$false; $r=$null
    try { throw 'identity changed' } catch { $exceptionSeen=$true } finally { $r=Run-CleanupFixture closing }
    Assert ($exceptionSeen -and $r.result.restored -and $r.result.leaseReleased) 'Exception path failed cleanup'
    Write-Output "PASS: $checks runner lifecycle checks; no game, registry or device access."
} finally {
    Set-Location $oldLocation; [Environment]::CurrentDirectory=$oldCwd
    $full=[IO.Path]::GetFullPath($scratch)
    $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if (!$full.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $full -Leaf) -notlike 'stage-runner-tests-*') { throw 'Unsafe fixture cleanup path' }
    Remove-Item -LiteralPath $full -Recurse -Force
}
