#Requires -Version 5.1
# Isolated HKCU fixture only; no game settings, launch, display or hardware access.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage-Registry.ps1')
Initialize-StageRegistry
$id = [guid]::NewGuid().ToString('N')
$sub = 'Software\Dbce\StageRegistryTests\' + $id
$wrong = 'Software\Dbce\StageRegistryTests\wrong-' + $id
$directory = Join-Path ([IO.Path]::GetTempPath()) ('stage-registry-' + $id)
$null = [IO.Directory]::CreateDirectory($directory)
$key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($sub); $key.Dispose()
$checks = 0
function Check($Condition,[string]$Message) { if (!$Condition) { throw $Message }; $script:checks++ }
try {
    $absent = Save-StageRegistry -SubKey $wrong -Path (Join-Path $directory 'absent.json')
    Check (!$absent.exists -and (Test-StageRegistry $wrong $absent.path $absent.sha256).equal) 'Absent key captured'
    [DbceStageRegistry]::CreateLeaf($wrong)
    [DbceStageRegistry]::Write($wrong,'test-created',3,'AA==')
    $one = Save-StageRegistry $wrong (Join-Path $directory 'one.json')
    Check ($one.count -eq 1 -and (Test-StageRegistry $wrong $one.path $one.sha256).equal) 'One-value inventory retains array shape'
    Check ((Restore-StageRegistry $wrong $absent.path $absent.sha256).equal -and ![DbceStageRegistry]::ExistsLeaf($wrong)) 'Run-created leaf restored to absence'
    $empty = Save-StageRegistry -SubKey $sub -Path (Join-Path $directory 'empty.json')
    Check ($empty.exists -and $empty.count -eq 0 -and (Test-StageRegistry $sub $empty.path $empty.sha256).equal) 'Empty differs from absent'
    [DbceStageRegistry]::Write($sub,'before',4,[Convert]::ToBase64String([BitConverter]::GetBytes([uint32]4294967295)))
    [DbceStageRegistry]::Write($sub,('long-'+('x'*280)),3,[Convert]::ToBase64String([byte[]](0,1,128,255)))
    [DbceStageRegistry]::Write($sub,'after',4,[Convert]::ToBase64String([BitConverter]::GetBytes([uint32]3)))
    [DbceStageRegistry]::Write($sub,'',1,[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes("default`0")))
    [DbceStageRegistry]::Write($sub,'empty',3,'')
    [DbceStageRegistry]::Write($sub,'multisz',7,[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes("one`0two`0`0")))
    [DbceStageRegistry]::Write($sub,'2026-10-08T01:00:00Z',4,[Convert]::ToBase64String([BitConverter]::GetBytes([uint64]4294967295)))
    $receipt = Save-StageRegistry -SubKey $sub -Path (Join-Path $directory 'before.json')
    Check ($receipt.count -eq 7) 'All names captured, including long/default/date names and eight-byte DWORD'
    Check ((Test-StageRegistry -SubKey $sub -Path $receipt.path -ExpectedSha256 $receipt.sha256).equal) 'Initial snapshot matches'
    $refused=$false; try { Save-StageRegistry -SubKey $sub -Path $receipt.path } catch { $refused=$true }; Check $refused 'Snapshot overwrite refused'
    [DbceStageRegistry]::Write($sub,'before',3,'AA==')
    [DbceStageRegistry]::Delete($sub,'after')
    [DbceStageRegistry]::Write($sub,'extra',3,'AQ==')
    $delta = Test-StageRegistry -SubKey $sub -Path $receipt.path -ExpectedSha256 $receipt.sha256
    Check (!$delta.equal -and $delta.changed.Count -eq 2 -and $delta.extra.Count -eq 1) 'Type/bytes, missing and extra detected'
    $refused=$false; try { Restore-StageRegistry -SubKey $wrong -Path $receipt.path -ExpectedSha256 $receipt.sha256 } catch { $refused=$true }; Check $refused 'Wrong target refused before writes'
    $refused=$false; try { Restore-StageRegistry -SubKey $sub -Path $receipt.path -ExpectedSha256 ('0'*64) } catch { $refused=$true }; Check $refused 'Wrong hash refused before writes'
    Check ([DbceStageRegistry]::Read($sub,'before').type -eq 3) 'Refusals left fixture unchanged'
    $restored = Restore-StageRegistry -SubKey $sub -Path $receipt.path -ExpectedSha256 $receipt.sha256
    Check ($restored.equal -and $restored.currentCount -eq 7) 'All native names/types/raw bytes restored'
    $child = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($sub+'\nested'); $child.Dispose()
    $refused=$false; try { Save-StageRegistry $sub (Join-Path $directory 'partial.json') } catch { $refused=$true }; Check $refused 'Nested key snapshot refused'
    $refused=$false; try { Restore-StageRegistry $sub $empty.path $empty.sha256 } catch { $refused=$true }; Check $refused 'Restore refuses nested key before changing values'
    Check ([DbceStageRegistry]::Names($sub).Count -eq 7) 'Nested-key refusal leaves all values intact'
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($sub+'\nested',$false)
    [DbceStageRegistry]::Delete($sub,'2026-10-08T01:00:00Z')
    $restored = Restore-StageRegistry $sub $empty.path $empty.sha256
    Check ($restored.equal -and $restored.currentExists -and $restored.currentCount -eq 0) 'Empty original restored without deleting key'
    [DbceStageRegistry]::DeleteEmptyLeaf($sub)
    Check ((Restore-StageRegistry $sub $receipt.path $receipt.sha256).equal) 'Missing original leaf recreated and raw values verified'
    # On affected Windows builds reg.exe succeeds but stops at the long name. Do
    # not require the Windows defect on every host; our inventory must stay exact.
    & reg.exe export ('HKCU\'+$sub) (Join-Path $directory 'reg-export.reg') /y | Out-Null
    $exportExit=$LASTEXITCODE
    $exported=@(Get-Content -LiteralPath (Join-Path $directory 'reg-export.reg') | Where-Object { $_ -match '^("|@)' }).Count
    Check ((Test-StageRegistry -SubKey $sub -Path $receipt.path -ExpectedSha256 $receipt.sha256).equal) 'Export probe never changed source'
    "PASS $checks raw-registry checks; reg.exe exit=$exportExit exported=$exported of 7 values. Evidence: $directory"
} finally {
    # Only the exact randomly named fixture created above; no recursive deletion.
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($sub+'\nested',$false)
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($sub,$false)
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($wrong,$false)
}
