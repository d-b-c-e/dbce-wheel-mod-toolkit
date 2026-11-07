#Requires -Version 7.0
<# Checks one built mod zip on a fixture game folder in %TEMP% (PKG-2): the game's exe names from the zip's
   dbce-mod.json as marker files, plus a settings file and a save that must never change. Install as Install.bat runs
   it (Windows PowerShell 5.1), reinstall, then uninstall as Wheelkit's RepoScriptInstaller runs it (PowerShell 7,
   -NonInteractive, -GameDir and -Uninstall), each compared byte for byte with a snapshot of the folder. For each
   package.oldReceipts entry it also takes over an old install, rolls back to it and uninstalls it. No game runs.
   Usage: pwsh <toolkit>\tools\package\Test-ModZip.ps1 -Zip "<repo>\dist\<name> <version>.zip" #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Zip)
$ErrorActionPreference = 'Stop'
$ps51 = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$work = Join-Path ([IO.Path]::GetTempPath()) ('dbce-zip-test-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-Text([string]$Path, [string]$Text) { [void][IO.Directory]::CreateDirectory((Split-Path $Path)); [IO.File]::WriteAllText($Path, $Text) }
function Snapshot([string]$Dir) {
    (Get-ChildItem -LiteralPath $Dir -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($Dir.Length + 1)
        if ($_.PSIsContainer) { "D $rel" } else { "F $rel $(Sha $_.FullName)" } }) -join "`n"
}
function Run([string[]]$Arguments, [switch]$Wheelkit) {
    $script = Join-Path $pkg 'dbce\Install.ps1'
    if ($Wheelkit) { $out = & pwsh -NoProfile -NonInteractive -File $script @Arguments 2>&1 | Out-String }
    else { $saved = $env:PSModulePath; try { $env:PSModulePath = $null; $out = & $ps51 -NoProfile -ExecutionPolicy Bypass -File $script @Arguments 2>&1 | Out-String } finally { $env:PSModulePath = $saved } }
    [pscustomobject]@{ Code = $LASTEXITCODE; Out = $out.Trim() }
}
try {
    $pkg = Join-Path $work 'package'
    Expand-Archive -LiteralPath $Zip -DestinationPath $pkg
    $mod = Get-Content -LiteralPath (Join-Path $pkg 'dbce-mod.json') -Raw | ConvertFrom-Json
    $game = Join-Path $work $mod.folder
    foreach ($exe in $mod.exe) { Write-Text (Join-Path $game $exe) 'fixture exe, never run' }
    Write-Text (Join-Path $game 'fixture-settings.ini') "[Kept]`r`nValue=1`r`n"
    Write-Text (Join-Path $game 'saves\fixture.sav') 'fixture save'
    $s0 = Snapshot $game

    $r = Run @('-GameDir', $game)
    Check ($r.Code -eq 0) "install as Install.bat runs it: $(($r.Out -split "`n")[-1])"
    foreach ($p in $mod.package.payload) {
        Check ((Sha (Join-Path $game $p.to)) -eq (Sha (Join-Path $pkg $p.from))) "install copies $($p.from) to $($p.to)"
    }
    $inst = (Get-Content -LiteralPath (Join-Path $game 'dbce-mod.json') -Raw | ConvertFrom-Json).installed
    Check ($inst.version -eq $mod.version -and @($inst.files).Count -eq @($mod.package.payload).Count) 'the game folder''s dbce-mod.json has the install record'
    $s1 = Snapshot $game
    $r = Run @('-GameDir', $game) -Wheelkit
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s1) 'reinstall with RepoScriptInstaller''s arguments changes nothing'
    $r = Run @('-GameDir', $game, '-Uninstall') -Wheelkit
    Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s0) 'uninstall with RepoScriptInstaller''s arguments returns the game folder byte-exact'

    # An install by the game's earlier installer: its file, its {file, sha256} receipt and its backups folder.
    $first = @($mod.package.payload)[0]
    foreach ($o in @($mod.package.oldReceipts)) {
        $old = Join-Path $game $first.to
        Write-Text $old 'the old installer''s build'
        Write-Text (Join-Path $game $o.receipt) ('{"file":"' + $first.to + '","sha256":"' + (Sha $old).ToUpperInvariant() + '"}')
        if ($o.backups) { Write-Text (Join-Path $game "$($o.backups)\20260101-000000-old\receipt.json") 'an earlier receipt' }
        $sOld = Snapshot $game
        $r = Run @('-GameDir', $game)
        Check ($r.Code -eq 0 -and (Sha $old) -eq (Sha (Join-Path $pkg $first.from)) -and -not [IO.File]::Exists((Join-Path $game $o.receipt))) "install takes over the old $($o.receipt) install"
        $r = Run @('-GameDir', $game, '-Rollback')
        Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $sOld) 'rollback returns the old install byte-exact'
        $r = Run @('-GameDir', $game)
        $r = Run @('-GameDir', $game, '-Uninstall') -Wheelkit
        Check ($r.Code -eq 0 -and (Snapshot $game) -ceq $s0) 'uninstall after the takeover returns the game folder byte-exact'
    }
} finally {
    if ([IO.Directory]::Exists($work)) { Remove-Item -LiteralPath $work -Recurse -Force }
}
if ($failed) { throw "$failed zip check(s) failed" }
"zip checks passed: $([IO.Path]::GetFileName($Zip))"
