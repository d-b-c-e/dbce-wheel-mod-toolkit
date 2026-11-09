# CFG-3S: installer\Setup-Wheelkit.ps1 (the Wheelkit half of the package's setup) on a fixture Wheelkit package in
# %TEMP%. Covers the three cases the package's setup must handle: Wheelkit missing (installs), installed and older
# than the bundle (updates), and installed equal or newer (leaves it alone). -Target keeps the real
# %LOCALAPPDATA%\Programs\Wheelkit untouched. The installer runs in Windows PowerShell 5.1, as Install.bat runs it.
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$setup = Join-Path $root 'installer\Setup-Wheelkit.ps1'
$ps51 = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$work = Join-Path ([IO.Path]::GetTempPath()) ('dbce-setup-wheelkit-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:failed++ } }
function Write-Text([string]$Path, [string]$Text) { [void][IO.Directory]::CreateDirectory((Split-Path $Path)); [IO.File]::WriteAllText($Path, $Text) }
function Make-WheelkitZip([string]$Path, [string]$Version) {
    $dir = Join-Path $work ('wk-src-' + [guid]::NewGuid().ToString('N').Substring(0, 6))
    Write-Text (Join-Path $dir 'Wheelkit.exe') "fixture Wheelkit $Version"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($dir, $Path)
}
function Run([string[]]$Arguments) {
    $out = & $ps51 -NoProfile -ExecutionPolicy Bypass -File $setup @Arguments 2>&1 | Out-String
    [pscustomobject]@{ Code = $LASTEXITCODE; Out = $out.Trim() }
}
function Marker([string]$Target) { Get-Content -LiteralPath (Join-Path $Target 'wheelkit-install.json') -Raw | ConvertFrom-Json }
try {
    $target = Join-Path $work 'Programs\Wheelkit'

    # 1. Missing: installs and records the bundled version.
    $pkg80 = Join-Path $work 'Wheelkit-0.6.80-win-x64.zip'; Make-WheelkitZip $pkg80 '0.6.80'
    $r = Run @('-Package', $pkg80, '-Target', $target)
    Check ($r.Code -eq 0 -and [IO.File]::Exists((Join-Path $target 'Wheelkit.exe'))) 'a missing Wheelkit installs'
    Check ((Marker $target).version -eq '0.6.80') 'the install records the bundled version'

    # 2. Older: updates to the bundled one.
    $pkg82 = Join-Path $work 'Wheelkit-0.6.82-win-x64.zip'; Make-WheelkitZip $pkg82 '0.6.82'
    $r = Run @('-Package', $pkg82, '-Target', $target)
    Check ($r.Code -eq 0 -and (Marker $target).version -eq '0.6.82') 'an older installed Wheelkit updates to the bundle'

    # 3. Equal: leaves it alone.
    $before = Get-Content -LiteralPath (Join-Path $target 'wheelkit-install.json') -Raw
    $r = Run @('-Package', $pkg82, '-Target', $target)
    Check ($r.Code -eq 0 -and $r.Out -match 'already up to date' -and (Get-Content -LiteralPath (Join-Path $target 'wheelkit-install.json') -Raw) -ceq $before) 'an equal installed Wheelkit is left alone'

    # 4. Newer installed: leaves it alone.
    $pkg79 = Join-Path $work 'Wheelkit-0.6.79-win-x64.zip'; Make-WheelkitZip $pkg79 '0.6.79'
    $r = Run @('-Package', $pkg79, '-Target', $target)
    Check ($r.Code -eq 0 -and $r.Out -match 'already up to date') 'a newer installed Wheelkit is left alone'

    # 5. A folder we did not install is refused and left alone.
    $foreign = Join-Path $work 'foreign'; Write-Text (Join-Path $foreign 'owner.txt') 'keep'
    $r = Run @('-Package', $pkg82, '-Target', $foreign)
    Check ($r.Code -ne 0 -and [IO.File]::Exists((Join-Path $foreign 'owner.txt'))) 'a non-Wheelkit folder is refused and left alone'
} finally {
    if ([IO.Directory]::Exists($work)) { Remove-Item -LiteralPath $work -Recurse -Force }
}
if ($failed) { throw "$failed setup-wheelkit check(s) failed" }
'setup-wheelkit checks passed'
