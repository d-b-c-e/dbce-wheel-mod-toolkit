#Requires -Version 7.2
<#
  ISS-0030 guard (read-only). Il2CppInterop rebuilds methods the game build stripped by translating their IL; when a
  translation is impossible the generated body throws "Method unstripping failed". This walks the interop call graph and
  reports every interop method a compiled plugin calls that can reach such a stub.

  - Failing stubs: interop methods whose IL loads the literal "...unstripping failed...".
  - Edges: call/callvirt/newobj/ldftn from translated interop bodies to other interop methods (MethodDef or MemberRef,
    resolved by declaring type full name + method name + parameter count). Natively backed methods invoke through
    il2cpp and have no managed callees, so they are leaves.
  - Plugin consumers: MemberRefs in the plugin assembly whose parent type lives in one of the interop assemblies.
  Output: one line per reachable failing call site (plugin method -> interop method -> ... -> stub). Exit 1 if any.

  Run it after each IL2CPP build (DbceRuntime=Il2Cpp in unity/Dbce.Unity.props), for example:
    pwsh tools/Find-StrippedInteropCalls.ps1 -Plugin <plugin.dll> -InteropDir <game>/BepInEx/interop
  Moved unchanged from dbce-mods-truckful tools/diagnostics/Find-StrippedInteropCalls.ps1 (0fcb282).
#>
param([Parameter(Mandatory)][string]$Plugin, [Parameter(Mandatory)][string]$InteropDir,
      [string[]]$Assemblies = @('UnityEngine.IMGUIModule.dll', 'UnityEngine.CoreModule.dll', 'Unity.InputSystem.dll',
                                'UnityEngine.TextRenderingModule.dll', 'UnityEngine.InputLegacyModule.dll'))
$ErrorActionPreference = 'Stop'
# pwsh -File passes an array argument as one string ("a.dll,b.dll"); accept comma lists so the runner can select more.
$Assemblies = @($Assemblies | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Assemblies.Count -eq 0) { throw 'No interop assembly selected (nothing would be read; refused)' }
Add-Type -AssemblyName System.Reflection.Metadata
$Op = [System.Reflection.Metadata.ILOpCode]

function Open-Md([string]$path) {
    $fs = [IO.File]::OpenRead($path); $pe = [System.Reflection.PortableExecutable.PEReader]::new($fs)
    [pscustomobject]@{ Fs = $fs; Pe = $pe; Md = [System.Reflection.Metadata.PEReaderExtensions]::GetMetadataReader($pe); Name = [IO.Path]::GetFileName($path) }
}
function Type-Name($md, $handle) {
    if ($handle -isnot [System.Reflection.Metadata.EntityHandle]) { $handle = [System.Reflection.Metadata.EntityHandle]$handle }
    switch ($handle.Kind) {
        'TypeDefinition' { $t = $md.GetTypeDefinition([System.Reflection.Metadata.TypeDefinitionHandle]$handle)
            $n = $md.GetString($t.Name); $ns = $md.GetString($t.Namespace)
            $dt = $t.GetDeclaringType(); if (-not $dt.IsNil) { return (Type-Name $md $dt) + '/' + $n }
            return $(if ($ns) { "$ns.$n" } else { $n }) }
        'TypeReference' { $t = $md.GetTypeReference([System.Reflection.Metadata.TypeReferenceHandle]$handle)
            $n = $md.GetString($t.Name); $ns = $md.GetString($t.Namespace)
            if ($t.ResolutionScope.Kind -eq 'TypeReference') { return (Type-Name $md $t.ResolutionScope) + '/' + $n }
            return $(if ($ns) { "$ns.$n" } else { $n }) }
        'TypeSpecification' { return '<spec>' }
        default { return '<?>' }
    }
}
# Parameter types, decoded from the signature blob into names that compare across assemblies (overloads differ).
function Sig-Type($md, [ref]$r) {
    $e = $r.Value.ReadByte()
    switch ($e) {
        0x01 { 'void' } 0x02 { 'bool' } 0x03 { 'char' } 0x04 { 'i1' } 0x05 { 'u1' } 0x06 { 'i2' } 0x07 { 'u2' }
        0x08 { 'i4' } 0x09 { 'u4' } 0x0A { 'i8' } 0x0B { 'u8' } 0x0C { 'r4' } 0x0D { 'r8' } 0x0E { 'string' }
        0x18 { 'native int' } 0x19 { 'native uint' } 0x1C { 'object' } 0x16 { 'typedref' }
        { $_ -in 0x11, 0x12 } { $h = $r.Value.ReadTypeHandle(); return (Type-Name $md $h) }
        0x0F { return 'ptr ' + (Sig-Type $md $r) }
        0x10 { return 'ref ' + (Sig-Type $md $r) }
        0x1D { return (Sig-Type $md $r) + '[]' }
        0x13 { return '!' + $r.Value.ReadCompressedInteger() }
        0x1E { return '!!' + $r.Value.ReadCompressedInteger() }
        0x1F { [void]$r.Value.ReadTypeHandle(); return (Sig-Type $md $r) }   # cmod_reqd
        0x20 { [void]$r.Value.ReadTypeHandle(); return (Sig-Type $md $r) }   # cmod_opt
        0x41 { return (Sig-Type $md $r) }                                    # pinned
        0x15 { $g = Sig-Type $md $r; $n = $r.Value.ReadCompressedInteger(); $a = for ($i = 0; $i -lt $n; $i++) { Sig-Type $md $r }; return "$g<$($a -join ',')>" }
        0x14 { $el = Sig-Type $md $r; $rank = $r.Value.ReadCompressedInteger(); $ns = $r.Value.ReadCompressedInteger(); for ($i = 0; $i -lt $ns; $i++) { [void]$r.Value.ReadCompressedInteger() }; $nl = $r.Value.ReadCompressedInteger(); for ($i = 0; $i -lt $nl; $i++) { [void]$r.Value.ReadCompressedSignedInteger() }; return "$el[$rank]" }
        default { "?$e" }
    }
}
function Sig-Count($md, $blobHandle) {
    $r = $md.GetBlobReader($blobHandle); $hdr = $r.ReadSignatureHeader()
    if ($hdr.IsGeneric) { [void]$r.ReadCompressedInteger() }
    $n = $r.ReadCompressedInteger(); $ref = [ref]$r
    [void](Sig-Type $md $ref)   # return type
    $p = for ($i = 0; $i -lt $n; $i++) { Sig-Type $md $ref }
    return '(' + ($p -join ',') + ')'
}
function Method-Key($md, $handle) {
    if ($handle -isnot [System.Reflection.Metadata.EntityHandle]) { $handle = [System.Reflection.Metadata.EntityHandle]$handle }
    switch ($handle.Kind) {
        'MethodDefinition' { $m = $md.GetMethodDefinition([System.Reflection.Metadata.MethodDefinitionHandle]$handle)
            return "$(Type-Name $md $m.GetDeclaringType())::$($md.GetString($m.Name))/$(Sig-Count $md $m.Signature)" }
        'MemberReference' { $m = $md.GetMemberReference([System.Reflection.Metadata.MemberReferenceHandle]$handle)
            if ($m.GetKind() -ne 'Method') { return $null }
            return "$(Type-Name $md $m.Parent)::$($md.GetString($m.Name))/$(Sig-Count $md $m.Signature)" }
        'MethodSpecification' { $s = $md.GetMethodSpecification([System.Reflection.Metadata.MethodSpecificationHandle]$handle); return (Method-Key $md $s.Method) }
        default { return $null }
    }
}
# Calls made by one IL body: token operands of call/callvirt/newobj/ldftn/ldvirtftn; ldstr literals for the stub marker.
function Scan-Body($md, [byte[]]$il) {
    $calls = [Collections.Generic.List[string]]::new(); $failing = $false; $i = 0
    while ($i -lt $il.Length) {
        $b = $il[$i]; $code = [int]$b; $i++
        if ($b -eq 0xFE) { $code = 0xFE00 -bor $il[$i]; $i++ }
        $opc = [System.Reflection.Metadata.ILOpCode]$code
        if ([System.Reflection.Metadata.ILOpCodeExtensions]::IsBranch($opc)) { $i += [System.Reflection.Metadata.ILOpCodeExtensions]::GetBranchOperandSize($opc); continue }
        if ($code -in 0x28, 0x6F, 0x73, 0xFE06, 0xFE07) {
            $tok = [BitConverter]::ToInt32($il, $i); $i += 4
            $h = [System.Reflection.Metadata.Ecma335.MetadataTokens]::EntityHandle($tok)
            $k = Method-Key $md $h; if ($k) { $calls.Add($k) }; continue
        }
        if ($code -eq 0x72) {
            $tok = [BitConverter]::ToInt32($il, $i); $i += 4
            try { if ($md.GetUserString([System.Reflection.Metadata.Ecma335.MetadataTokens]::UserStringHandle($tok -band 0xFFFFFF)) -like '*unstripping failed*') { $failing = $true } } catch { }
            continue
        }
        if ($code -eq 0x45) { $n = [BitConverter]::ToInt32($il, $i); $i += 4 + 4 * $n; continue }   # switch
        # Operand sizes for the remaining opcodes (ECMA-335 III).
        $i += switch ($code) {
            { $_ -in 0x0E,0x0F,0x10,0x11,0x12,0x13,0x1F } { 1 }        # ldarg.s ldarga.s starg.s ldloc.s ldloca.s stloc.s ldc.i4.s
            { $_ -in 0x20,0x22 } { 4 }                                 # ldc.i4 ldc.r4
            { $_ -in 0x21,0x23 } { 8 }                                 # ldc.i8 ldc.r8
            { $_ -in 0x27,0x29,0x70,0x71,0x74,0x75,0x79,0x7B,0x7C,0x7D,0x7E,0x7F,0x80,0x81,0x8C,0x8D,0x8F,0xA3,0xA4,0xA5,0xC2,0xC6,0xD0,0xFE15,0xFE16,0xFE1C } { 4 }
            { $_ -in 0xFE09,0xFE0A,0xFE0B,0xFE0C,0xFE0D,0xFE0E } { 2 } # ldarg ldarga starg ldloc ldloca stloc
            { $_ -in 0xFE12,0xFE19 } { 1 }                             # unaligned. no.
            default { 0 }
        }
    }
    [pscustomobject]@{ Calls = $calls; Failing = $failing }
}

$readers = @(); $graph = @{}; $failing = [Collections.Generic.HashSet[string]]::new()
try {
    foreach ($name in $Assemblies) {
        $p = Join-Path $InteropDir $name; if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "Selected interop assembly missing: $p (nothing would be read; refused)" }
        $r = Open-Md $p; $readers += $r
        foreach ($th in $r.Md.TypeDefinitions) {
            foreach ($mh in $r.Md.GetTypeDefinition($th).GetMethods()) {
                $m = $r.Md.GetMethodDefinition($mh); if ($m.RelativeVirtualAddress -eq 0) { continue }
                $key = Method-Key $r.Md $mh
                $s = Scan-Body $r.Md ([System.Reflection.Metadata.PEReaderExtensions]::GetMethodBody($r.Pe, $m.RelativeVirtualAddress).GetILBytes())
                $graph[$key] = $s.Calls; if ($s.Failing) { [void]$failing.Add($key) }
            }
        }
    }
    # Reverse reachability: every method that can reach a failing stub, with one witness path.
    $bad = @{}; foreach ($f in $failing) { $bad[$f] = @($f) }
    do {
        $grew = $false
        foreach ($k in @($graph.Keys | Sort-Object)) {   # sorted: deterministic witness paths
            if ($bad.ContainsKey($k)) { continue }
            foreach ($c in @($graph[$k] | Sort-Object)) { if ($bad.ContainsKey($c)) { $bad[$k] = @($k) + $bad[$c]; $grew = $true; break } }
        }
    } while ($grew)

    $pluginMd = Open-Md $Plugin; $readers += $pluginMd
    $interopTypes = [Collections.Generic.HashSet[string]]::new()
    foreach ($k in $graph.Keys) { [void]$interopTypes.Add($k.Substring(0, $k.IndexOf('::'))) }
    $hits = [Collections.Generic.List[string]]::new(); $pluginCalls = @{}; $called = [Collections.Generic.HashSet[string]]::new()
    foreach ($th in $pluginMd.Md.TypeDefinitions) {
        foreach ($mh in $pluginMd.Md.GetTypeDefinition($th).GetMethods()) {
            $m = $pluginMd.Md.GetMethodDefinition($mh); if ($m.RelativeVirtualAddress -eq 0) { continue }
            $owner = Method-Key $pluginMd.Md $mh
            $s = Scan-Body $pluginMd.Md ([System.Reflection.Metadata.PEReaderExtensions]::GetMethodBody($pluginMd.Pe, $m.RelativeVirtualAddress).GetILBytes())
            $pluginCalls[$owner] = @($s.Calls | Sort-Object -Unique); foreach ($c in $pluginCalls[$owner]) { [void]$called.Add($c) }
        }
    }
    # Report a plugin method only if other plugin code calls it or Unity/Harmony can enter it; vendored helpers the
    # plugin no longer calls (for example RectBindingPanel.TextInput after ISS-0030) are listed separately, not failed.
    $entry = '::(OnGUI|Update|LateUpdate|FixedUpdate|Awake|Start|OnEnable|OnDisable|OnDestroy|OnApplicationFocus|OnApplicationQuit|Load|Unload|Before\w+|After\w+|Prefix\w*|Postfix\w*)/'
    $unreferenced = [Collections.Generic.List[string]]::new()
    foreach ($owner in $pluginCalls.Keys) {
        foreach ($c in $pluginCalls[$owner]) {
            if (-not $bad.ContainsKey($c)) { continue }
            $line = "$owner -> " + ($bad[$c] -join ' -> ')
            if ($called.Contains($owner) -or $owner -match $entry) { $hits.Add($line) } else { $unreferenced.Add($line) }
        }
    }
    "coverage: $($Assemblies.Count) interop assemblies read, $($graph.Count) interop method bodies, $($pluginCalls.Count) plugin method bodies"
    if ($graph.Count -eq 0 -or $pluginCalls.Count -eq 0) { throw 'Guard read no method bodies; refused' }
    "failing stubs: $($failing.Count); methods reaching one: $($bad.Count); referenced plugin call sites reaching one: $($hits.Count); unreferenced: $($unreferenced.Count)"
    foreach ($u in ($unreferenced | Sort-Object -Unique)) { "unreferenced (not failed): $u" }
    $hits | Sort-Object -Unique
    if ($hits.Count) { exit 1 }
} finally { foreach ($rd in $readers) { if ($rd) { $rd.Pe.Dispose(); $rd.Fs.Dispose() } } }
