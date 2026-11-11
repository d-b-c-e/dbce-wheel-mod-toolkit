# Every `## STD-` id in STANDARDS.md has an entry in standards-checks.json and vice versa, every entry is well formed,
# every script path exists and every regex compiles. A new standard cannot land without deciding its check.
$ErrorActionPreference = 'Stop'
$toolkit = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$hub = [IO.Path]::GetFullPath((Join-Path $toolkit '..\..\..\dbce-project-mgmt'))
$problems = [Collections.Generic.List[string]]::new()
function Bad([string]$m) { $script:problems.Add($m) }

$ids = @(Select-String -LiteralPath (Join-Path $toolkit 'STANDARDS.md') -Pattern '^## (STD-\d+)\b' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$json = Get-Content -LiteralPath (Join-Path $toolkit 'standards-checks.json') -Raw | ConvertFrom-Json
$entries = @($json.standards.PSObject.Properties.Name)
foreach ($id in $ids) { if ($id -notin $entries) { Bad "$id is in STANDARDS.md but has no entry in standards-checks.json (decide: checked, advice or superseded)" } }
foreach ($id in $entries) { if ($id -notin $ids) { Bad "$id is in standards-checks.json but not in STANDARDS.md" } }
if (@($ids | Select-Object -Unique).Count -ne $ids.Count) { Bad 'STANDARDS.md repeats a STD id' }

$families = 'all', 'native', 'unreal', 'unity', 'web', 'triples'
function Test-Regex([string]$id, [string]$what, [string]$rx) {
    if (-not $rx) { Bad "$id $what is empty"; return }
    try { [void][regex]::new($rx) } catch { Bad "$id $what does not compile: $($_.Exception.Message)" }
}
foreach ($p in $json.standards.PSObject.Properties) {
    $id = $p.Name; $s = $p.Value
    if (-not $s.title) { Bad "$id has no title" }
    switch ($s.status) {
        'superseded' { if (-not $s.supersededBy) { Bad "$id is superseded but names no replacement (supersededBy)" } }
        'advice'     { if (-not $s.why) { Bad "$id is advice but says no why" } }
        'checked' {
            foreach ($a in @($s.appliesTo)) { if ($a -notin $families) { Bad "$id appliesTo '$a' is not one of: $($families -join ', ')" } }
            if (-not @($s.appliesTo).Count) { Bad "$id is checked but has no appliesTo" }
            $list = @($s.check)
            if (-not $list.Count -or -not $list[0]) { Bad "$id is checked but has no check" }
            foreach ($c in $list) {
                switch ($c.kind) {
                    { $_ -in 'forbid', 'require' } {
                        Test-Regex $id "$($c.kind) pattern" $c.pattern
                        Test-Regex $id "$($c.kind) files" $c.files
                        if (-not $c.why) { Bad "$id $($c.kind) check says no why" }
                    }
                    'file' { if (-not $c.path) { Bad "$id file check has no path" } }
                    'script' {
                        $path = if ($c.path -like 'hub:*') { Join-Path $hub $c.path.Substring(4) } else { Join-Path $toolkit $c.path }
                        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Bad "$id script does not exist: $($c.path)" }
                    }
                    default { Bad "$id has unknown check kind '$($c.kind)'" }
                }
            }
        }
        default { Bad "$id has status '$($s.status)'; use checked, advice or superseded" }
    }
}
if ($problems.Count) { $problems | ForEach-Object { "FAIL $_" }; exit 1 }
$n = @($json.standards.PSObject.Properties | Where-Object { $_.Value.status -eq 'checked' }).Count
"ok: $($ids.Count) standards, $n checked, $(@($json.standards.PSObject.Properties | Where-Object { $_.Value.status -eq 'advice' }).Count) advice, $(@($json.standards.PSObject.Properties | Where-Object { $_.Value.status -eq 'superseded' }).Count) superseded"
