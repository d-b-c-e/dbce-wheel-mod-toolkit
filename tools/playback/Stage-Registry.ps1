# Windows PowerShell 5.1 and PowerShell 7. Dot-source; no registry access until called.
# reg.exe export can return success after silently truncating enumeration at a long
# Unity/Rewired value name. Preserve the complete native name/type/raw-byte inventory.
function Initialize-StageRegistry {
    if ('DbceStageRegistry' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32;
public static class DbceStageRegistry {
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegOpenKeyEx(IntPtr root,string path,int options,int access,out IntPtr result);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegQueryValueEx(IntPtr key,string name,IntPtr reserved,out uint type,byte[] data,ref uint size);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegSetValueEx(IntPtr key,string name,int reserved,uint type,byte[] data,int size);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegDeleteValue(IntPtr key,string name);
    [DllImport("advapi32.dll")] static extern int RegCloseKey(IntPtr key);
    public sealed class Value { public string name; public uint type; public string data; }
    public static bool ExistsLeaf(string path) {
        ValidatePath(path); using(var key=Registry.CurrentUser.OpenSubKey(path)) {
            if(key==null)return false;
            if(key.SubKeyCount!=0)throw new InvalidOperationException("Nested registry keys require a separate reviewed snapshot: " + path);
            return true;
        }
    }
    public static void CreateLeaf(string path) { ValidatePath(path); using(var key=Registry.CurrentUser.CreateSubKey(path)) {} }
    public static void DeleteEmptyLeaf(string path) {
        if(!ExistsLeaf(path))return;
        using(var key=Registry.CurrentUser.OpenSubKey(path)) { if(key.ValueCount!=0)throw new InvalidOperationException("Refusing to remove nonempty registry key"); }
        Registry.CurrentUser.DeleteSubKey(path,false);
    }
    public static void ValidatePath(string path) {
        if (String.IsNullOrEmpty(path) || !path.StartsWith("Software\\",StringComparison.OrdinalIgnoreCase) || path.Length<=9 || path.IndexOf('\0')>=0 || path.EndsWith("\\") || path.Contains("/"))
            throw new ArgumentException("Pass an explicit HKCU Software subkey, without hive prefix or trailing separator.");
    }
    static IntPtr Open(string path,int access) { ValidatePath(path); IntPtr key;int error=RegOpenKeyEx(new IntPtr(unchecked((int)0x80000001)),path,0,access,out key);if(error!=0)throw new Win32Exception(error);return key; }
    public static string[] Names(string path) {
        ValidatePath(path); using(var key=Registry.CurrentUser.OpenSubKey(path)) {
            if(key==null)throw new InvalidOperationException("Registry key is absent: " + path);
            var names=key.GetValueNames();Array.Sort(names,StringComparer.OrdinalIgnoreCase);return names;
        }
    }
    public static Value Read(string path,string name) {
        IntPtr key=Open(path,1);
        try { uint size=0,type;int error=RegQueryValueEx(key,name,IntPtr.Zero,out type,null,ref size);if(error!=0)throw new Win32Exception(error);
            if(size>16777216)throw new InvalidOperationException("Oversize registry value");
            byte[] bytes=new byte[size];error=RegQueryValueEx(key,name,IntPtr.Zero,out type,bytes,ref size);if(error!=0)throw new Win32Exception(error,"Registry value changed or could not be read: " + name);
            return new Value { name=name,type=type,data=Convert.ToBase64String(bytes,0,(int)size) };
        } finally { RegCloseKey(key); }
    }
    public static void Write(string path,string name,uint type,string data) {
        if(name==null || name.IndexOf('\0')>=0)throw new ArgumentException("Invalid value name");
        byte[] bytes=Convert.FromBase64String(data);if(bytes.Length>16777216)throw new InvalidOperationException("Oversize registry value");
        IntPtr key=Open(path,2);try { int error=RegSetValueEx(key,name,0,type,bytes,bytes.Length);if(error!=0)throw new Win32Exception(error); } finally { RegCloseKey(key); }
    }
    public static void Delete(string path,string name) {
        IntPtr key=Open(path,2);try { int error=RegDeleteValue(key,name);if(error!=0)throw new Win32Exception(error); } finally { RegCloseKey(key); }
    }
}
'@
}

function Get-StageRegistryValues {
    param([Parameter(Mandatory)][string]$SubKey)
    Initialize-StageRegistry
    if (![DbceStageRegistry]::ExistsLeaf($SubKey)) { throw ('Registry key is absent: '+$SubKey) }
    $names = [DbceStageRegistry]::Names($SubKey)
    if ($names.Count -gt 10000) { throw 'Registry snapshot exceeds 10,000 values.' }
    $values = @(foreach ($name in $names) { [DbceStageRegistry]::Read($SubKey,$name) })
    # A game must be closed: refuse a moving snapshot rather than blessing mixed state.
    $again = [DbceStageRegistry]::Names($SubKey)
    if ($names.Count -ne $again.Count -or ($names -join "`0") -cne ($again -join "`0")) { throw 'Registry names changed during capture.' }
    foreach ($value in $values) {
        $current = [DbceStageRegistry]::Read($SubKey,$value.name)
        if ($current.type -ne $value.type -or $current.data -cne $value.data) { throw ('Registry value changed during capture: '+$value.name) }
    }
    return ,$values
}

function Save-StageRegistry {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$SubKey,[Parameter(Mandatory)][string]$Path)
    Initialize-StageRegistry
    $exists = [DbceStageRegistry]::ExistsLeaf($SubKey)
    $values = @(); if ($exists) { $values = Get-StageRegistryValues -SubKey $SubKey }
    if ([DbceStageRegistry]::ExistsLeaf($SubKey) -ne $exists) { throw 'Registry key existence changed during capture.' }
    # Encode names too: older PS7 JSON readers turn ISO-looking strings into DateTime.
    $encoded = @(foreach ($value in $values) { [ordered]@{ nameUtf16=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($value.name)); type=$value.type; data=$value.data } })
    $document = [ordered]@{ schema='dbce.stage-registry@1'; hive='CurrentUser'; subKey=$SubKey; exists=$exists; count=$values.Count; values=$encoded }
    $json = ConvertTo-Json -InputObject $document -Depth 5
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($json)
    if ($bytes.Length -gt 67108864) { throw 'Registry snapshot exceeds 64 MiB.' }
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    $file = [IO.File]::Open($full,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try { $file.Write($bytes,0,$bytes.Length); $file.Flush($true) } finally { $file.Dispose() }
    [pscustomobject]@{ path=$full; sha256=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash; subKey=$SubKey; exists=$exists; count=$values.Count }
}

function Read-StageRegistrySnapshot {
    param([string]$SubKey,[string]$Path,[string]$ExpectedSha256)
    Initialize-StageRegistry
    [DbceStageRegistry]::ValidatePath($SubKey)
    if ($ExpectedSha256 -notmatch '^[A-Fa-f0-9]{64}$' -or (Get-Item -LiteralPath $Path).Length -gt 67108864 -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $ExpectedSha256) { throw 'Registry snapshot hash/size differs.' }
    $snapshot = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if ($snapshot.schema -cne 'dbce.stage-registry@1' -or $snapshot.hive -cne 'CurrentUser' -or $snapshot.subKey -ine $SubKey -or
        $snapshot.exists -isnot [bool] -or $snapshot.count -ne @($snapshot.values).Count -or $snapshot.count -gt 10000 -or
        (!$snapshot.exists -and $snapshot.count -ne 0)) { throw 'Wrong registry snapshot identity/count.' }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($value in $snapshot.values) {
        if ($value.nameUtf16 -isnot [string]) { throw 'Invalid registry value name encoding.' }
        $nameBytes = [Convert]::FromBase64String($value.nameUtf16)
        if ($nameBytes.Length % 2 -ne 0 -or $nameBytes.Length -gt 32766) { throw 'Invalid registry value name length.' }
        $name = [Text.UnicodeEncoding]::new($false,$false,$true).GetString($nameBytes)
        if ($name.Contains([string][char]0) -or !$seen.Add($name) -or
            $value.type -lt 0 -or $value.type -gt 11 -or $value.type -ne [uint32]$value.type -or $value.data -isnot [string]) { throw 'Invalid/duplicate registry value.' }
        if ([Convert]::FromBase64String($value.data).Length -gt 16777216) { throw 'Oversize registry value.' }
        $value | Add-Member -NotePropertyName name -NotePropertyValue $name
    }
    return $snapshot
}

function Test-StageRegistry {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$SubKey,[Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][string]$ExpectedSha256)
    $snapshot = Read-StageRegistrySnapshot $SubKey $Path $ExpectedSha256
    $exists = [DbceStageRegistry]::ExistsLeaf($SubKey)
    $current = @(); if ($exists) { $current = Get-StageRegistryValues -SubKey $SubKey }
    $byName = @{}; foreach ($value in $current) { $byName[$value.name] = $value }
    $changed = @()
    foreach ($value in $snapshot.values) {
        if (!$byName.ContainsKey($value.name) -or $byName[$value.name].type -ne $value.type -or $byName[$value.name].data -cne $value.data) { $changed += $value.name }
        $byName.Remove($value.name)
    }
    [pscustomobject]@{ equal=($snapshot.exists -eq $exists -and $changed.Count -eq 0 -and $byName.Count -eq 0); expectedExists=$snapshot.exists; currentExists=$exists; expectedCount=$snapshot.count; currentCount=$current.Count; changed=@($changed); extra=@($byName.Keys) }
}

function Restore-StageRegistry {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$SubKey,[Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][string]$ExpectedSha256)
    # Caller supplies the exact target separately; an artifact cannot redirect registry writes.
    # Validate all bytes before mutation. Restore known values first, remove only extra
    # values afterward. Delete a newly created leaf only for an absent-before snapshot.
    $snapshot = Read-StageRegistrySnapshot $SubKey $Path $ExpectedSha256
    $exists = [DbceStageRegistry]::ExistsLeaf($SubKey)
    $before = @(); if ($exists) { $before = Get-StageRegistryValues -SubKey $SubKey }
    if ($snapshot.exists -and !$exists) { [DbceStageRegistry]::CreateLeaf($SubKey) }
    $wanted = @{}; foreach ($value in $snapshot.values) { $wanted[$value.name] = $true }
    foreach ($value in $snapshot.values) { [DbceStageRegistry]::Write($SubKey,$value.name,$value.type,$value.data) }
    foreach ($value in $before) { if (!$wanted.ContainsKey($value.name)) { [DbceStageRegistry]::Delete($SubKey,$value.name) } }
    if (!$snapshot.exists -and $exists) { [DbceStageRegistry]::DeleteEmptyLeaf($SubKey) }
    $verified = Test-StageRegistry -SubKey $SubKey -Path $Path -ExpectedSha256 $ExpectedSha256
    if (!$verified.equal) { throw 'Registry restoration did not verify; preserve the snapshot and report the failure.' }
    $verified
}
