#Requires -Version 7.0
param([string]$Helper=(Join-Path $PSScriptRoot 'Stage-RunnerLifecycle.ps1'))
$ErrorActionPreference='Stop'
. $Helper
$checks=0;$jobs=@();$held=$null
function Check([bool]$Ok,[string]$Message){if(!$Ok){throw $Message};$script:checks++}
function Await([scriptblock]$Condition,[string]$Message){
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while(!(& $Condition)){if($clock.Elapsed.TotalSeconds -gt 5){throw $Message};Start-Sleep -Milliseconds 5}
}
$root=Join-Path ([IO.Path]::GetTempPath()) ('dbce-shared-text-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root)|Out-Null
$path=Join-Path $root 'status.json'
try {
    $phase='read/disposal'
    $first=Write-StageAtomicText $path '{"seq":0}' -DeadlineUtc ([DateTime]::UtcNow.AddSeconds(1))
    Check ($first.attempts -eq 1) 'First publication failed'
    Check ((Read-StageSharedText $path|ConvertFrom-Json).seq -eq 0) 'Shared read failed'
    $exclusive=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$exclusive.Dispose()
    Check $true 'Successful read handle disposed'
    $refused=$false;try{Read-StageSharedText $path -MaximumBytes 1|Out-Null}catch{$refused=$true}
    Check $refused 'Oversize read did not fail'
    $exclusive=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$exclusive.Dispose()
    Check $true 'Failed read handle disposed'
    $missing=$false;try{Read-StageSharedText (Join-Path $root 'absent.json')|Out-Null}catch{$missing=$true}
    Check $missing 'Missing file was silently treated as published'

    # Reproduce the old replacement failure while a legacy reader denies delete.
    $phase='legacy reader retry'
    $held=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    $oldTemp=Join-Path $root 'old-publication.tmp';[IO.File]::WriteAllText($oldTemp,'{"seq":1}')
    $oldFailure=$null;try{[IO.File]::Move($oldTemp,$path,$true)}catch{$oldFailure=$_.Exception.GetBaseException().HResult -band 65535}
    Check ($oldFailure -in @(5,32,33)) 'Legacy read did not reproduce a sharing/access failure'
    [IO.File]::Delete($oldTemp)
    $writer=Start-ThreadJob -ArgumentList $Helper,$path -ScriptBlock {
        param($helper,$path);$ErrorActionPreference='Stop';. $helper
        Write-StageAtomicText $path '{"seq":1}' -DeadlineUtc ([DateTime]::UtcNow.AddSeconds(3)) -RetryMilliseconds 1000
    };$jobs+=@($writer)
    Await {@(Get-ChildItem -LiteralPath $root -Filter 'status.json.tmp.*').Count -gt 0} 'Writer did not publish its temporary file'
    Start-Sleep -Milliseconds 40
    Check ($writer.State -eq 'Running') 'Writer did not wait for the conflicting reader'
    $held.Dispose();$held=$null
    $null=Wait-Job $writer -Timeout 3
    $retry=Receive-Job $writer -ErrorAction Stop
    Check ($writer.State -eq 'Completed' -and $retry.retries -gt 0) 'Transient replacement did not retry and complete'
    Check ((Read-StageSharedText $path|ConvertFrom-Json).seq -eq 1) 'Retried publication bytes differ'

    # A cooperative held handle keeps the old snapshot without blocking replace.
    $phase='cooperative held handle'
    $held=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $cooperative=Write-StageAtomicText $path '{"seq":2}' -DeadlineUtc ([DateTime]::UtcNow.AddSeconds(1))
    $oldReader=[IO.StreamReader]::new($held,[Text.Encoding]::UTF8,$true,1024,$true)
    try{$oldSnapshot=$oldReader.ReadToEnd()|ConvertFrom-Json}finally{$oldReader.Dispose()}
    $held.Dispose();$held=$null
    Check ($cooperative.attempts -eq 1 -and $oldSnapshot.seq -eq 1 -and (Read-StageSharedText $path|ConvertFrom-Json).seq -eq 2) 'Read/delete-safe snapshot replacement failed'

    # Exercise the actual shared reader concurrently with repeated publications.
    $phase='concurrent publication'
    $ready=Join-Path $root 'ready';$stop=Join-Path $root 'stop'
    $reader=Start-ThreadJob -ArgumentList $Helper,$path,$ready,$stop -ScriptBlock {
        param($helper,$path,$ready,$stop);$ErrorActionPreference='Stop';. $helper;$samples=0
        [IO.File]::WriteAllText($ready,'ready')
        while(!(Test-Path -LiteralPath $stop)){
            $record=Read-StageSharedText $path|ConvertFrom-Json -ErrorAction Stop
            if($record.seq -lt 2 -or $record.seq -gt 42){throw 'Torn or unrelated publication'}
            $samples++;Start-Sleep -Milliseconds 1
        }
        [pscustomobject]@{samples=$samples;final=(Read-StageSharedText $path|ConvertFrom-Json).seq}
    };$jobs+=@($reader)
    Await {Test-Path -LiteralPath $ready} 'Concurrent reader was not ready'
    for($seq=3;$seq -le 42;$seq++){
        Write-StageAtomicText $path ('{"seq":'+$seq+'}') -DeadlineUtc ([DateTime]::UtcNow.AddSeconds(1))|Out-Null
        Start-Sleep -Milliseconds 1
    }
    [IO.File]::WriteAllText($stop,'stop');$null=Wait-Job $reader -Timeout 3
    $observed=Receive-Job $reader -ErrorAction Stop
    Check ($reader.State -eq 'Completed' -and $observed.samples -gt 0 -and $observed.final -eq 42) 'Concurrent reader did not observe valid complete publications'

    $phase='persistent denial deadline'
    $unknown=Join-Path $root 'status.json.tmp.preserved';[IO.File]::WriteAllText($unknown,'unowned')
    $held=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    $clock=[Diagnostics.Stopwatch]::StartNew();$denied=$false
    try{Write-StageAtomicText $path '{"seq":99}' -DeadlineUtc ([DateTime]::UtcNow.AddMilliseconds(80)) -RetryMilliseconds 1000|Out-Null}catch{$denied=$true}
    $held.Dispose();$held=$null
    Check ($denied -and $clock.ElapsedMilliseconds -lt 500) 'Persistent sharing denial exceeded the caller deadline'
    Check ((Read-StageSharedText $path|ConvertFrom-Json).seq -eq 42) 'Failed publication changed the prior target'
    Check ([IO.File]::ReadAllText($unknown) -ceq 'unowned') 'Unknown temporary file was changed'
    Check (@(Get-ChildItem -LiteralPath $root -Filter 'status.json.tmp.*').Count -eq 1) 'Owned failed temporary file was not disposed'
    [pscustomobject]@{schema='dbce.shared-text-check@1';status='PASS';checks=$checks;legacyFailureCode=$oldFailure;
        transientRetries=$retry.retries;concurrentSamples=$observed.samples;concurrentPublications=40;
        persistentDenialMilliseconds=$clock.ElapsedMilliseconds;devicesUsed=$false;gameLaunched=$false}|ConvertTo-Json
} catch {
    throw ('Shared IO fixture phase '+$phase+': '+$_.Exception.Message+' '+$_.ScriptStackTrace)
} finally {
    if($held){$held.Dispose()}
    foreach($job in $jobs){if($job.State -notin @('Completed','Failed','Stopped')){Stop-Job $job};Remove-Job $job -Force}
    $safeRoot=[IO.Path]::GetFullPath($root)
    $safeParent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if(!$safeRoot.StartsWith($safeParent,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($safeRoot) -notmatch '^dbce-shared-text-[a-f0-9]{32}$'){throw 'Fixture cleanup path refused'}
    Remove-Item -LiteralPath $safeRoot -Recurse -Force
}
