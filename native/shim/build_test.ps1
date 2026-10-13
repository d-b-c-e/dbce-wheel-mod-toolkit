# Builds the shared input shim's DLL and runs its checks (native/shim; see PROVENANCE.md). x64, clang-cl: the toolset
# the consumers' dbce_shim.dll was built with. No game and no device: the tests use the scripted provider and a fake
# DirectInput. -Crt MT (default) links the C runtime statically: the ABI passes no allocation or CRT object across the
# boundary (README.md). -Crt MD reproduces the earlier dynamic runtime for comparison.
param([ValidateSet('MT', 'MD')][string]$Crt = 'MT')
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$clang = Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang-cl.exe'
if (!(Test-Path -LiteralPath $clang)) { throw "clang-cl not found ($clang): install the Visual Studio C++ Clang tools" }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$here = $PSScriptRoot
$dir = Join-Path $here ("build\x64-" + $Crt.ToLowerInvariant())
New-Item -ItemType Directory -Force $dir | Out-Null

# The decomp's CMake Release flags (ClangCL), per target.
$common = "/nologo /D WIN32 /D _WINDOWS /EHsc /O2 /Ob2 /D NDEBUG /std:c++17 /$Crt /W4"
$win = '/D NOMINMAX /D WIN32_LEAN_AND_MEAN /D _CRT_SECURE_NO_WARNINGS'
$inc = '/I shim /I host /I host\recorder /I shim_dll /I tests /I ..\controls /I ..\session'
$dllSources = 'shim_dll\dbce_shim.cpp shim\core.cpp shim\parse.cpp shim\dinput_provider.cpp'
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

# What the DLL needs at run time, and its identity.
$dump = cmd /c "`"$vcvars`" >nul && dumpbin /nologo /dependents `"$dll`"" 2>&1
if ($LASTEXITCODE) { $dump | Select-Object -Last 10; throw "dumpbin failed (exit $LASTEXITCODE): the imports were not inspected" }
$deps = @($dump | Where-Object { $_ -match '^\s+\S+\.dll\s*$' } | ForEach-Object { $_.Trim() })
# An empty or partial listing proves nothing: the imports this DLL must have are the check's own evidence.
$missing = @('KERNEL32.dll', 'DINPUT8.dll', 'bcrypt.dll' | Where-Object { $deps -notcontains $_ })
if ($missing.Count) { throw "the import listing lacks expected Windows imports ($($missing -join ', ')): got '$($deps -join ', ')'" }
$crtDlls = @($deps | Where-Object { $_ -match '^(VCRUNTIME|MSVCP|api-ms-win-crt-|ucrtbase)' })
if ($Crt -eq 'MT' -and $crtDlls.Count) { throw "a static-CRT build imports the C runtime: $($crtDlls -join ', ')" }
# The linked sources: this folder plus the toolkit's own controls and session headers.
$sourceDirs = $here, (Join-Path $here '..\controls'), (Join-Path $here '..\session')
$dirty = @(foreach ($d in $sourceDirs) { git -C $here status --porcelain -- (Resolve-Path $d).Path })
$receipt = [ordered]@{
    dll = 'dbce_shim.dll'; sha256 = (Get-FileHash -LiteralPath $dll).Hash; crt = $Crt; dependents = @($deps)
    compiler = ((& $clang --version) | Select-Object -First 1); toolkitCommit = (git -C $here rev-parse HEAD)
    sources = 'native/shim, native/controls, native/session'; sourceChanged = [bool]$dirty.Count; changedFiles = $dirty
}
$receipt | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $dir 'build-receipt.json')
"dbce_shim.dll ($Crt) $($receipt.sha256); imports: $($deps -join ', ')"
'PASS native/shim build and checks'
