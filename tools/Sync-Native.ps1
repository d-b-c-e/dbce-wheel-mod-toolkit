<#
.SYNOPSIS
Vendor the native toolkit into a native dinput8-proxy game's src/vendor, from one wheel-toolkit commit and one
triple-screen commit, or check that src/vendor still matches the two commits named in its VERSION file.

.DESCRIPTION
src/vendor is owned by this script: it holds exactly the folders below, at those two commits, without tests, plus a
two-line VERSION. Never edit a vendored file; change the toolkit and sync again. Text files are compared with CRLF
read as LF, so a checkout's line endings do not count as an edit.

.EXAMPLE
pwsh tools\Sync-Native.ps1 -GameRepo E:\Source\game-modding\dbce-mods-grid-2019            # sync at master + main
pwsh tools\Sync-Native.ps1 -GameRepo <game> -Commit 08a8c2a -TripleCommit 76dd607 -DryRun  # list deltas, write nothing
pwsh tools\Sync-Native.ps1 -GameRepo <game> -Check   # exit 1 if any vendored file differs from VERSION's commits
#>
param(
  [Parameter(Mandatory)][string]$GameRepo,
  [string]$Commit = 'master',
  [string]$TripleCommit = 'main',
  [switch]$Check,
  [switch]$DryRun,
  [string]$ToolkitRepo = (Join-Path $PSScriptRoot '..'),
  [string]$TripleRepo = (Join-Path $PSScriptRoot '../../dbce-triple-screen-toolkit')
)
$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'
# Source folder -> folder under src/vendor. Whole folders, the same list for every game.
$sources = @(
  @{ Name = 'dbce-wheel-mod-toolkit'; Repo = $ToolkitRepo; Ref = $Commit; Folders = [ordered]@{
      'native/controls' = 'controls'; 'native/dinput-proxy' = 'dinput-proxy'; 'native/forza' = 'forza'
      'native/panel' = 'panel'; 'native/settings' = 'settings'; 'native/support' = 'support'
      'native/third_party/imgui' = 'third_party/imgui'; 'shared/display' = 'display' } },
  @{ Name = 'dbce-triple-screen-toolkit'; Repo = $TripleRepo; Ref = $TripleCommit; Folders = [ordered]@{
      'native/compositor' = 'triple-screen/compositor'; 'native/geometry' = 'triple-screen/geometry'
      'native/layout' = 'triple-screen/layout' } })
# Tests, test vectors, build scripts, git dotfiles and the managed (.cs) half of shared/display stay behind.
$drop = '(^|/)(build|tests?|fixtures|tools)/|(^|/)(test[^/]*|[^/]*_test\.[^/]*|build_[^/]*|[^/]*-vectors\.txt|CMakeLists\.txt|\.git[^/]*|[^/]*\.cs)$'
$vendor = Join-Path (Resolve-Path -LiteralPath $GameRepo).ProviderPath 'src/vendor'
$versionFile = Join-Path $vendor 'VERSION'

if ($Check) {  # take both commits from VERSION, never from the command line
  if (-not (Test-Path -LiteralPath $versionFile)) { throw "No $versionFile; run Sync-Native.ps1 without -Check first" }
  $pins = @{}; foreach ($line in Get-Content -LiteralPath $versionFile) { $name, $sha = $line -split ' ', 2; $pins[$name] = $sha }
  foreach ($s in $sources) { if (-not $pins[$s.Name]) { throw "VERSION has no $($s.Name) line" }; $s.Ref = $pins[$s.Name] }
}
$stage = Join-Path ([IO.Path]::GetTempPath()) ('sync-native-' + [guid]::NewGuid().ToString('N'))
try {
  New-Item -ItemType Directory -Path $stage | Out-Null
  $version = foreach ($s in $sources) {
    $sha = & git -C $s.Repo rev-parse --verify --quiet "$($s.Ref)^{commit}"
    if ($LASTEXITCODE -ne 0 -or -not $sha) { throw "Commit '$($s.Ref)' not found in $($s.Repo); fetch it there first" }
    $zip = "$stage-$($s.Name).zip"; $raw = "$stage-$($s.Name)"
    & git -C $s.Repo -c core.autocrlf=false archive --format=zip -o $zip $sha -- @($s.Folders.Keys)
    if ($LASTEXITCODE -ne 0) { throw "git archive failed in $($s.Repo) at $sha" }
    Expand-Archive -LiteralPath $zip -DestinationPath $raw
    foreach ($folder in $s.Folders.Keys) {
      $root = [IO.Path]::GetFullPath((Join-Path $raw $folder))
      if (-not (Test-Path -LiteralPath $root)) { throw "$($s.Name) $sha has no $folder" }
      foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse -Force) {
        $rel = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
        if ($rel -match $drop) { continue }
        $to = Join-Path $stage "$($s.Folders[$folder])/$rel"
        New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
        Move-Item -LiteralPath $file.FullName -Destination $to
      }
    }
    "$($s.Name) $sha"
  }
  [IO.File]::WriteAllText((Join-Path $stage 'VERSION'), (($version -join "`n") + "`n"))

  # Deltas between what the commits give and what src/vendor holds now.
  function List([string]$dir) {
    $map = @{}; if (Test-Path -LiteralPath $dir) {
      foreach ($f in Get-ChildItem -LiteralPath $dir -File -Recurse -Force) { $map[$f.FullName.Substring($dir.Length + 1).Replace('\', '/')] = $f.FullName } }
    $map }
  function Text([string]$path) { [IO.File]::ReadAllText($path, [Text.Encoding]::GetEncoding(28591)).Replace("`r`n", "`n") }
  $want = List $stage; $have = List $vendor
  $deltas = @(foreach ($rel in (@($want.Keys) + @($have.Keys) | Sort-Object -Unique)) {
    if (-not $have.ContainsKey($rel)) { "added   $rel" }
    elseif (-not $want.ContainsKey($rel)) { "removed $rel" }
    elseif ((Text $want[$rel]) -cne (Text $have[$rel])) { "changed $rel" } })
  $deltas | ForEach-Object { Write-Output $_ }

  if ($Check) {
    if ($deltas.Count) { Write-Error "src/vendor differs from VERSION in $($deltas.Count) file(s); change the toolkit and re-sync, never the copy" -ErrorAction Continue; exit 1 }
    Write-Output "src/vendor matches $($version -join ', ')"
  } elseif ($DryRun) {
    Write-Output "Dry run: $($deltas.Count) file(s) would change; nothing written"
  } else {
    if (Test-Path -LiteralPath $vendor) { Remove-Item -LiteralPath $vendor -Recurse -Force }
    New-Item -ItemType Directory -Path (Split-Path -Parent $vendor) -Force | Out-Null
    Copy-Item -LiteralPath $stage -Destination $vendor -Recurse
    Write-Output "Synced $($deltas.Count) file(s) into $vendor at $($version -join ', ')"
  }
} finally {
  Get-Item -Path "$stage*" -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
