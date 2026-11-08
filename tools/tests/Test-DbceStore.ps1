# The shared store (docs/STORE.md, CFG-1). Checks, under pwsh 7 (Test-Json):
#  - the schemas accept the fixtures and refuse broken files;
#  - the additive rule: no object in either schema refuses unknown fields (additionalProperties false), and every field
#    in schema/fixtures/store/fields.txt is still in the schemas, so fields are only ever added;
#  - the PowerShell 5.1 reader's dump of each fixture equals its .dump.txt golden, and each X.future.json (the file
#    with fields a later version adds) dumps exactly like X.json: readers ignore fields they do not know.
# The C# (Dbce.Wheel.ModFile.Tests) and C++ (native/panel/build_test.ps1) readers compare with the same goldens.
# -Update rewrites the goldens from this reader and appends new schema fields to fields.txt (it never removes one).
param([switch]$Update)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixtures = Join-Path $root 'schema\fixtures\store'
$schemas = [ordered]@{
    profiles = Join-Path $root 'schema\dbce-store-profiles.schema.json'
    runtime = Join-Path $root 'schema\dbce-store-runtime.schema.json'
}
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
function Valid([string]$json, [string]$kind) { Test-Json -Json $json -SchemaFile $schemas[$kind] -ErrorAction SilentlyContinue }
function KindOf([string]$name) { if ($name -like '*.runtime.*') { 'runtime' } else { 'profiles' } }

$files = @(Get-ChildItem $fixtures -Filter '*.json' | Where-Object { $_.Name -notlike '*.future.json' } | Sort-Object Name)
Check ($files.Count -eq 3) 'three store fixtures'
foreach ($f in $files) { Check (Valid ([IO.File]::ReadAllText($f.FullName)) (KindOf $f.Name)) "schema accepts $($f.Name)" }
$bad = [ordered]@{
    'another profiles Schema' = @('owner.profiles.json', { param($d) $d.Schema = 3 })
    'no profiles' = @('owner.profiles.json', { param($d) $d.Profiles = @() })
    'an unknown binding kind' = @('owner.profiles.json', { param($d) $d.Profiles[0].Profile.Bindings.steer.Kind = 'Pedal' })
    'an assignment without a profile' = @('owner.profiles.json', { param($d) $d.Games.'dirt-4'.PSObject.Properties.Remove('Profile') })
    'another runtime Schema' = @('art-of-rally.runtime.json', { param($d) $d.Schema = 2 })
    'a runtime file without its game' = @('art-of-rally.runtime.json', { param($d) $d.PSObject.Properties.Remove('Game') })
}
foreach ($k in $bad.Keys) {
    $file, $change = $bad[$k]
    $d = [IO.File]::ReadAllText((Join-Path $fixtures $file)) | ConvertFrom-Json
    & $change $d
    Check (-not (Valid ($d | ConvertTo-Json -Depth 30) (KindOf $file))) "schema refuses $k"
}

# The additive rule, on the schemas: unknown fields are always allowed, and no field ever leaves.
# A field is <file>.<member> at the top level or <file>#<def>.<member>; every nested object in the schemas is a $def.
function Fields($tree, [string]$kind) {
    foreach ($p in $tree['properties'].Keys) { "$kind.$p" }
    foreach ($d in $tree['$defs'].Keys) { foreach ($p in $tree['$defs'][$d]['properties'].Keys) { "$kind#$d.$p" } }
}
$now = New-Object System.Collections.Generic.SortedSet[string] ([StringComparer]::Ordinal)
foreach ($kind in $schemas.Keys) {
    $text = [IO.File]::ReadAllText($schemas[$kind])
    Check ($text -notmatch '"additionalProperties"\s*:\s*false') "$kind schema never refuses unknown fields"
    $tree = $text | ConvertFrom-Json -AsHashtable
    foreach ($field in (Fields $tree $kind)) { [void]$now.Add($field) }
}
$fieldsFile = Join-Path $fixtures 'fields.txt'
$baseline = @(Get-Content -LiteralPath $fieldsFile | Where-Object { $_ -and $_ -notlike '#*' })
$gone = @($baseline | Where-Object { -not $now.Contains($_) })
Check ($gone.Count -eq 0) "every field in fields.txt is still in the schemas (files only gain fields)$(if ($gone) { ': missing ' + ($gone -join ', ') })"
$new = @($now | Where-Object { $_ -notin $baseline })
if ($Update -and $new) { Add-Content -LiteralPath $fieldsFile -Value $new; "appended $($new.Count) field(s) to fields.txt" }
elseif ($new) { Check $false "fields.txt lists every schema field (run -Update to append: $($new -join ', '))" }

# The installer's reader, in Windows PowerShell 5.1.
$reader = Join-Path $root 'installer\DbceStore.ps1'
function Dump([string]$path) {
    $kind = KindOf ([IO.Path]::GetFileName($path))
    $format = if ($kind -eq 'runtime') { 'Format-DbceRuntimeSettings (Read-DbceRuntimeSettings -Path' } else { 'Format-DbceStore (Read-DbceStore -Path' }
    $tmp = [IO.Path]::GetTempFileName()
    try {
        $cmd = ". '$reader'; [IO.File]::WriteAllLines('$tmp', ($format '$path')), (New-Object Text.UTF8Encoding `$false))"
        & "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command $cmd
        if ($LASTEXITCODE) { throw "PowerShell 5.1 reader failed on $path" }
        [IO.File]::ReadAllText($tmp).Replace("`r", '')
    } finally { Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue }
}
foreach ($f in $files) {
    $golden = $f.FullName -replace '\.json$', '.dump.txt'
    $dump = Dump $f.FullName
    if ($Update) { [IO.File]::WriteAllText($golden, $dump, (New-Object Text.UTF8Encoding $false)); "wrote $golden" }
    $want = [IO.File]::ReadAllText($golden).Replace("`r", '')
    Check ($dump -ceq $want) "PowerShell 5.1 reader matches $([IO.Path]::GetFileName($golden))"
    $future = $f.FullName -replace '\.json$', '.future.json'
    if (Test-Path -LiteralPath $future) { Check ((Dump $future) -ceq $want) "PowerShell 5.1 reader ignores the unknown fields in $([IO.Path]::GetFileName($future))" }
}

# Resolution and the fixed folder, in pwsh 7 too.
. $reader
$store = Read-DbceStore -Path (Join-Path $fixtures 'owner.profiles.json')
$art = Get-DbceStoreChoice $store 'art-of-rally'
Check ($art.Profile.Name -eq 'Moza KS Pro Triple Auto' -and $art.ScreensFrom -eq 'cockpit' -and $art.Screens.Count -eq 3) 'art of rally uses the Auto profile with the cockpit'
Check ((Get-DbceStoreChoice $store 'dirt-4').Profile.Transmission -eq 'H-pattern') 'DiRT 4 uses the Manual profile'
Check ((Get-DbceRuntimePath 'art-of-rally') -eq (Join-Path $env:LOCALAPPDATA 'Wheelkit\store\runtime\art-of-rally.json')) 'runtime files live in %LOCALAPPDATA%\Wheelkit\store\runtime'
if ($failed) { throw "$failed store check(s) failed" }
'store checks passed'
