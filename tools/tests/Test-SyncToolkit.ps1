$ErrorActionPreference='Stop'
$scriptPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Sync-Toolkit.ps1'))
$caseRoot=Join-Path ([IO.Path]::GetTempPath()) ('sync-test-'+[guid]::NewGuid().ToString('N'))
$repo=Join-Path $caseRoot 'repo'; $dest=Join-Path $caseRoot 'consumer/toolkit'
$script:checks=0
function Assert($ok,$message) { $script:checks++; if (-not $ok) { throw $message } }
function Snapshot { if (Test-Path -LiteralPath $dest) { return (@(Get-ChildItem -LiteralPath $dest -File -Recurse | Sort-Object FullName | ForEach-Object { $_.FullName.Substring($dest.Length)+':'+(Get-FileHash -LiteralPath $_.FullName).Hash }) -join "`n") }; return '(absent)' }
function Reject([scriptblock]$action) { $before=Snapshot; $failed=$false; try { & $action } catch { $failed=$true }; Assert $failed 'Expected sync failure'; Assert ((Snapshot) -eq $before) 'Failed sync changed destination' }
try {
 New-Item -ItemType Directory -Path (Join-Path $repo 'native/wheelffb/build'),(Join-Path $repo 'profiles'),(Join-Path $repo 'tools/a'),(Join-Path $repo 'tools/b') -Force | Out-Null
 Set-Content -LiteralPath (Join-Path $repo 'native/wheelffb/build/WheelFfb.dll') 'native-one'
 Set-Content -LiteralPath (Join-Path $repo 'profiles/force-profiles.ini') 'profile-one'
 Set-Content -LiteralPath (Join-Path $repo 'tools/a/same.txt') 'a'
 Set-Content -LiteralPath (Join-Path $repo 'tools/b/same.txt') 'b'
 & git -C $repo init -q
 & git -C $repo add .
 & git -C $repo -c user.name=SyncTest -c user.email=sync-test@example.invalid commit -qm fixture
 if ($LASTEXITCODE -ne 0) { throw 'Fixture git failed' }
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native,unknown }
 Assert (-not (Test-Path -LiteralPath $dest)) 'Preflight created destination'
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts native,profiles
 Assert ((Get-Content -LiteralPath (Join-Path $dest 'VERSION') -First 1) -match '^local\+[a-f0-9]{40}\.clean$') 'Wrong local identity'
 $profile=Join-Path $dest 'profiles/force-profiles.ini'
 Set-Content -LiteralPath $profile 'my tune'
 Set-Content -LiteralPath (Join-Path $repo 'native/wheelffb/build/WheelFfb.dll') 'native-two'
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native,profiles }
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native,dotnet }
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts native,profiles -Force
 $backup=@(Get-ChildItem -LiteralPath (Split-Path -Parent $profile) -Filter '*.local-*')
 Assert ($backup.Count -eq 1 -and (Get-Content -LiteralPath $backup[0].FullName -Raw).Trim() -eq 'my tune') 'Force lost tune backup'
 $manifest=Get-Content -LiteralPath (Join-Path $dest 'MANIFEST.txt') | Where-Object { $_ -match 'profiles/force-profiles.ini$' }
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts native
 Assert ((Get-Content -LiteralPath (Join-Path $dest 'MANIFEST.txt')) -contains $manifest) 'Partial sync dropped other manifest entries'
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts tools
 Assert ((Get-Content -LiteralPath (Join-Path $dest 'tools/a/same.txt') -Raw).Trim() -eq 'a') 'Nested path a lost'
 Assert ((Get-Content -LiteralPath (Join-Path $dest 'tools/b/same.txt') -Raw).Trim() -eq 'b') 'Nested path b lost'
 # Inject a failure at the second rename. All other filesystem operations are real.
 $global:SyncMoveCalls=0
 function global:Move-Item { param([string]$LiteralPath,[string]$Destination)
   $global:SyncMoveCalls++
   if ($global:SyncMoveCalls -eq 2) { throw 'Injected staged-tree commit failure' }
   Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
 }
 try { Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native } }
 finally { Remove-Item Function:/Move-Item; Remove-Variable SyncMoveCalls -Scope Global }
 # Locked original file blocks staging; the pin and whole old tree survive.
 $locked=[IO.File]::Open((Join-Path $dest 'native/WheelFfb.dll'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 try {
  $failed=$false; try { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native } catch { $failed=$true }
  Assert $failed 'Locked file was accepted'
 } finally { $locked.Dispose() }
 $before=Snapshot
 $file=Join-Path $repo 'native/wheelffb/build/WheelFfb.dll'
 Remove-Item -LiteralPath $file
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native }
 Assert ((Snapshot) -eq $before) 'Missing required DLL modified tree'
 # Unmanifested differing destination files are edits even on a first sync.
 Remove-Item -LiteralPath (Join-Path $dest 'MANIFEST.txt')
 Set-Content -LiteralPath $file 'replacement'
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native }
 [ordered]@{status='passed';assertions=$script:checks} | ConvertTo-Json -Compress
} finally {
 $absolute=[IO.Path]::GetFullPath($caseRoot)
 $tempParent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
 if ((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'sync-test-*') { throw 'Unsafe test cleanup path' }
 if (Test-Path -LiteralPath $absolute) { Remove-Item -LiteralPath $absolute -Recurse -Force }
}
