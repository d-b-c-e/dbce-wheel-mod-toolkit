#Requires -Version 7.0
# Pure admission cases; no native input query, device, game or sleep.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Stage-OwnerIdle.ps1')
# Exercise the default guard independently of this workstation's owner policy.
$PSDefaultParameterValues['Assert-StageOwnerIdle:AllowOwnerActivity']=$false
$checks=0
foreach($sample in @(0,1,299.999,-1,[double]::NaN,[double]::PositiveInfinity,[double]::NegativeInfinity,$null,'300',$true)) {
    $refused=$false
    try { Assert-StageOwnerIdle -ReadSeconds { $sample } | Out-Null } catch { $refused=$true }
    if(!$refused){throw "Invalid/recent observation accepted: $sample"};$checks++
}
foreach($sample in @(300,300.001,3600)) {
    $result=Assert-StageOwnerIdle -ReadSeconds { $sample }
    if($result.idleSeconds -ne $sample -or $result.minimumSeconds -ne 300){throw 'Idle evidence changed'};$checks++
}
foreach($reader in @({throw 'native read failed'},{300;301})) {
    $refused=$false;try{Assert-StageOwnerIdle -ReadSeconds $reader | Out-Null}catch{$refused=$true}
    if(!$refused){throw 'Failed/ambiguous observation accepted'};$checks++
}
$result=Assert-StageOwnerIdle -AllowOwnerActivity $true -ReadSeconds { throw 'Idle should not be queried when the owner explicitly allows activity.' }
if (!$result.ownerActivityAllowed -or $result.source -ne 'owner-activity-allowed' -or $null -ne $result.idleSeconds) { throw 'Owner activity authorization was not reported honestly.' };$checks++
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('dbce-idle-policy-'+[Guid]::NewGuid().ToString('N')+'.json')
try {
    if(Test-StageOwnerActivityAllowed -PolicyPath $fixture){throw 'Missing policy allowed activity'};$checks++
    foreach($allowed in @($true,$false)) {
        @{schemaVersion=1;allowOwnerActivity=$allowed} | ConvertTo-Json | Set-Content -LiteralPath $fixture
        if((Test-StageOwnerActivityAllowed -PolicyPath $fixture) -ne $allowed){throw 'Policy value ignored'};$checks++
    }
    foreach($invalid in @('{"schemaVersion":1,"allowOwnerActivity":"true"}','{"schemaVersion":2,"allowOwnerActivity":true}','{}','invalid')) {
        Set-Content -LiteralPath $fixture -Value $invalid
        $refused=$false;try{Test-StageOwnerActivityAllowed -PolicyPath $fixture | Out-Null}catch{$refused=$true}
        if(!$refused){throw 'Invalid policy accepted'};$checks++
    }
} finally { Remove-Item -LiteralPath $fixture -ErrorAction SilentlyContinue; $PSDefaultParameterValues.Remove('Assert-StageOwnerIdle:AllowOwnerActivity') }
Write-Output "PASS: $checks owner-idle admission and activity-policy checks; no native/game activity."
