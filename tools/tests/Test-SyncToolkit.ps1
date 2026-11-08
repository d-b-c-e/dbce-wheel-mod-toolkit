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
 # Source parts: shared/ with the Input and Shim projects' sources as shared/input and shared/shim (no csproj, no bin), unity, installer.
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts shared }
 New-Item -ItemType Directory -Path (Join-Path $repo 'shared/ui'),(Join-Path $repo 'dotnet/Dbce.Wheel.Input/bin/Release'),(Join-Path $repo 'dotnet/Dbce.Wheel.Shim'),(Join-Path $repo 'unity/Runtime'),(Join-Path $repo 'installer') -Force | Out-Null
 Set-Content -LiteralPath (Join-Path $repo 'shared/ui/F6Style.cs') 'f6'
 Set-Content -LiteralPath (Join-Path $repo 'dotnet/Dbce.Wheel.Input/ControlBinding.cs') 'binding'
 Set-Content -LiteralPath (Join-Path $repo 'dotnet/Dbce.Wheel.Input/Dbce.Wheel.Input.csproj') 'project'
 Set-Content -LiteralPath (Join-Path $repo 'dotnet/Dbce.Wheel.Shim/ShimSnapshot.cs') 'snapshot'
 Set-Content -LiteralPath (Join-Path $repo 'dotnet/Dbce.Wheel.Shim/Dbce.Wheel.Shim.csproj') 'project'
 Set-Content -LiteralPath (Join-Path $repo 'dotnet/Dbce.Wheel.Input/bin/Release/Dbce.Wheel.Input.dll') 'built'
 Set-Content -LiteralPath (Join-Path $repo 'unity/Dbce.Unity.props') 'props'
 Set-Content -LiteralPath (Join-Path $repo 'unity/Runtime/ModDescriptor.cs') 'descriptor'
 Set-Content -LiteralPath (Join-Path $repo 'installer/README.md') 'installer'
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts shared,unity,installer
 $lines=@(Get-Content -LiteralPath (Join-Path $dest 'MANIFEST.txt'))
 foreach ($want in 'shared/ui/F6Style.cs','shared/input/ControlBinding.cs','shared/shim/ShimSnapshot.cs','unity/Dbce.Unity.props','unity/Runtime/ModDescriptor.cs','installer/README.md','profiles/force-profiles.ini') {
  Assert (@($lines | Where-Object { $_ -match ('^[A-F0-9]{64}  ' + [regex]::Escape($want) + '$') }).Count -eq 1) "Manifest lacks $want"
 }
 Assert (@($lines | Where-Object { $_ -match '\.csproj$|/bin/' }).Count -eq 0) 'Project file or build output delivered as source'
 Assert ($lines[0] -match 'selected parts: shared,unity,installer;') 'Manifest header lacks the source parts'
 $vendored=Join-Path $dest 'shared/input/ControlBinding.cs'
 Set-Content -LiteralPath $vendored 'edited in the game'
 Set-Content -LiteralPath (Join-Path $repo 'dotnet/Dbce.Wheel.Input/ControlBinding.cs') 'binding two'
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts shared }
 Assert ((Get-Content -LiteralPath $vendored -Raw).Trim() -eq 'edited in the game') 'Edited vendored source was replaced'
 Set-Content -LiteralPath $vendored 'binding'
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts shared
 Assert ((Get-Content -LiteralPath $vendored -Raw).Trim() -eq 'binding two') 'Unedited vendored source was not updated'
 # triple: the sibling triple checkout's core sources (no csproj, no bin) and contracts, its revision in the header.
 $triple=Join-Path $caseRoot 'dbce-triple-screen-toolkit'
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts triple }
 New-Item -ItemType Directory -Path (Join-Path $triple 'src/Dbce.TripleScreen.Core/bin/Release'),(Join-Path $triple 'contracts') -Force | Out-Null
 Set-Content -LiteralPath (Join-Path $triple 'src/Dbce.TripleScreen.Core/Vector3d.cs') 'vector'
 Set-Content -LiteralPath (Join-Path $triple 'src/Dbce.TripleScreen.Core/Dbce.TripleScreen.Core.csproj') 'project'
 Set-Content -LiteralPath (Join-Path $triple 'src/Dbce.TripleScreen.Core/bin/Release/Dbce.TripleScreen.Core.dll') 'built'
 Set-Content -LiteralPath (Join-Path $triple 'contracts/README.md') 'contracts'
 & git -C $triple init -q; & git -C $triple add .
 & git -C $triple -c user.name=SyncTest -c user.email=sync-test@example.invalid commit -qm fixture
 if ($LASTEXITCODE -ne 0) { throw 'Triple fixture git failed' }
 Reject { & $scriptPath -Version v0.0.0 -Destination $dest -Parts triple }
 & $scriptPath -LocalRepo $repo -Destination $dest -Parts triple
 $lines=@(Get-Content -LiteralPath (Join-Path $dest 'MANIFEST.txt'))
 foreach ($want in 'triple/core/Vector3d.cs','triple/contracts/README.md','shared/input/ControlBinding.cs') {
  Assert (@($lines | Where-Object { $_ -match ('^[A-F0-9]{64}  ' + [regex]::Escape($want) + '$') }).Count -eq 1) "Manifest lacks $want"
 }
 Assert (@($lines | Where-Object { $_ -match '^[A-F0-9]{64}  triple/.*(\.csproj$|/bin/)' }).Count -eq 0) 'Triple project file or build output delivered'
 Assert ($lines[0] -match 'selected parts: triple; toolkit local\+[a-f0-9]{40}\.(clean|dirty); triple local\+[a-f0-9]{40}\.clean$') 'Manifest header lacks the triple revision'
 # Inject a failure at the second rename. All other filesystem operations are real.
 $global:SyncMoveCalls=0
 function global:Move-Item { param([string]$LiteralPath,[string]$Destination)
   $global:SyncMoveCalls++
   if ($global:SyncMoveCalls -eq 2) { throw 'Injected staged-tree commit failure' }
   Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
 }
 try { Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native } }
 finally { Remove-Item Function:/Move-Item; Remove-Variable SyncMoveCalls -Scope Global }
 # Fail after a real staging copy has succeeded; the destination must stay byte-identical.
 $global:SyncCopyCalls=0
 function global:Copy-Item { param([string]$LiteralPath,[string]$Destination,[switch]$Recurse,[switch]$Force)
   $global:SyncCopyCalls++
   if ($global:SyncCopyCalls -eq 2) { throw 'Injected staged copy failure' }
   Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
 }
 try { Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native } }
 finally { Remove-Item Function:/Copy-Item; Remove-Variable SyncCopyCalls -Scope Global }
 # An existing but empty source is different from a missing source, and must fail too.
 New-Item -ItemType Directory -Path (Join-Path $repo 'knowledge') | Out-Null
 Reject { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native,knowledge }
 # Locked original file blocks staging; the pin and whole old tree survive.
 $locked=[IO.File]::Open((Join-Path $dest 'native/WheelFfb.dll'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 try {
  $failed=$false; try { & $scriptPath -LocalRepo $repo -Destination $dest -Parts native } catch { $failed=$true }
  Assert $failed 'Locked file was accepted'
 } finally { $locked.Dispose() }
 # -Check changes nothing: it passes on the synced tree and fails on an edited or unlisted file.
 Reject { & $scriptPath -Check -Destination $dest }   # the -Force backup above is an unlisted local edit
 Get-ChildItem -LiteralPath $dest -Recurse -File -Filter '*.local-*' | Remove-Item
 & $scriptPath -Check -Destination $dest | Out-Null
 $vendored=Join-Path $dest 'native/WheelFfb.dll'; $good=[IO.File]::ReadAllBytes($vendored)
 Add-Content -LiteralPath $vendored 'edit'; Reject { & $scriptPath -Check -Destination $dest }
 [IO.File]::WriteAllBytes($vendored,$good); Set-Content -LiteralPath (Join-Path $dest 'stray.txt') 'x'
 Reject { & $scriptPath -Check -Destination $dest }
 Remove-Item -LiteralPath (Join-Path $dest 'stray.txt'); & $scriptPath -Check -Destination $dest | Out-Null
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
