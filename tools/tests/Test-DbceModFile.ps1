# dbce-mod.json (XC-1): the schema validates the three fixtures and the UI-1 settings samples unchanged, refuses
# broken files, and the PowerShell 5.1 reader's dump of each fixture equals schema/fixtures/<id>.dump.txt. The C#
# (Dbce.Wheel.ModFile.Tests) and C++ (native/panel/build_test.ps1) readers compare with the same dump files.
# Runs under pwsh 7 (Test-Json); the reader itself runs in Windows PowerShell 5.1. -Update rewrites the dumps.
param([switch]$Update)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$schema = Join-Path $root 'schema\dbce-mod.schema.json'
$fixtures = Join-Path $root 'schema\fixtures'
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
function Valid([string]$json) { Test-Json -Json $json -SchemaFile $schema -ErrorAction SilentlyContinue }

$files = @(Get-ChildItem (Join-Path $fixtures '*.dbce-mod.json'))
Check ($files.Count -eq 3) "three fixtures"
foreach ($f in $files) { Check (Valid ([IO.File]::ReadAllText($f.FullName))) "schema accepts $($f.Name)" }

# The UI-1 settings block, exactly as the panel samples hold it, is a valid settings section.
foreach ($f in Get-ChildItem (Join-Path $root 'native\panel\samples\*.settings.json')) {
    $block = ([IO.File]::ReadAllText($f.FullName) | ConvertFrom-Json).settings | ConvertTo-Json -Depth 20
    $doc = '{"schema":"dbce.mod@1","id":"sample","name":"Sample","version":"1.0.0","package":{"kind":"standalone"},"settings":' + $block + '}'
    Check (Valid $doc) "schema accepts the UI-1 block in $($f.Name) unchanged"
}

$base = [IO.File]::ReadAllText((Join-Path $fixtures 'dirt-4.dbce-mod.json')) | ConvertFrom-Json
$bad = [ordered]@{
    'an unknown top-level field' = { param($d) $d | Add-Member -NotePropertyName extra -NotePropertyValue 1 }
    'a wrong schema id' = { param($d) $d.schema = 'dbce.mod@2' }
    'an unknown package kind' = { param($d) $d.package.kind = 'melonloader' }
    'a payload path with ..' = { param($d) $d.package.payload[0].to = '../dinput8.dll' }
    'an unknown row field' = { param($d) $d.settings.pages.ffb[0] | Add-Member -NotePropertyName hidden -NotePropertyValue $true }
    'a choice without choices' = { param($d) $d.settings.pages.display[0].PSObject.Properties.Remove('choices') }
    'an upper-case file hash' = { param($d) $d.installed.files[0].sha256 = $d.installed.files[0].sha256.ToUpperInvariant() }
    'an unknown camera start' = { param($d) $d | Add-Member -NotePropertyName cameras -NotePropertyValue @(@{ camera = 'bonnet'; start = 'wide'; key = 'K' }) }
}
foreach ($k in $bad.Keys) {
    $d = [IO.File]::ReadAllText((Join-Path $fixtures 'dirt-4.dbce-mod.json')) | ConvertFrom-Json
    & $bad[$k] $d
    Check (-not (Valid ($d | ConvertTo-Json -Depth 20))) "schema refuses $k"
}

# The installer's reader, in Windows PowerShell 5.1.
$reader = Join-Path $root 'installer\DbceModFile.ps1'
foreach ($f in $files) {
    $golden = Join-Path $fixtures ($f.Name -replace '\.dbce-mod\.json$', '.dump.txt')
    $tmp = [IO.Path]::GetTempFileName()
    try {
        $cmd = ". '$reader'; [IO.File]::WriteAllLines('$tmp', (Format-DbceModFile (Read-DbceModFile -Path '$($f.FullName)')), (New-Object Text.UTF8Encoding `$false))"
        & "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command $cmd
        if ($LASTEXITCODE) { throw "PowerShell 5.1 reader failed on $($f.Name)" }
        $dump = [IO.File]::ReadAllText($tmp)
    } finally { Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue }
    if ($Update) { [IO.File]::WriteAllText($golden, $dump, (New-Object Text.UTF8Encoding $false)); "wrote $golden" }
    Check ($dump.Replace("`r", '') -ceq [IO.File]::ReadAllText($golden).Replace("`r", '')) "PowerShell 5.1 reader matches $([IO.Path]::GetFileName($golden))"
}
if ($failed) { throw "$failed dbce-mod.json check(s) failed" }
'dbce-mod.json checks passed'
