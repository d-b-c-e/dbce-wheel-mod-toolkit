# Builds the shared input shim's DLL and runs its checks (native/shim; see PROVENANCE.md). x64, clang-cl: the toolset
# the consumers' dbce_shim.dll was built with. No game and no device: the tests use the scripted provider and a fake
# DirectInput. -Crt MT (default) links the C runtime statically: the ABI passes no allocation or CRT object across the
# boundary (README.md). -Crt MD reproduces the earlier dynamic runtime for comparison.
# -Arch x86 builds the ordinary-only DLL for 32-bit games (shim/ordinary_only.hpp; shim_dll/README.md): the same
# clang-cl for i686-pc-windows-msvc under vcvars32, into build\x86-<crt>, with the ordinary and refusal tests. The
# copied-input bridge test is x64-only. Both architectures check the DLL's machine, imports and export names.
param([ValidateSet('MT', 'MD')][string]$Crt = 'MT', [ValidateSet('x64', 'x86')][string]$Arch = 'x64')
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$clang = Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang-cl.exe'
if (!(Test-Path -LiteralPath $clang)) { throw "clang-cl not found ($clang): install the Visual Studio C++ Clang tools" }
$vcvars = Join-Path $vs $(if ($Arch -eq 'x86') { 'VC\Auxiliary\Build\vcvars32.bat' } else { 'VC\Auxiliary\Build\vcvars64.bat' })
$here = $PSScriptRoot
$dir = Join-Path $here ("build\$Arch-" + $Crt.ToLowerInvariant())
New-Item -ItemType Directory -Force $dir | Out-Null

# The decomp's CMake Release flags (ClangCL), per target. x64 passes no target, exactly as before.
$target = if ($Arch -eq 'x86') { ' --target=i686-pc-windows-msvc' } else { '' }
$common = "/nologo$target /D WIN32 /D _WINDOWS /EHsc /O2 /Ob2 /D NDEBUG /std:c++17 /$Crt /W4"
$win = '/D NOMINMAX /D WIN32_LEAN_AND_MEAN /D _CRT_SECURE_NO_WARNINGS'
$inc = '/I shim /I host /I host\recorder /I shim_dll /I tests /I ..\controls /I ..\session'
# The DirectInput provider and the copied-input bridge (shim\bridge_provider; docs\COPIED-INPUT-BRIDGE.md) are both in the DLL.
$dllSources = 'shim_dll\dbce_shim.cpp shim\core.cpp shim\parse.cpp shim\dinput_provider.cpp shim\bridge_provider.cpp'
$libs = 'dinput8.lib dxguid.lib bcrypt.lib'

function Invoke-Step([string]$name, [string]$line) {
    $obj = Join-Path $dir $name
    New-Item -ItemType Directory -Force $obj | Out-Null
    $objArg = '"' + $obj + '\\"'
    $resolved = $line.Replace('{obj}', $objArg)
    $out = cmd /c "`"$vcvars`" >nul && cd /d `"$here`" && $resolved" 2>&1
    $code = $LASTEXITCODE
    $out | Select-String -Pattern ' warning| error|FAIL|PASS|passed|checks' | ForEach-Object { "${name}: $($_.Line.Trim())" }
    if ($code) { $out | Select-Object -Last 30; throw "$name failed (exit $code)" }
}

$dll = Join-Path $dir 'dbce_shim.dll'
Invoke-Step 'dbce_shim' "`"$clang`" $common /WX $win $inc /LD $dllSources /Fo{obj} /Fe`"$dll`" /link $libs"
Invoke-Step 'test_shim' "`"$clang`" $common $inc tests\test_shim.cpp shim\parse.cpp shim\core.cpp /Fo{obj} /Fe`"$dir\test_shim.exe`" && `"$dir\test_shim.exe`""
Invoke-Step 'test_dinput_provider' "`"$clang`" $common $win $inc tests\test_dinput_provider.cpp shim\dinput_provider.cpp shim\parse.cpp shim\core.cpp /Fo{obj} /Fe`"$dir\test_dinput_provider.exe`" /link dinput8.lib dxguid.lib && `"$dir\test_dinput_provider.exe`""
Invoke-Step 'test_provider_host' "`"$clang`" $common /WX $win $inc tests\test_provider_host.cpp shim\core.cpp shim\parse.cpp shim\dinput_provider.cpp /Fo{obj} /Fe`"$dir\test_provider_host.exe`" /link $libs && `"$dir\test_provider_host.exe`""
Invoke-Step 'test_shim_abi'"`"$clang`" $common /WX $win /D DBCE_SHIM_BUILD $inc tests\test_shim_abi.cpp $dllSources /Fo{obj} /Fe`"$dir\test_shim_abi.exe`" /link $libs && `"$dir\test_shim_abi.exe`" `"$dll`""
if ($Arch -eq 'x64') { Invoke-Step 'test_shim_bridge' "`"$clang`" $common /WX $win /D DBCE_SHIM_BUILD $inc tests\test_shim_bridge.cpp $dllSources /Fo{obj} /Fe`"$dir\test_shim_bridge.exe`" /link $libs && `"$dir\test_shim_bridge.exe`"" }
else { 'test_shim_bridge: x64 only (on x86 the bridge open refuses; test_shim_abi checks that)' }

# What the DLL needs at run time, and its identity.
$dump = cmd /c "`"$vcvars`" >nul && dumpbin /nologo /dependents `"$dll`"" 2>&1
if ($LASTEXITCODE) { $dump | Select-Object -Last 10; throw "dumpbin failed (exit $LASTEXITCODE): the imports were not inspected" }
$deps = @($dump | Where-Object { $_ -match '^\s+\S+\.dll\s*$' } | ForEach-Object { $_.Trim() })
# An empty or partial listing proves nothing: the imports this DLL must have are the check's own evidence.
$missing = @('KERNEL32.dll', 'DINPUT8.dll', 'bcrypt.dll' | Where-Object { $deps -notcontains $_ })
if ($missing.Count) { throw "the import listing lacks expected Windows imports ($($missing -join ', ')): got '$($deps -join ', ')'" }
$crtDlls = @($deps | Where-Object { $_ -match '^(VCRUNTIME|MSVCP|api-ms-win-crt-|ucrtbase)' })
if ($Crt -eq 'MT' -and $crtDlls.Count) { throw "a static-CRT build imports the C runtime: $($crtDlls -join ', ')" }
# The machine, and the export names against every DBCE_SHIM_API declaration in both headers: none missing, none
# extra, so a decorated name (_name, name@N) fails as both.
$info = cmd /c "`"$vcvars`" >nul && dumpbin /nologo /headers /exports `"$dll`"" 2>&1
if ($LASTEXITCODE) { $info | Select-Object -Last 10; throw "dumpbin failed (exit $LASTEXITCODE): the headers and exports were not inspected" }
$machine = if ($Arch -eq 'x86') { '14C machine (x86)' } else { '8664 machine (x64)' }
if (-not @($info | Where-Object { "$_".Trim() -ceq $machine }).Count) { throw "the DLL's header has no '$machine' line" }
$exports = @($info | ForEach-Object { if ("$_" -cmatch '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)\s*$') { $Matches[1] } })
$declared = @(foreach ($h in 'shim_dll\dbce_shim.h', 'shim_dll\dbce_shim_bridge.h') {
    [regex]::Matches([IO.File]::ReadAllText((Join-Path $here $h)), 'DBCE_SHIM_API\s+[\w\s\*]+?\b(dbce_\w+)\s*\(') | ForEach-Object { $_.Groups[1].Value } })
$missingExports = @($declared | Where-Object { $exports -cnotcontains $_ }); $extraExports = @($exports | Where-Object { $declared -cnotcontains $_ })
if (-not $declared.Count -or $missingExports.Count -or $extraExports.Count) {
    throw "the exports differ from the headers: missing '$($missingExports -join ', ')', extra '$($extraExports -join ', ')'"
}
# The linked sources: this folder plus the toolkit's own controls and session headers.
$sourceDirs = $here, (Join-Path $here '..\controls'), (Join-Path $here '..\session')
$dirty = @(foreach ($d in $sourceDirs) { git -C $here status --porcelain -- (Resolve-Path $d).Path })
$receipt = [ordered]@{
    dll = 'dbce_shim.dll'; arch = $Arch; ordinaryOnly = $Arch -eq 'x86'; sha256 = (Get-FileHash -LiteralPath $dll).Hash; crt = $Crt; dependents = @($deps)
    exports = @($exports)
    compiler = ((& $clang --version) | Select-Object -First 1); toolkitCommit = (git -C $here rev-parse HEAD)
    sources = 'native/shim, native/controls, native/session'; sourceChanged = [bool]$dirty.Count; changedFiles = $dirty
}
$receipt | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $dir 'build-receipt.json')
"dbce_shim.dll ($Arch, $Crt) $($receipt.sha256); imports: $($deps -join ', '); exports: $($exports.Count), as declared"
"PASS native/shim build and checks ($Arch)"
