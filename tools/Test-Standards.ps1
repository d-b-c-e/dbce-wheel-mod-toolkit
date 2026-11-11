<#
  Runs the mechanical checks in standards-checks.json over a game repo's tracked files.

    pwsh tools/Test-Standards.ps1 [-Repo <path or part of a dbce-mods-* name>] [-Json]

  With no -Repo it runs every game in the hub's tools/maturity.json and prints one line per game: standards checked,
  passed, failed. A failure prints as `STD-0NN <file>:<line> <matched text>`. Exit 1 when anything fails.

  Check kinds (see standards-checks.json):
    forbid   no own-source file whose path matches `files` may contain `pattern`
    require  at least one tracked file whose path matches `files` must contain `pattern`
    file     the tracked file `path` must exist
    script   an existing test already enforces it; listed, not run
  `files` is a regex on the tracked path. Own source = tracked, not vendored (the same list as the hub's Get-Adoption.ps1),
  not a test, not under docs/, not Markdown. `require` also counts vendored files (a vendored panel is the panel).
  A standard applies when its `appliesTo` has `all`, the game's family (native, unreal, unity, web), or `triples`
  (the repo has an own-source file whose path matches Triple|triple). `exempt` maps a game name to the reason it is excused.
#>
[CmdletBinding()]
param([string]$Repo = '', [switch]$Json)
$ErrorActionPreference = 'Stop'
$toolkit = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$modding = [IO.Path]::GetFullPath((Join-Path $toolkit '..\..'))
$hub = [IO.Path]::GetFullPath((Join-Path $modding '..\dbce-project-mgmt'))
# Same list as the hub's Get-Adoption.ps1.
$vendored = '(^|/)(lib/toolkit|src/vendor|vendor|lib/recording|third_party|imgui|external|deps|upstream|node_modules)/'
$textFile = '\.(cs|cpp|cc|h|hpp|c|lua|js|ts|json|xml|ini|ps1|bat|cmd|csproj|props|targets|cmake|txt|yml|yaml|toml|html|css)$'
$maturity = Get-Content -LiteralPath (Join-Path $hub 'tools\maturity.json') -Raw | ConvertFrom-Json
$checks = Get-Content -LiteralPath (Join-Path $toolkit 'standards-checks.json') -Raw | ConvertFrom-Json

function Get-Games {
    if ($Repo) {
        $dir = if (Test-Path -LiteralPath $Repo) { (Resolve-Path -LiteralPath $Repo).Path }
               else { Get-ChildItem -LiteralPath $modding -Directory -Filter 'dbce-mods-*' | Where-Object { $_.Name -notmatch '\.wt-' -and $_.Name -match [regex]::Escape($Repo) } | Select-Object -First 1 -ExpandProperty FullName }
        if (-not $dir) { throw "No repo matches '$Repo'" }
        $name = (Split-Path -Leaf $dir) -replace '^dbce-mods-', ''
        [pscustomobject]@{ Name = $name; Dir = $dir; Family = $maturity.games.$name }
    } else {
        foreach ($p in $maturity.games.PSObject.Properties) {
            [pscustomobject]@{ Name = $p.Name; Dir = (Join-Path $modding "dbce-mods-$($p.Name)"); Family = $p.Value }
        }
    }
}

function Test-Game($game) {
    $files = @(git -C $game.Dir ls-files 2>$null)
    if (-not $files.Count) { throw "$($game.Name): no tracked files in $($game.Dir)" }
    $own = @($files | Where-Object { $_ -notmatch $vendored -and $_ -notmatch '(?i)test' -and $_ -notmatch '(^|/)docs/' -and $_ -notmatch '\.md$' })
    $all = @($files | Where-Object { $_ -notmatch '(?i)test' -and $_ -notmatch '(^|/)docs/' -and $_ -notmatch '\.md$' })
    $triples = @($own | Where-Object { $_ -cmatch 'Triple|triple' }).Count -gt 0
    $cache = @{}
    function Read-Text([string]$rel) {
        if (-not $cache.ContainsKey($rel)) {
            $p = Join-Path $game.Dir $rel
            $cache[$rel] = if ([IO.File]::Exists($p) -and ([IO.FileInfo]$p).Length -lt 3MB) { [IO.File]::ReadAllText($p) } else { '' }
        }
        $cache[$rel]
    }
    function Find-Matches([string[]]$paths, [string]$pattern, [bool]$ignoreCase) {
        $opt = [Text.RegularExpressions.RegexOptions]::Multiline
        if ($ignoreCase) { $opt = $opt -bor [Text.RegularExpressions.RegexOptions]::IgnoreCase }
        $rx = [regex]::new($pattern, $opt)
        foreach ($rel in $paths) {
            if ($rel -notmatch $textFile) { continue }
            $text = Read-Text $rel
            if (-not $text) { continue }
            foreach ($m in $rx.Matches($text)) {
                $line = 1; foreach ($ch in $text.Substring(0, $m.Index).ToCharArray()) { if ($ch -eq "`n") { $line++ } }
                $shown = ($m.Value -replace '\s+', ' ').Trim(); if ($shown.Length -gt 90) { $shown = $shown.Substring(0, 90) + '...' }
                [pscustomobject]@{ File = $rel; Line = $line; Text = $shown }
            }
        }
    }

    $results = foreach ($p in $checks.standards.PSObject.Properties) {
        $id = $p.Name; $s = $p.Value
        if ($s.status -ne 'checked') { continue }
        $applies = @($s.appliesTo) | Where-Object { $_ -eq 'all' -or $_ -eq $game.Family -or ($_ -eq 'triples' -and $triples) }
        if (-not $applies) { continue }
        $list = @($s.check)
        if ($list[0].kind -eq 'script') { [pscustomobject]@{ Id = $id; Kind = 'script'; Result = 'listed'; Hits = @(); Note = $list[0].path }; continue }
        if ($s.exempt -and $s.exempt.PSObject.Properties[$game.Name]) { [pscustomobject]@{ Id = $id; Kind = $list[0].kind; Result = 'exempt'; Hits = @(); Note = $s.exempt.($game.Name) }; continue }
        $hits = @()
        foreach ($c in $list) {
            $ic = [bool]$c.ignoreCase
            switch ($c.kind) {
                'forbid' { $hits += @(Find-Matches @($own | Where-Object { $_ -match $c.files }) $c.pattern $ic) }
                'require' {
                    if (-not @(Find-Matches @($all | Where-Object { $_ -match $c.files }) $c.pattern $ic | Select-Object -First 1).Count) {
                        $hits += [pscustomobject]@{ File = '(no file)'; Line = 0; Text = "missing: $($c.pattern)" }
                    }
                }
                'file' {
                    if (-not (@($files | Where-Object { $_ -ieq $c.path }).Count)) { $hits += [pscustomobject]@{ File = $c.path; Line = 0; Text = 'file missing' } }
                }
                default { throw "$id has unknown check kind '$($c.kind)'" }
            }
        }
        [pscustomobject]@{ Id = $id; Kind = $list[0].kind; Result = $(if ($hits.Count) { 'fail' } else { 'pass' }); Hits = $hits; Note = '' }
    }
    $results = @($results)
    [pscustomobject]@{
        Game    = $game.Name; Family = $game.Family; Triples = $triples
        Checked = @($results | Where-Object { $_.Result -in 'pass', 'fail' }).Count
        Passed  = @($results | Where-Object Result -eq 'pass').Count
        Failed  = @($results | Where-Object Result -eq 'fail')
        Exempt  = @($results | Where-Object Result -eq 'exempt')
        Scripts = @($results | Where-Object Result -eq 'listed')
    }
}

$report = foreach ($g in Get-Games) { Test-Game $g }
$report = @($report)
if ($Json) {
    $report | ForEach-Object {
        [pscustomobject]@{
            game = $_.Game; family = $_.Family; triples = $_.Triples; checked = $_.Checked; passed = $_.Passed
            failed = @($_.Failed | ForEach-Object { [pscustomobject]@{ id = $_.Id; hits = @($_.Hits | ForEach-Object { "$($_.File):$($_.Line) $($_.Text)" }) } })
            exempt = @($_.Exempt | ForEach-Object Id); scripts = @($_.Scripts | ForEach-Object Id)
        }
    } | ConvertTo-Json -Depth 6
} else {
    foreach ($r in $report) {
        $ids = if ($r.Failed.Count) { ' (' + (($r.Failed | ForEach-Object Id) -join ', ') + ')' } else { '' }
        '{0,-22} {1,-7} checked {2}, passed {3}, failed {4}{5}' -f $r.Game, $r.Family, $r.Checked, $r.Passed, $r.Failed.Count, $ids
        foreach ($f in $r.Failed) {
            foreach ($h in ($f.Hits | Select-Object -First 8)) { "    $($f.Id) $($h.File):$($h.Line) $($h.Text)" }
            if ($f.Hits.Count -gt 8) { "    $($f.Id) ... $($f.Hits.Count - 8) more" }
        }
        if ($Repo) {
            foreach ($e in $r.Exempt) { "    $($e.Id) exempt: $($e.Note)" }
            foreach ($s in $r.Scripts) { "    $($s.Id) script (listed, not run): $($s.Note)" }
        }
    }
}
if (@($report | Where-Object { $_.Failed.Count }).Count) { exit 1 }
