# PKG-1: tools\package\New-ModPackage.ps1 and the shared installer engine (installer\Install.ps1, dbce\Install.ps1 in
# the zip) on a fixture game repo and a fixture game folder in %TEMP%. Packages from a committed repo only, with
# computed hashes; then install, reinstall, uninstall (keeping a changed setting), upgrade and rollback, a foreign
# dinput8.dll, links, a changed package and a file in use. Every refusal and every undo is checked byte for byte
# against a snapshot of the game folder. The installer runs in Windows PowerShell 5.1, plus once in PowerShell 7 with
# Wheelkit RepoScriptInstaller's arguments. No hash is written in this file; each is computed from the fixture.
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$packager = Join-Path $root 'tools\package\New-ModPackage.ps1'
$schema = Join-Path $root 'schema\dbce-mod.schema.json'
$ps51 = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$work = Join-Path ([IO.Path]::GetTempPath()) ('dbce-pkg-test-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-Bytes([string]$Path, [string]$Text) { [void][IO.Directory]::CreateDirectory((Split-Path $Path)); [IO.File]::WriteAllText($Path, $Text) }
# Every folder and every file with its hash, relative to $Dir: the byte-exact picture of a game folder.
function Snapshot([string]$Dir) {
    (Get-ChildItem -LiteralPath $Dir -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($Dir.Length + 1)
        if ($_.PSIsContainer) { "D $rel" } else { "F $rel $(Sha $_.FullName)" } }) -join "`n"
}
# Runs the packaged installer; returns its exit code and output.
function Invoke-Installer([string]$Package, [string[]]$Arguments, [switch]$Pwsh7) {
    $saved = $env:PSModulePath
    try {
        if ($Pwsh7) { $out = & pwsh -NoProfile -NonInteractive -File (Join-Path $Package 'dbce\Install.ps1') @Arguments 2>&1 | Out-String }
        else { $env:PSModulePath = $null; $out = & $ps51 -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $Package 'dbce\Install.ps1') @Arguments 2>&1 | Out-String }
        [pscustomobject]@{ Code = $LASTEXITCODE; Out = $out }
    } finally { $env:PSModulePath = $saved }
}
function Invoke-Git { & git -C $repo @args 2>&1 | Out-Null; if ($LASTEXITCODE) { throw "git $args failed" } }

try {
    # The fixture game repo: a committed dbce-mod.json; the "built" payload is ignored by git, as build output is.
    $repo = Join-Path $work 'repo'
    $mod = [ordered]@{
        schema = 'dbce.mod@1'; id = 'fixture-game'; name = 'Fixture Game Wheel Mod'; version = '1.0.0'; steamAppId = 1
        folder = 'Fixture Game'; exe = @('fixturegame.exe'); repository = 'd-b-c-e/dbce-mods-fixture-game'
        log = [ordered]@{ file = '%LOCALAPPDATA%\dbce\fixture\dbce-fixture.log'; start = 'dbce fixture (?<v>\S+)' }
        package = [ordered]@{
            kind = 'dinput8'; loader = $null
            payload = @([ordered]@{ from = 'build/dinput8.dll'; to = 'dinput8.dll' }, [ordered]@{ from = 'build/dbce/fixture-data.bin'; to = 'dbce/fixture-data.bin' })
            conflicts = @('dinput8.dll'); seedConfig = @([ordered]@{ from = 'config/dbce-fixture.ini'; to = 'dbce-fixture.ini' }); uiHost = 'native-panel'
        }
        settings = [ordered]@{ product = 'Fixture Game Wheel Mod'; keys = 'F6 settings'; config = 'dbce-fixture.ini'
            pages = [ordered]@{ ffb = @([ordered]@{ id = 'ffb.enabled'; kind = 'toggle'; label = 'Force feedback'; default = $true }) } }
    }
    function Set-Build([string]$Version, [string]$Dll) {
        $mod.version = $Version
        [IO.File]::WriteAllText((Join-Path $repo 'dbce-mod.json'), ($mod | ConvertTo-Json -Depth 20))
        Write-Bytes (Join-Path $repo 'build\dinput8.dll') $Dll
        Write-Bytes (Join-Path $repo 'build\dbce\fixture-data.bin') "fixture data $Version"
    }
    [void][IO.Directory]::CreateDirectory($repo)
    Invoke-Git init -q
    Invoke-Git config user.email fixture@example.invalid; Invoke-Git config user.name fixture
    Write-Bytes (Join-Path $repo '.gitignore') "build/`ndist/`n"
    Write-Bytes (Join-Path $repo 'LICENSE') 'MIT fixture license'
    Write-Bytes (Join-Path $repo 'config\dbce-fixture.ini') "[Settings]`r`nffb.enabled=1`r`n"
    Set-Build '1.0.0' 'fixture proxy dll 1.0.0'
    Invoke-Git add -A; Invoke-Git commit -q -m v1
    $head1 = (& git -C $repo rev-parse HEAD).Trim()

    $setupTool = Join-Path $work 'dbce-ui.exe'; Write-Bytes $setupTool 'fixture setup tool'
    $dist = Join-Path $work 'dist'
    & $packager -Repo $repo -Out $dist -SetupTool $setupTool | Out-Null
    $zip1 = Join-Path $dist 'Fixture Game Wheel Mod 1.0.0.zip'
    Check ([IO.File]::Exists($zip1)) 'the zip is named "<name> <version>.zip"'
    Check ((Get-Content -LiteralPath "$zip1.sha256" -Raw).StartsWith((Sha $zip1) + '  ')) 'the .sha256 beside the zip matches it'
    $pkg1 = Join-Path $work 'pkg1'; Expand-Archive -LiteralPath $zip1 -DestinationPath $pkg1
    foreach ($f in 'Install.bat', 'Uninstall.bat', 'Setup.bat', 'README.txt', 'dbce-mod.json', 'release-manifest.json', 'dbce\Install.ps1', 'dbce\DbceModFile.ps1',
        'dbce\dbce-ui.exe', 'build\dinput8.dll', 'build\dbce\fixture-data.bin', 'config\dbce-fixture.ini', 'licenses\LICENSE', 'licenses\dbce-wheel-mod-toolkit-LICENSE.txt', 'licenses\DearImGui-MIT.txt') {
        Check ([IO.File]::Exists((Join-Path $pkg1 $f))) "the zip holds $f"
    }
    $manifest = Get-Content -LiteralPath (Join-Path $pkg1 'release-manifest.json') -Raw | ConvertFrom-Json
    $all = @(Get-ChildItem -LiteralPath $pkg1 -Recurse -File | Where-Object Name -ne 'release-manifest.json')
    Check ($manifest.commit -eq $head1 -and $manifest.version -eq '1.0.0') 'release-manifest.json carries the commit and version'
    Check (@($manifest.files).Count -eq $all.Count) 'release-manifest.json lists every other file'
    Check (@($manifest.files | Where-Object { $_.sha256 -cne (Sha (Join-Path $pkg1 $_.path)) }).Count -eq 0) 'release-manifest.json hashes are the files'' own'
    Check ((Get-Content -LiteralPath (Join-Path $pkg1 'dbce-mod.json') -Raw) -ceq (Get-Content -LiteralPath (Join-Path $repo 'dbce-mod.json') -Raw)) 'the zip ships the committed dbce-mod.json unchanged'

    $refused = { param($block) try { & $block | Out-Null; $false } catch { $true } }
    Check (& $refused { & $packager -Repo $repo -Out $dist -SetupTool $setupTool }) 'the packager refuses an existing zip'
    Write-Bytes (Join-Path $repo 'LICENSE') 'edited'
    Check (& $refused { & $packager -Repo $repo -Out (Join-Path $work 'dist-dirty') -SetupTool $setupTool }) 'the packager refuses uncommitted source'
    Invoke-Git checkout -q -- LICENSE

    # The fixture game folder.
    $game = Join-Path $work 'Fixture Game'
    Write-Bytes (Join-Path $game 'fixturegame.exe') 'fixture game exe'
    Write-Bytes (Join-Path $game 'data\track.pak') 'fixture game data'
    $s0 = Snapshot $game

    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    Check ($r.Code -eq 0) 'install succeeds in Windows PowerShell 5.1'
    Check ($r.Out.Trim().Split("`n")[-1] -match '^Start Fixture Game to play') 'the last line says what to do next'
    Check ((Sha (Join-Path $game 'dinput8.dll')) -eq (Sha (Join-Path $repo 'build\dinput8.dll'))) 'install copies the payload'
    Check ([IO.File]::Exists((Join-Path $game 'dbce\fixture-data.bin')) -and [IO.File]::Exists((Join-Path $game 'dbce-fixture.ini'))) 'install copies the payload subfolder and seeds the config'
    $receiptText = Get-Content -LiteralPath (Join-Path $game 'dbce-mod.json') -Raw
    Check (Test-Json -Json $receiptText -SchemaFile $schema -ErrorAction SilentlyContinue) 'the installed dbce-mod.json matches the schema'
    $inst = ($receiptText | ConvertFrom-Json).installed
    Check ($inst.commit -eq $head1 -and $inst.version -eq '1.0.0' -and $inst.loaderInstalledByUs -eq $false) 'the installed section carries version and commit'
    Check (@($inst.files).Count -eq 3 -and @($inst.files | Where-Object { $_.sha256 -cne (Sha (Join-Path $game $_.path)) }).Count -eq 0) 'the installed section lists every file it wrote with its hash'
    $s1 = Snapshot $game

    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s1) 'reinstalling the same build changes nothing'
    $r = Invoke-Installer $pkg1 @('-Mode', 'Install', '-GameDir', $game, '-PackageRoot', $pkg1)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s1) 'reinstall with Wheelkit PackageScriptRunner''s arguments changes nothing'

    $r = Invoke-Installer $pkg1 @('-Uninstall', '-GameDir', $game)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s0) 'uninstall returns the game folder byte-exact'
    $r = Invoke-Installer $pkg1 @('-Uninstall', '-GameDir', $game)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s0) 'uninstall with nothing installed changes nothing'

    # A setting the player changed is theirs: uninstall keeps it and removes the rest.
    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    Write-Bytes (Join-Path $game 'dbce-fixture.ini') "[Settings]`r`nffb.enabled=0`r`n"
    $edited = Sha (Join-Path $game 'dbce-fixture.ini')
    $r = Invoke-Installer $pkg1 @('-Mode', 'Uninstall', '-GameDir', $game, '-PackageRoot', $pkg1)
    $keptIni = [IO.File]::Exists((Join-Path $game 'dbce-fixture.ini')) -and (Sha (Join-Path $game 'dbce-fixture.ini')) -eq $edited
    Remove-Item -LiteralPath (Join-Path $game 'dbce-fixture.ini') -ErrorAction SilentlyContinue
    Check ($r.Code -eq 0 -and $keptIni -and (Snapshot $game) -ceq $s0) 'uninstall keeps a changed setting and removes everything else'

    # Upgrade, then rollback twice: back to 1.0.0 exactly, then to no mod exactly.
    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    $s1 = Snapshot $game
    Set-Build '1.1.0' 'fixture proxy dll 1.1.0'
    Invoke-Git add -A; Invoke-Git commit -q -m v2
    & $packager -Repo $repo -Out $dist -SetupTool $setupTool | Out-Null
    $pkg2 = Join-Path $work 'pkg2'; Expand-Archive -LiteralPath (Join-Path $dist 'Fixture Game Wheel Mod 1.1.0.zip') -DestinationPath $pkg2
    $r = Invoke-Installer $pkg2 @('-GameDir', $game)
    Check ($r.Code -eq 0 -and (Sha (Join-Path $game 'dinput8.dll')) -eq (Sha (Join-Path $repo 'build\dinput8.dll'))) 'installing 1.1.0 over 1.0.0 replaces the payload'
    $r = Invoke-Installer $pkg2 @('-Rollback', '-GameDir', $game)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s1) 'rollback returns the exact 1.0.0 install'
    $r = Invoke-Installer $pkg1 @('-Mode', 'Rollback', '-GameDir', $game)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s0) 'rollback of the first install returns the game folder byte-exact'
    $r = Invoke-Installer $pkg1 @('-Rollback', '-GameDir', $game)
    Check ($r.Code -ne 0 -and (Snapshot $game) -ceq $s0) 'rollback with nothing to go back to refuses and changes nothing'

    # Refusals leave the game folder exactly as it was.
    Write-Bytes (Join-Path $game 'dinput8.dll') 'another mod''s dinput8.dll'
    $sForeign = Snapshot $game
    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    Check ($r.Code -ne 0 -and $r.Out -match 'belongs to another mod' -and (Snapshot $game) -ceq $sForeign) 'a foreign dinput8.dll is refused and nothing changes'
    Remove-Item -LiteralPath (Join-Path $game 'dinput8.dll')

    $link = Join-Path $work 'Linked Game'
    New-Item -ItemType Junction -Path $link -Target $game | Out-Null
    $r = Invoke-Installer $pkg1 @('-GameDir', $link)
    Check ($r.Code -ne 0 -and $r.Out -match 'is a link' -and (Snapshot $game) -ceq $s0) 'a game folder reached through a junction is refused and nothing changes'
    [IO.Directory]::Delete($link)
    $elsewhere = Join-Path $work 'elsewhere'; [void][IO.Directory]::CreateDirectory($elsewhere)
    New-Item -ItemType Junction -Path (Join-Path $game 'dbce') -Target $elsewhere | Out-Null
    $sLinked = Snapshot $game
    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    Check ($r.Code -ne 0 -and $r.Out -match 'is a link' -and (Snapshot $game) -ceq $sLinked -and -not (Get-ChildItem -LiteralPath $elsewhere -Force)) 'a payload folder that is a junction is refused and nothing changes'
    [IO.Directory]::Delete((Join-Path $game 'dbce'))

    $pkgBad = Join-Path $work 'pkg-bad'; Copy-Item -LiteralPath $pkg1 -Destination $pkgBad -Recurse
    Write-Bytes (Join-Path $pkgBad 'build\dinput8.dll') 'changed after packaging'
    $r = Invoke-Installer $pkgBad @('-GameDir', $game)
    Check ($r.Code -ne 0 -and $r.Out -match 'incomplete or changed' -and (Snapshot $game) -ceq $s0) 'a changed package file is refused and nothing changes'

    # A write that fails halfway puts everything back: a game file the payload replaces is held open.
    Write-Bytes (Join-Path $game 'dbce\fixture-data.bin') 'the game''s own file'
    $sBusy = Snapshot $game
    $lock = [IO.File]::Open((Join-Path $game 'dbce\fixture-data.bin'), 'Open', 'Read', 'Read')
    try { $r = Invoke-Installer $pkg1 @('-GameDir', $game) } finally { $lock.Dispose() }
    Check ($r.Code -ne 0 -and $r.Out -match 'put back' -and (Snapshot $game) -ceq $sBusy) 'a write that fails puts every file back'
    # The game file the mod replaces comes back on uninstall.
    $r = Invoke-Installer $pkg1 @('-GameDir', $game)
    Check ($r.Code -eq 0 -and (Get-Content -LiteralPath (Join-Path $game 'dbce\fixture-data.bin') -Raw) -ceq 'fixture data 1.0.0') 'install replaces a game file it ships'
    $r = Invoke-Installer $pkg1 @('-Uninstall', '-GameDir', $game)
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $sBusy) 'uninstall puts back the game file it replaced, byte-exact'
    Remove-Item -LiteralPath (Join-Path $game 'dbce') -Recurse

    # Wheelkit's RepoScriptInstaller runs the script in PowerShell 7 with -GameDir and -Uninstall.
    $r = Invoke-Installer $pkg1 @('-GameDir', $game) -Pwsh7
    Check ($r.Code -eq 0 -and [IO.File]::Exists((Join-Path $game 'dinput8.dll'))) 'install succeeds in PowerShell 7'
    $r = Invoke-Installer $pkg1 @('-GameDir', $game, '-Uninstall') -Pwsh7
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s0) 'uninstall in PowerShell 7 returns the game folder byte-exact'
} finally {
    if ($lock) { $lock.Dispose() }
    if ([IO.Directory]::Exists($work)) {
        foreach ($j in @(Get-ChildItem -LiteralPath $work -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue)) { [IO.Directory]::Delete($j.FullName) }
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
if ($failed) { throw "$failed package/installer check(s) failed" }
'package and installer checks passed'
