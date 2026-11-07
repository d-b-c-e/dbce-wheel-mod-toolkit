<#
.SYNOPSIS
    Lists type and member references in a built plugin that the game's Mono 2.0 runtime lacks.

.DESCRIPTION
    Mono 2.0 plugins (DbceRuntime=Mono20 in unity/Dbce.Unity.props) compile against .NET 4.x
    reference assemblies, but the game runs the legacy Mono 2.0 profile, so a 4.0-only API (e.g.
    PropertyInfo.op_Equality from `==` on a reflection type) compiles and then throws
    MissingMethodException in the game. This reads every type and member reference into
    mscorlib / System / System.Core and checks that the game's own copy of THAT assembly defines
    it (by type, then member name). A type the game has only in another assembly counts as
    missing: a field typed Func<IntPtr> referenced mscorlib, where Mono 2.0 has no Func (it is in
    System.Core), and the whole class failed to load (TypeLoadException, 2026-10-08). Exit code 1
    when anything is missing. Run it after each build.

    Moved from dbce-mods-horizon-chase-turbo tools/Check-Net35Api.ps1 (b5014ce); only the
    parameters changed, so that any Mono 2.0 game can pass its own plugin and Managed folder.

.PARAMETER Dll
    The built plugin, e.g. src\HorizonChase.Plugin\bin\Release\net472\HorizonChaseTurboDbce.dll.

.PARAMETER GameManaged
    The game's <Game>_Data\Managed folder, which holds its mscorlib, System and System.Core. The
    .NET 3.5 reference assemblies folder also works for an offline check.
#>
param([Parameter(Mandatory)][string]$Dll,
      [Parameter(Mandatory)][string]$GameManaged)
$ErrorActionPreference = 'Stop'
function Open([string]$path) {
    $fs = [IO.File]::OpenRead($path)
    $pe = [System.Reflection.PortableExecutable.PEReader]::new($fs)
    return @{ fs = $fs; md = [System.Reflection.Metadata.PEReaderExtensions]::GetMetadataReader($pe) }
}
# Members per type in the game's framework assemblies, keyed "assembly|namespace.type".
$defined = @{}
foreach ($name in 'mscorlib', 'System', 'System.Core') {
    $a = Open (Join-Path $GameManaged "$name.dll"); $md = $a.md
    foreach ($th in $md.TypeDefinitions) {
        $td = $md.GetTypeDefinition($th)
        $full = $name + '|' + $md.GetString($td.Namespace) + '.' + $md.GetString($td.Name)
        if (-not $defined.ContainsKey($full)) { $defined[$full] = New-Object 'System.Collections.Generic.HashSet[string]' }
        foreach ($mh in $td.GetMethods()) { [void]$defined[$full].Add($md.GetString($md.GetMethodDefinition($mh).Name)) }
        foreach ($fh in $td.GetFields()) { [void]$defined[$full].Add($md.GetString($md.GetFieldDefinition($fh).Name)) }
    }
    $a.fs.Close()
}
$p = Open $Dll; $md = $p.md
$missing = New-Object System.Collections.Generic.List[string]
foreach ($h in $md.TypeReferences) {
    $tr = $md.GetTypeReference($h)
    if ($tr.ResolutionScope.Kind -ne [System.Reflection.Metadata.HandleKind]::AssemblyReference) { continue }
    $asm = $md.GetString($md.GetAssemblyReference([System.Reflection.Metadata.AssemblyReferenceHandle]$tr.ResolutionScope).Name)
    if ($asm -notin 'mscorlib', 'System', 'System.Core') { continue }
    $type = $md.GetString($tr.Namespace) + '.' + $md.GetString($tr.Name)
    if (-not $defined.ContainsKey("$asm|$type")) { $missing.Add("$asm $type (type missing)") }
}
foreach ($h in $md.MemberReferences) {
    $mr = $md.GetMemberReference($h)
    if ($mr.Parent.Kind -ne [System.Reflection.Metadata.HandleKind]::TypeReference) { continue }
    $tr = $md.GetTypeReference([System.Reflection.Metadata.TypeReferenceHandle]$mr.Parent)
    if ($tr.ResolutionScope.Kind -ne [System.Reflection.Metadata.HandleKind]::AssemblyReference) { continue }
    $asm = $md.GetString($md.GetAssemblyReference([System.Reflection.Metadata.AssemblyReferenceHandle]$tr.ResolutionScope).Name)
    if ($asm -notin 'mscorlib', 'System', 'System.Core') { continue }
    $type = $md.GetString($tr.Namespace) + '.' + $md.GetString($tr.Name)
    $member = $md.GetString($mr.Name)
    if (-not $defined.ContainsKey("$asm|$type")) { $missing.Add("$asm $type (type missing)") }
    elseif (-not $defined["$asm|$type"].Contains($member)) { $missing.Add("$asm $type :: $member") }
}
$p.fs.Close()
if ($missing.Count) { "Missing in the game's Mono 2.0 framework:"; $missing | Sort-Object -Unique; exit 1 }
'all framework type and member references exist in the game''s Mono 2.0 assemblies'
