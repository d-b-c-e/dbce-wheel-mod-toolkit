#Requires -Version 7.0
# RacingRow.cs and WindowsMonitors.cs on Unity's legacy Mono 2.0 profile (.NET 3.5). Two gates:
# 1. They compile for net35, so no 4.x API (IReadOnlyList, Array.Empty, the generic string.Join, ValueTuple) is used.
# 2. Their net472 build is how Mono 2.0 mods compile. Every framework type it references must exist in the same
#    assembly in the .NET 3.5 reference assemblies, and every member it references must exist by name on that type.
#    The check catches a reference a 4.x compile places differently from 3.5:
#    - Func from LINQ lambdas, which is in mscorlib on 4.x but in System.Core on 3.5;
#    - IsReadOnlyAttribute from `readonly struct`, which is in mscorlib only from 4.7.2.
#    Member checks go by name; gate 1 covers overloads.
# A consumer still runs its own check against the game's assemblies (Horizon Chase Turbo: tools\Check-Net35Api.ps1).
# This script builds only this compile-only project, and makes no game, device or display call.
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'RacingRowCompat35.csproj'
dotnet build $project -c Release -nologo -v q | Out-Host
if ($LASTEXITCODE) { throw 'The racing-row source does not compile for net35 and net472.' }
$packages = ((dotnet nuget locals global-packages -l) -replace '^global-packages:\s*', '').Trim()
$ref35 = Get-ChildItem (Join-Path $packages 'microsoft.netframework.referenceassemblies.net35') -Directory | Sort-Object Name | Select-Object -Last 1 |
    ForEach-Object { Join-Path $_.FullName 'build\.NETFramework\v3.5' }
if (-not $ref35 -or -not (Test-Path (Join-Path $ref35 'mscorlib.dll'))) { throw 'The .NET 3.5 reference assemblies were not restored.' }
function Open-Metadata([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    [pscustomobject]@{ Stream = $stream; Md = [System.Reflection.Metadata.PEReaderExtensions]::GetMetadataReader([System.Reflection.PortableExecutable.PEReader]::new($stream)) }
}
# "assembly|namespace.type" -> member names, from the 3.5 reference assemblies.
$defined = @{}
foreach ($name in 'mscorlib', 'System', 'System.Core') {
    $a = Open-Metadata (Join-Path $ref35 "$name.dll")
    try {
        foreach ($h in $a.Md.TypeDefinitions) {
            $td = $a.Md.GetTypeDefinition($h)
            $key = "$name|" + $a.Md.GetString($td.Namespace) + '.' + $a.Md.GetString($td.Name)
            $members = [Collections.Generic.HashSet[string]]::new()
            foreach ($m in $td.GetMethods()) { [void]$members.Add($a.Md.GetString($a.Md.GetMethodDefinition($m).Name)) }
            foreach ($f in $td.GetFields()) { [void]$members.Add($a.Md.GetString($a.Md.GetFieldDefinition($f).Name)) }
            $defined[$key] = $members
        }
    } finally { $a.Stream.Dispose() }
}
$built = Open-Metadata (Join-Path $PSScriptRoot 'bin\Release\net472\RacingRowCompat35.dll')
$problems = [Collections.Generic.List[string]]::new(); $types = 0; $members = 0
try {
    $md = $built.Md
    function Get-RefKey($Handle) {
        $t = $md.GetTypeReference($Handle)
        if ($t.ResolutionScope.Kind -ne [System.Reflection.Metadata.HandleKind]::AssemblyReference) { return $null }
        $md.GetString($md.GetAssemblyReference([System.Reflection.Metadata.AssemblyReferenceHandle]$t.ResolutionScope).Name) + '|' + $md.GetString($t.Namespace) + '.' + $md.GetString($t.Name)
    }
    foreach ($h in $md.TypeReferences) {
        $key = Get-RefKey $h
        if (-not $key) { continue }
        $types++
        if (-not $defined.ContainsKey($key)) { $problems.Add("type not in .NET 3.5: $key") }
    }
    foreach ($h in $md.MemberReferences) {
        $mr = $md.GetMemberReference($h)
        if ($mr.Parent.Kind -ne [System.Reflection.Metadata.HandleKind]::TypeReference) { continue }
        $key = Get-RefKey ([System.Reflection.Metadata.TypeReferenceHandle]$mr.Parent)
        if (-not $key) { continue }
        $members++
        $name = $md.GetString($mr.Name)
        if ($defined.ContainsKey($key) -and -not $defined[$key].Contains($name)) { $problems.Add("member not in .NET 3.5: $key :: $name") }
    }
} finally { $built.Stream.Dispose() }
if ($problems.Count) { 'Not Mono 2.0 safe (net472 build):'; $problems | Sort-Object -Unique | ForEach-Object { "  $_" }; exit 1 }
"PASS Mono 2.0 racing row: net35 compile; the net472 build's $types framework type and $members member references all exist in the .NET 3.5 reference assemblies"
