# dbce.selftest@1 (ST-1): the script schema accepts every fixture script except bad-script, which the C++ reader also
# refuses, and refuses a few broken scripts; the result schema accepts every expected result.json. The C++ interpreter
# itself is checked by native\selftest\build_test.ps1. Runs under pwsh 7 (Test-Json).
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot)
$scriptSchema = Join-Path $root 'schema\dbce-selftest.schema.json'
$resultSchema = Join-Path $root 'schema\dbce-selftest-result.schema.json'
$fixtures = Join-Path $root 'schema\fixtures\selftest'
$fail = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "ok   $what" } else { "FAIL $what"; $script:fail++ } }
function Valid([string]$json, [string]$schema) { Test-Json -Json $json -SchemaFile $schema -ErrorAction SilentlyContinue }

$scripts = Get-ChildItem $fixtures -Filter '*.script.json'
Check ($scripts.Count -ge 5) 'five or more fixture scripts'
foreach ($f in $scripts) {
    $ok = Valid ([IO.File]::ReadAllText($f.FullName)) $scriptSchema
    if ($f.Name -eq 'bad-script.script.json') { Check (-not $ok) "script schema refuses $($f.Name)" } else { Check $ok "script schema accepts $($f.Name)" }
    $result = Join-Path $fixtures ($f.Name -replace '\.script\.json$', '.result.json')
    Check (Valid ([IO.File]::ReadAllText($result)) $resultSchema) "result schema accepts $(Split-Path $result -Leaf)"
}
$head = '{"schema":"dbce.selftest@1","name":"x","steps":['
$broken = [ordered]@{
    'an unknown step'              = $head + '{"cam":"next"}]}'
    'two actions in one step'      = $head + '{"wait":"menu","key":"F7"}]}'
    'a wait for something unknown' = $head + '{"wait":"loading"}]}'
    'a set that is saved'          = $head + '{"set":"Triple.Enabled","value":false,"session":false}]}'
    'a set without a value'        = $head + '{"set":"Triple.Enabled"}]}'
    'a page on a close'            = $head + '{"f6":"close","page":"Cameras"}]}'
    'a repeat inside a repeat'     = $head + '{"camera":"next","repeat":2,"each":[{"wait":"frames:2","repeat":2}]}]}'
    'a repeated quit'              = $head + '{"quit":true,"repeat":2}]}'
    'no steps'                     = $head + ']}'
    'another schema'               = '{"schema":"dbce.selftest@2","name":"x","steps":[{"quit":true}]}'
}
foreach ($k in $broken.Keys) { Check (-not (Valid $broken[$k] $scriptSchema)) "script schema refuses $k" }
Check (Valid ($head + '{"camera":"next","repeat":2,"each":[{"capture":"cam-{i}"}]}]}') $scriptSchema) 'script schema accepts repeat with each'
if ($fail) { throw "$fail self-test schema check(s) failed" }
'PASS: self-test schemas'
