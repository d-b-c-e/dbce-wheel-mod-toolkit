#Requires -Version 7.0
# Pure admission cases; no native input query, device, game or sleep.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Stage-OwnerIdle.ps1')
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
Write-Output "PASS: $checks owner-idle admission checks; no native/game activity."
