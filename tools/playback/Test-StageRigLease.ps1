#Requires -Version 7.0
# Real file ownership checks in a new temporary directory; never touches the rig slot.
[CmdletBinding()]
param([string]$Results = (Join-Path ([IO.Path]::GetTempPath()) ('dbce-stage-lease-' + [guid]::NewGuid().ToString('N'))))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage-RigLease.ps1')
if (Test-Path -LiteralPath $Results) { throw 'Choose a new fixture directory' }
$path = Join-Path $Results 'test-slot.txt'
$first = Enter-StageRigLease -Path $path -Owner 'fixture first' -Purpose 'acquire'
$hash = (Get-FileHash -LiteralPath $path).Hash
$refused = $false
try { Enter-StageRigLease -Path $path -Owner 'fixture second' -Purpose 'conflict' | Out-Null } catch { $refused = $_.Exception.Message -like 'Shared rig test slot is held:*' }
if (!$refused -or (Get-FileHash -LiteralPath $path).Hash -ne $hash) { throw 'Fresh owner was not preserved' }
if (!(Exit-StageRigLease $first) -or (Test-Path -LiteralPath $path)) { throw 'Owned release failed' }
$lease = Enter-StageRigLease -Path $path -Owner 'fixture first' -Purpose 'replacement'
[IO.File]::WriteAllText($path, 'claude other-game fresh owner')
if (Exit-StageRigLease $lease) { throw 'Another owner was released' }
if ([IO.File]::ReadAllText($path) -cne 'claude other-game fresh owner') { throw 'Replacement owner was changed' }
[IO.File]::SetLastWriteTimeUtc($path, [DateTime]::UtcNow.AddHours(-3))
$stale = Enter-StageRigLease -Path $path -Owner 'fixture first' -Purpose 'expired'
if ($stale.previous -cne 'claude other-game fresh owner') { throw 'Expired lease evidence missing' }
if (!(Exit-StageRigLease $stale)) { throw 'Expired-slot successor release failed' }
[IO.File]::WriteAllText($path, '')
$refused = $false
try { Enter-StageRigLease -Path $path -Owner 'fixture second' -Purpose 'empty fresh' | Out-Null } catch { $refused = $_.Exception.Message -like 'Shared rig test slot is held:*' }
if (!$refused -or (Get-Item -LiteralPath $path).Length -ne 0) { throw 'Fresh empty slot was stolen' }
[IO.File]::SetLastWriteTimeUtc($path, [DateTime]::UtcNow.AddHours(-3))
$locked = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $refused = $false
    try { Enter-StageRigLease -Path $path -Owner 'fixture second' -Purpose 'locked' | Out-Null } catch [IO.IOException] { $refused = $true }
    if (!$refused) { throw 'Locked slot was stolen' }
} finally { $locked.Dispose() }
$last = Enter-StageRigLease -Path $path -Owner 'fixture final' -Purpose 'cleanup'
if (!(Exit-StageRigLease $last) -or (Exit-StageRigLease $last)) { throw 'Final release was not idempotent' }
Write-Host "PASS acquisition, fresh refusal, exact release, replacement preservation, expiry evidence, fresh-empty refusal, file lock and repeated release. Evidence: $Results"
