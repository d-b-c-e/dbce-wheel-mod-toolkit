# tools\powershell\RegistryState.ps1 on a throwaway HKCU key: every value type survives byte for byte, including the
# ones `reg export` loses or PowerShell cannot read (an 8-byte REG_DWORD, a long binary map, an empty value, a value
# with an odd name); restore puts back changed and deleted values, removes only what the run added, and never deletes
# the key. No game state is touched.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\powershell\RegistryState.ps1')
$name = 'DBCE-RegistryStateTest-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$key = "HKCU:\Software\$name"
$work = Join-Path ([IO.Path]::GetTempPath()) $name
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
function Raw([string]$sub, [string]$value, [int]$type, [byte[]]$bytes) {
    $h = [IntPtr]::Zero; $d = 0
    if ([Dbce.RawRegistry]::RegCreateKeyExW($script:RegHkcu, $sub, 0, $null, 0, 0xF003F, [IntPtr]::Zero, [ref]$h, [ref]$d) -ne 0) { throw 'create failed' }
    try { if ([Dbce.RawRegistry]::RegSetValueExW($h, $value, 0, $type, $bytes, $bytes.Length) -ne 0) { throw 'set failed' } } finally { [void][Dbce.RawRegistry]::RegCloseKey($h) }
}
try {
    [void][IO.Directory]::CreateDirectory($work)
    $sub = "Software\$name"
    $long = [byte[]]::new(70000); (New-Object Random 7).NextBytes($long)
    Raw $sub 'Text' 1 ([Text.Encoding]::Unicode.GetBytes("caf`u{00E9} map`0"))
    Raw $sub 'settings:cameraType' 4 ([byte[]](1, 0, 0, 0, 2, 0, 0, 0))          # a malformed 8-byte DWORD, as DRIVE writes
    Raw $sub 'RewiredControllerMap' 3 $long                                       # longer than reg.exe handles reliably
    Raw $sub 'Empty' 3 ([byte[]]::new(0))
    Raw $sub 'Odd name = "quoted" \ slash' 1 ([Text.Encoding]::Unicode.GetBytes("x`0"))
    Raw $sub '' 1 ([Text.Encoding]::Unicode.GetBytes("default`0"))                # the key's default value
    Raw "$sub\Child\Deeper" 'Number' 11 ([BitConverter]::GetBytes([long]123456789012))

    $file = Join-Path $work 'state.json'
    $saved = Save-RegistryState -Key $key -Path $file
    Check ($saved -match '7 value') "save records every value ($saved)"
    Check (Compare-RegistryState -Path $file) 'the live key equals a fresh snapshot'
    $refused = $false; try { Save-RegistryState -Key $key -Path $file | Out-Null } catch { $refused = $true }
    Check $refused 'a snapshot is never overwritten'

    # A run changes, deletes and adds things.
    Raw $sub 'Text' 1 ([Text.Encoding]::Unicode.GetBytes("changed`0"))
    Raw $sub 'settings:cameraType' 4 ([byte[]](9, 0, 0, 0))
    Remove-ItemProperty -LiteralPath $key -Name 'RewiredControllerMap'
    Raw $sub 'AddedByRun' 1 ([Text.Encoding]::Unicode.GetBytes("new`0"))
    Raw "$sub\AddedKey\Inner" 'v' 4 ([byte[]](1, 0, 0, 0))
    Remove-Item -LiteralPath "$key\Child" -Recurse
    $diff = @(Compare-RegistryState -Path $file -List)
    Check (-not (Compare-RegistryState -Path $file) -and $diff.Count -eq 6) "the changes are seen ($($diff.Count): changed 2, missing value 1, added value 1, missing key 1, added key 1)"

    $restored = Restore-RegistryState -Path $file
    Check ($restored -match '6 difference') "restore reports what it put back ($restored)"
    Check (Compare-RegistryState -Path $file) 'after restore the live key equals the snapshot byte for byte'
    Check (Test-Path -LiteralPath $key) 'the key itself was never deleted'
    Check (-not (Test-Path -LiteralPath "$key\AddedKey")) 'a subkey the run added is gone'
    $h = [IntPtr]::Zero; [void][Dbce.RawRegistry]::RegOpenKeyExW($script:RegHkcu, $sub, 0, 0x20019, [ref]$h)
    $n = [Text.StringBuilder]::new(64); $nl = 64; $t = 0; $buf = [byte[]]::new(80000); $bl = $buf.Length; $found = $null
    for ($i = 0; [Dbce.RawRegistry]::RegEnumValueW($h, $i, $n, [ref]$nl, [IntPtr]::Zero, [ref]$t, $buf, [ref]$bl) -eq 0; $i++) {
        if ($n.ToString() -eq 'settings:cameraType') { $found = "$t/$bl/$($buf[4])" }
        $nl = 64; $bl = $buf.Length
    }
    [void][Dbce.RawRegistry]::RegCloseKey($h)
    Check ($found -eq '4/8/2') "the 8-byte DWORD came back with its type and all eight bytes ($found)"

    # A key that did not exist before the run: what the run created in it is removed.
    $absent = "HKCU:\Software\$name-absent"; $file2 = Join-Path $work 'absent.json'
    $saved = Save-RegistryState -Key $absent -Path $file2
    Check ($saved -match 'absent') 'an absent key is recorded as absent'
    Raw "Software\$name-absent" 'made' 1 ([Text.Encoding]::Unicode.GetBytes("x`0"))
    Restore-RegistryState -Path $file2 | Out-Null
    Check (@(Get-Item -LiteralPath $absent).Property.Count -eq 0) 'values created under a key that was absent are removed'

    $bad = $false; try { Save-RegistryState -Key 'HKLM:\Software\Anything' -Path (Join-Path $work 'x.json') | Out-Null } catch { $bad = $true }
    Check $bad 'a key outside HKCU is refused'
} finally {
    foreach ($k in $key, "HKCU:\Software\$name-absent") { if (Test-Path -LiteralPath $k) { Remove-Item -LiteralPath $k -Recurse -Force } }
    if ([IO.Directory]::Exists($work)) { [IO.Directory]::Delete($work, $true) }
}
if ($failed) { throw "$failed registry-state check(s) failed" }
'registry-state checks passed'
