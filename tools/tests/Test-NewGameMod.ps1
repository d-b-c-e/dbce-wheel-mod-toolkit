# The new-game template builds and starts: New-GameMod.ps1 creates a throwaway mod in %TEMP% from the toolkit's current
# commit, and that mod's own build.ps1 must pass without an edit (vendor check, six exports, the start check under the
# game's exe name with the force fence on). Its dbce-mod.json must read back through the shared reader. No game or device.
# The template is taken from HEAD, so commit template or core changes before running this.
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$work = Join-Path ([IO.Path]::GetTempPath()) ('dbce-new-game-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
try {
    $repo = Join-Path $work 'dbce-mods-templatecheck'
    $head = (& git -C $root rev-parse HEAD).Trim()
    $out = & pwsh -NoProfile -File (Join-Path $root 'tools\New-GameMod.ps1') -Id templatecheck -Name 'Template Check' -Exe TemplateCheck.exe -Folder 'Template Check' -SteamAppId 480 -Out $repo -Commit $head 2>&1
    Check ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath (Join-Path $repo 'src\vendor\VERSION'))) "New-GameMod creates the repo with the toolkit vendored ($($out | Select-Object -Last 1))"
    Check (-not (& git -C $repo status --porcelain)) 'the new repo is one clean commit'
    Check (-not (Select-String -Path (Join-Path $repo 'src\proxy.cpp'), (Join-Path $repo 'tests\host.cpp'), (Join-Path $repo 'build.ps1') -Pattern '__[A-Z]+__' -Quiet)) 'no template placeholder is left'

    . (Join-Path $root 'installer\DbceModFile.ps1')
    $mod = $null
    try { $mod = Read-DbceModFile -Path (Join-Path $repo 'dbce-mod.json') } catch { "  $($_.Exception.Message)" }
    Check ($null -ne $mod -and $mod.Id -eq 'templatecheck' -and $mod.Package.Kind -eq 'dinput8') 'dbce-mod.json reads back through the shared reader'

    # The toolkit folder name in the temp tree differs, so the template's vendor check is skipped there; run it here.
    & pwsh -NoProfile -File (Join-Path $root 'tools\Sync-Native.ps1') -GameRepo $repo -Check | Out-Null
    Check ($LASTEXITCODE -eq 0) 'the vendored copy matches its VERSION'
    $build = & pwsh -NoProfile -File (Join-Path $repo 'build.ps1') 2>&1
    $code = $LASTEXITCODE
    if ($code) { $build | Select-Object -Last 25 }
    Check ($code -eq 0 -and ($build -match 'build and offline tests passed')) "the new mod's build.ps1 passes without an edit"
    Check ([bool]($build -match 'ok   the game was given the fenced DirectInput')) 'the start check ran under the force fence'
} finally {
    if ([IO.Directory]::Exists($work)) { Remove-Item -LiteralPath $work -Recurse -Force }
    $log = Join-Path $env:LOCALAPPDATA 'dbce\templatecheck'
    if ([IO.Directory]::Exists($log)) { Remove-Item -LiteralPath $log -Recurse -Force }
}
if ($failed) { throw "$failed new-game check(s) failed" }
'new-game template checks passed'
