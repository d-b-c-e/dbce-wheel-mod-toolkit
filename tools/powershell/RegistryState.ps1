<# Saves and restores a registry key around a game run, value by value, as raw bytes (HOW-WE-WORK Safety 1).

   Unity games keep bindings and settings in HKCU (PlayerPrefs). Never back such a key up with `reg export` and
   restore it with `reg delete` + `reg import`: reg.exe export silently omits some values (long Rewired maps,
   8-byte DWORDs), so that restore wiped the owner's DRIVE bindings on 2026-10-07. This script reads and writes
   through the Win32 API instead: every value's name, type and exact bytes, in the key and its subkeys.

   Dot-source it, then:
     Save-RegistryState    -Key 'HKCU:\Software\Vendor\Game' -Path <file.json>     before the run
     Restore-RegistryState -Path <file.json>                                        after the game has closed
     Compare-RegistryState -Path <file.json>                                        $true when live equals saved

   Restore writes back every saved value that differs or is missing, deletes only the values the run added, removes
   only the subkeys the run added, and never deletes the key itself. A key that was absent at save time is left
   alone if still absent; values the run created under it are removed. HKCU only.

   Tested by tools\tests\Test-RegistryState.ps1 on a throwaway key.
#>
if (-not ('Dbce.RawRegistry' -as [type])) {
    Add-Type -Namespace Dbce -Name RawRegistry -MemberDefinition @'
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegOpenKeyExW(IntPtr hKey, string subKey, int options, int sam, out IntPtr result);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegCreateKeyExW(IntPtr hKey, string subKey, int reserved, string cls, int options, int sam, IntPtr security, out IntPtr result, out int disposition);
[DllImport("advapi32.dll")] public static extern int RegCloseKey(IntPtr hKey);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegEnumValueW(IntPtr hKey, int index, System.Text.StringBuilder name, ref int nameLength, IntPtr reserved, out int type, byte[] data, ref int dataLength);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegQueryInfoKeyW(IntPtr hKey, IntPtr cls, IntPtr clsLength, IntPtr reserved, out int subKeys, out int maxSubKeyLength, IntPtr maxClassLength, out int values, out int maxValueNameLength, out int maxValueLength, IntPtr security, IntPtr lastWrite);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegEnumKeyExW(IntPtr hKey, int index, System.Text.StringBuilder name, ref int nameLength, IntPtr reserved, IntPtr cls, IntPtr clsLength, IntPtr lastWrite);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegSetValueExW(IntPtr hKey, string name, int reserved, int type, byte[] data, int dataLength);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegDeleteValueW(IntPtr hKey, string name);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegDeleteTreeW(IntPtr hKey, string subKey);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern int RegDeleteKeyW(IntPtr hKey, string subKey);
'@
}

function script:RegSubPath([string]$Key) {
    if ($Key -notmatch '^(HKCU:\\|HKEY_CURRENT_USER\\|HKCU\\)(.+)$') { throw "Only keys under HKCU are handled, not '$Key'." }
    $Matches[2].TrimEnd('\')
}
$script:RegHkcu = [IntPtr]::new(0x80000001L -bor ([long]-1 -shl 32))
$script:RegRead = 0x20019; $script:RegAll = 0xF003F

# The key's values and subkeys as an ordered tree: @{ values = @{ name = @{ type; data(base64) } }; keys = @{ name = <tree> } }, or $null when absent.
function script:RegReadTree([string]$SubPath) {
    $h = [IntPtr]::Zero
    if ([Dbce.RawRegistry]::RegOpenKeyExW($script:RegHkcu, $SubPath, 0, $script:RegRead, [ref]$h) -ne 0) { return $null }
    try {
        $subKeys = 0; $maxKey = 0; $values = 0; $maxName = 0; $maxData = 0
        $rc = [Dbce.RawRegistry]::RegQueryInfoKeyW($h, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [ref]$subKeys, [ref]$maxKey, [IntPtr]::Zero, [ref]$values, [ref]$maxName, [ref]$maxData, [IntPtr]::Zero, [IntPtr]::Zero)
        if ($rc -ne 0) { throw "RegQueryInfoKey failed ($rc) for $SubPath" }
        $tree = [ordered]@{ values = [ordered]@{}; keys = [ordered]@{} }
        $read = @{}
        for ($i = 0; $i -lt $values; $i++) {
            $name = [Text.StringBuilder]::new($maxName + 2); $nameLength = $name.Capacity
            $data = [byte[]]::new([Math]::Max($maxData, 1)); $dataLength = $data.Length; $type = 0
            $rc = [Dbce.RawRegistry]::RegEnumValueW($h, $i, $name, [ref]$nameLength, [IntPtr]::Zero, [ref]$type, $data, [ref]$dataLength)
            if ($rc -ne 0) { throw "RegEnumValue failed ($rc) at index $i of $SubPath" }
            $bytes = [byte[]]::new($dataLength); [Array]::Copy($data, $bytes, $dataLength)
            $read[$name.ToString()] = @{ type = $type; data = [Convert]::ToBase64String($bytes) }
        }
        foreach ($n in ($read.Keys | Sort-Object { $_ } -CaseSensitive)) { $tree.values[$n] = $read[$n] }
        $names = for ($i = 0; $i -lt $subKeys; $i++) {
            $name = [Text.StringBuilder]::new($maxKey + 2); $nameLength = $name.Capacity
            $rc = [Dbce.RawRegistry]::RegEnumKeyExW($h, $i, $name, [ref]$nameLength, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero)
            if ($rc -ne 0) { throw "RegEnumKeyEx failed ($rc) at index $i of $SubPath" }
            $name.ToString()
        }
        foreach ($n in ($names | Sort-Object)) { $tree.keys[$n] = RegReadTree "$SubPath\$n" }
        $tree
    } finally { [void][Dbce.RawRegistry]::RegCloseKey($h) }
}

function script:RegCount($Tree) {
    if ($null -eq $Tree) { return 0 }
    $n = @($Tree.values.Keys).Count
    foreach ($k in @($Tree.keys.Keys)) { $n += RegCount $Tree.keys[$k] }
    $n
}

# Differences between a saved tree and the live one, as "what path\name" lines; empty when they are equal.
function script:RegDiff($Saved, $Live, [string]$At) {
    if ($null -eq $Saved -and $null -eq $Live) { return }
    if ($null -eq $Saved) { "added key $At"; return }
    if ($null -eq $Live) { "missing key $At"; return }
    foreach ($n in @($Saved.values.Keys)) {
        if (-not $Live.values.Contains($n)) { "missing value $At\$n" }
        elseif ($Live.values[$n].type -ne $Saved.values[$n].type -or $Live.values[$n].data -cne $Saved.values[$n].data) { "changed value $At\$n" }
    }
    foreach ($n in @($Live.values.Keys)) { if (-not $Saved.values.Contains($n)) { "added value $At\$n" } }
    foreach ($n in @($Saved.keys.Keys)) { RegDiff $Saved.keys[$n] $(if ($Live.keys.Contains($n)) { $Live.keys[$n] } else { $null }) "$At\$n" }
    foreach ($n in @($Live.keys.Keys)) { if (-not $Saved.keys.Contains($n)) { "added key $At\$n" } }
}

function script:RegWriteTree([string]$SubPath, $Saved, $Live) {
    $h = [IntPtr]::Zero; $disposition = 0
    $rc = [Dbce.RawRegistry]::RegCreateKeyExW($script:RegHkcu, $SubPath, 0, $null, 0, $script:RegAll, [IntPtr]::Zero, [ref]$h, [ref]$disposition)
    if ($rc -ne 0) { throw "Could not open $SubPath for writing ($rc)" }
    try {
        foreach ($n in @($Saved.values.Keys)) {
            $want = $Saved.values[$n]
            if ($null -ne $Live -and $Live.values.Contains($n) -and $Live.values[$n].type -eq $want.type -and $Live.values[$n].data -ceq $want.data) { continue }
            $bytes = [Convert]::FromBase64String($want.data)
            $rc = [Dbce.RawRegistry]::RegSetValueExW($h, $n, 0, [int]$want.type, $bytes, $bytes.Length)
            if ($rc -ne 0) { throw "Could not write $SubPath\$n ($rc)" }
        }
        if ($null -ne $Live) {
            foreach ($n in @($Live.values.Keys)) {
                if (-not $Saved.values.Contains($n)) { $rc = [Dbce.RawRegistry]::RegDeleteValueW($h, $n); if ($rc -ne 0) { throw "Could not remove added value $SubPath\$n ($rc)" } }
            }
            foreach ($n in @($Live.keys.Keys)) {
                if (-not $Saved.keys.Contains($n)) { $rc = [Dbce.RawRegistry]::RegDeleteTreeW($h, $n); if ($rc -ne 0) { throw "Could not remove added key $SubPath\$n ($rc)" } }
            }
        }
    } finally { [void][Dbce.RawRegistry]::RegCloseKey($h) }
    foreach ($n in @($Saved.keys.Keys)) { RegWriteTree "$SubPath\$n" $Saved.keys[$n] $(if ($null -ne $Live -and $Live.keys.Contains($n)) { $Live.keys[$n] } else { $null }) }
}

function Save-RegistryState {
    [CmdletBinding()] param([Parameter(Mandatory)][string]$Key, [Parameter(Mandatory)][string]$Path)
    if (Test-Path -LiteralPath $Path) { throw "$Path already exists; a snapshot is never overwritten." }
    $sub = RegSubPath $Key
    $tree = RegReadTree $sub
    $doc = [ordered]@{ schema = 'dbce.registry-state@1'; key = "HKCU\$sub"; saved = [DateTime]::UtcNow.ToString('o'); present = $null -ne $tree; valueCount = RegCount $tree; tree = $tree }
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent ([IO.Path]::GetFullPath($Path))))
    [IO.File]::WriteAllText($Path, ($doc | ConvertTo-Json -Depth 64 -Compress), (New-Object Text.UTF8Encoding $false))
    # Read the file back and compare it with the live key, so a snapshot that lost anything is caught before the run.
    $diff = @(RegDiff (RegLoad $Path).tree (RegReadTree $sub) "HKCU\$sub")
    if ($diff.Count) { throw "The snapshot does not match the live key ($($diff.Count) difference(s), first: $($diff[0])). Do not start the run." }
    "saved HKCU\${sub}: $($doc.valueCount) value(s)$(if (-not $doc.present) { ' (the key is absent)' })"
}

function script:RegLoad([string]$Path) {
    $doc = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -Depth 64
    if ($doc.schema -ne 'dbce.registry-state@1') { throw "$Path is not a registry snapshot." }
    $doc
}

function Compare-RegistryState {
    [CmdletBinding()] param([Parameter(Mandatory)][string]$Path, [switch]$List)
    $doc = RegLoad $Path
    $diff = @(RegDiff $doc.tree (RegReadTree (RegSubPath $doc.key)) $doc.key)
    if ($List) { $diff } else { $diff.Count -eq 0 }
}

function Restore-RegistryState {
    [CmdletBinding()] param([Parameter(Mandatory)][string]$Path)
    $doc = RegLoad $Path
    $sub = RegSubPath $doc.key
    $live = RegReadTree $sub
    $before = @(RegDiff $doc.tree $live $doc.key)
    if ($null -eq $doc.tree) {
        # The key did not exist before the run. Remove what the run put in it; the (now empty) key itself stays.
        if ($null -ne $live) { RegWriteTree $sub ([ordered]@{ values = [ordered]@{}; keys = [ordered]@{} }) $live }
    } else { RegWriteTree $sub $doc.tree $live }
    $after = @(RegDiff $(if ($null -eq $doc.tree -and $null -ne $live) { [ordered]@{ values = [ordered]@{}; keys = [ordered]@{} } } else { $doc.tree }) (RegReadTree $sub) $doc.key)
    if ($after.Count) { throw "Restore left $($after.Count) difference(s), first: $($after[0])" }
    "restored $($doc.key): $($before.Count) difference(s) put back, $($doc.valueCount) value(s) verified"
}
