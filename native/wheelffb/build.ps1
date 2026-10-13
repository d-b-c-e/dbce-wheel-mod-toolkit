<#
.SYNOPSIS
    Builds WheelFfb.dll (x64) and verifies every export named in WheelFfb.def.

.DESCRIPTION
    The compile lives in build.bat (MSVC environment setup and quoting are a
    minefield from PowerShell). This wrapper verifies the exports with dumpbin:
    a mangled or missing export fails silently at runtime as an
    EntryPointNotFoundException deep inside a game, which looks like "no FFB".

    vcvars64.bat may print "'vswhere.exe' is not recognized" on some machines;
    that comes from inside Microsoft's script and is harmless.

.PARAMETER CopyTo
    Folder to copy the built DLL into (a consumer's vendored lib folder or a
    game's plugin folder).

.PARAMETER As
    File name to copy it as, when a game dictates the name it P/Invokes
    (art of rally: UnityForceFeedback.dll). Default WheelFfb.dll.

.PARAMETER Arch
    x64 (default) or x86.

.PARAMETER NoForce
    Builds the internal test flavor (WHEELFFB_NO_FORCE_BUILD): every force export refuses from process load, reads and
    test injection work, the export set is unchanged. Output goes to build\noforce[\x86], linked deterministically,
    with WheelFfb.noforce.provenance.json beside it (DLL SHA-256, commit, dirty flag, toolchain, flags, and the SHA-256
    of every source file). It is for internal, hash-pinned, temporary test deployments that restore the product
    afterwards: never a release, a product pin or a lasting install. -CopyTo is refused; the test runner that owns the
    deployment copies it itself.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -CopyTo "D:\...\artofrally\Mods\ArtOfSimRally" -As UnityForceFeedback.dll
    .\build.ps1 -NoForce -Arch x86
#>
[CmdletBinding()]
param(
    [string]$CopyTo,
    [string]$As = 'WheelFfb.dll',
    [ValidateSet('x64', 'x86')][string]$Arch = 'x64',
    [switch]$NoForce
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($NoForce -and $CopyTo) { throw '-NoForce builds are for internal test deployments: the runner that owns the deployment copies the DLL itself.' }
$outDir = Join-Path $here ('build' + $(if ($NoForce) { '\noforce' } else { '' }) + $(if ($Arch -eq 'x86') { '\x86' } else { '' }))
$dll  = Join-Path $outDir 'WheelFfb.dll'
$def  = Join-Path $here 'WheelFfb.def'

$batArgs = @($Arch) + $(if ($NoForce) { @('noforce') } else { @() })
Write-Host "Building $Arch WheelFfb.dll$(if ($NoForce) { ' (no-force build)' })..." -ForegroundColor Cyan
& cmd.exe /c "$here\build.bat" @batArgs
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
if (-not (Test-Path $dll)) { throw "Build reported success but $dll is missing" }
Write-Host "Built $dll" -ForegroundColor Green

# Every name in the .def is part of the C ABI contract; verify all of them.
$required = Get-Content $def | Where-Object { $_ -match '^\s+[A-Za-z_][A-Za-z0-9_]*\s*$' } | ForEach-Object { $_.Trim() }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath  = & $vswhere -latest -products * -property installationPath
    $dumpbin = Get-ChildItem "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue |
               Select-Object -First 1 -ExpandProperty FullName
    if ($dumpbin) {
        $exports = (& $dumpbin /exports $dll) -join "`n"
        $missing = $required | Where-Object { $exports -notmatch "\s$_\s*(`n|$)" }
        if ($missing) { throw "DLL is missing exports named in WheelFfb.def: $($missing -join ', ')" }
        Write-Host "All $($required.Count) exports present and undecorated ($Arch)." -ForegroundColor Green
        if ($NoForce) {
            # Drop-in for the product: exactly the .def's exports, nothing more (the marker is data, not an export).
            $names = @([regex]::Matches($exports, '(?m)^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)') | ForEach-Object { $_.Groups[1].Value })
            $extra = @($names | Where-Object { $required -notcontains $_ })
            if ($extra -or $names.Count -ne $required.Count) { throw "No-force DLL exports differ from WheelFfb.def: $($names.Count) exported, extra: $($extra -join ', ')" }
            Write-Host "No-force export set equals WheelFfb.def exactly ($($names.Count))." -ForegroundColor Green
        }
    } else { if ($NoForce) { throw 'dumpbin not found; a no-force build must verify its exports' }; Write-Warning "dumpbin not found; exports not verified" }
} else { if ($NoForce) { throw 'vswhere not found; a no-force build must verify its exports' }; Write-Warning "vswhere not found; exports not verified" }

# The C header must list every export too - a native consumer resolves by name.
$header = Get-Content (Join-Path $here 'include\wheelffb.h') -Raw
$notInHeader = $required | Where-Object { $header -notmatch "X\([a-z]+,\s*$_\s*," }
if ($notInHeader) { throw "include\wheelffb.h is missing exports named in WheelFfb.def: $($notInHeader -join ', ')" }
Write-Host "include\wheelffb.h lists all $($required.Count) exports." -ForegroundColor Green

# The marker string identifies a no-force file; a product build must never carry it.
$marker = 'DBCE-WHEELFFB-NO-FORCE-TEST-BUILD'
$hasMarker = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($dll)).Contains($marker)
if ($hasMarker -ne [bool]$NoForce) { throw "$dll $(if ($NoForce) { 'lacks' } else { 'carries' }) the no-force marker" }

if ($NoForce) {
    function Get-Sha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
    # Every source the DLL is compiled from: WheelFfb.cpp's quoted-include closure, the .def and build.bat.
    $sources = [Collections.Generic.List[string]]::new()
    $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue((Join-Path $here 'WheelFfb.cpp'))
    while ($pending.Count) {
        $file = [IO.Path]::GetFullPath($pending.Dequeue())
        if ($sources.Contains($file)) { continue }
        $sources.Add($file)
        foreach ($m in [regex]::Matches([IO.File]::ReadAllText($file), '(?m)^\s*#\s*include\s+"([^"]+)"')) {
            $found = @((Split-Path -Parent $file), $here) | ForEach-Object { Join-Path $_ $m.Groups[1].Value } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
            if (-not $found) { throw "Cannot resolve #include `"$($m.Groups[1].Value)`" from $file" }
            $pending.Enqueue($found)
        }
    }
    $sources.Add([IO.Path]::GetFullPath($def)); $sources.Add([IO.Path]::GetFullPath((Join-Path $here 'build.bat')))
    $repo = [IO.Path]::GetFullPath((& git -C $here rev-parse --show-toplevel).Trim()).TrimEnd('\')
    $relative = @($sources | ForEach-Object {
        if (-not $_.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "$_ is outside the repository" }
        $_.Substring($repo.Length + 1).Replace('\', '/') } | Sort-Object)
    $dirty = @(& git -C $repo status --porcelain -- @relative)
    $toolchain = @(Get-Content -LiteralPath (Join-Path $outDir 'toolchain.txt') | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^VCToolsVersion=.+|^Microsoft \(R\) C/C\+\+' })
    if ($toolchain.Count -ne 2) { throw "toolchain.txt does not hold VCToolsVersion and the compiler banner" }
    $provenance = [ordered]@{
        flavor          = 'no-force test build: internal, hash-pinned, temporary test deployments only; never a release, product pin or lasting install'
        define          = 'WHEELFFB_NO_FORCE_BUILD'
        arch            = $Arch
        dll             = 'WheelFfb.dll'
        dllSha256       = Get-Sha256 $dll
        marker          = $marker
        firstCallStatus = 'GetTestInjectionStatus: armed: no force (no-force build); ...'
        commit          = (& git -C $repo rev-parse HEAD).Trim()
        sourcesDirty    = [bool]$dirty.Count
        dirtyEntries    = $dirty
        toolchain       = $toolchain
        compile         = "cl /nologo /std:c++17 /LD /O2 /W4 /EHsc /DNDEBUG /DWHEELFFB_NO_FORCE_BUILD /Brepro WheelFfb.cpp /link /DEF:WheelFfb.def /MACHINE:$Arch /Brepro"
        sources         = [ordered]@{}
        builtUtc        = [DateTime]::UtcNow.ToString('o')
    }
    foreach ($r in $relative) { $provenance.sources[$r] = Get-Sha256 (Join-Path $repo $r) }
    $provenance | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outDir 'WheelFfb.noforce.provenance.json') -Encoding utf8
    Write-Host "No-force build: marker present, provenance written ($($relative.Count) sources, dirty=$([bool]$dirty.Count))." -ForegroundColor Green
}

if ($CopyTo) {
    if (-not (Test-Path $CopyTo)) { throw "Destination folder not found: $CopyTo" }
    $dest = Join-Path $CopyTo $As
    Copy-Item $dll $dest -Force
    Write-Host "Copied to $dest" -ForegroundColor Green
}
