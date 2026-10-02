#requires -Version 7.0
param([Parameter(Mandatory)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not [IO.Path]::IsPathFullyQualified($OutputRoot)) { throw 'OutputRoot must be absolute.' }
$output = [IO.Path]::GetFullPath($OutputRoot)
if ($output.StartsWith($repo + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or $output -eq $repo) { throw 'Use an output directory outside the source checkout.' }
function Assert-Blob([string]$Path,[string]$Expected) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $prefix = [Text.Encoding]::ASCII.GetBytes("blob $($bytes.Length)`0")
    $all = [byte[]]::new($prefix.Length + $bytes.Length)
    [Array]::Copy($prefix,0,$all,0,$prefix.Length)
    [Array]::Copy($bytes,0,$all,$prefix.Length,$bytes.Length)
    $actual = [Convert]::ToHexString([Security.Cryptography.SHA1]::HashData($all)).ToLowerInvariant()
    if ($actual -ne $Expected) { throw "Source identity mismatch: $Path" }
}
Assert-Blob (Join-Path $PSScriptRoot 'force_model.h') '67860969fe78d01abfcd046451a309859c67bebe'
Assert-Blob (Join-Path $PSScriptRoot 'force_profile.h') '207b229f633ccc6e3acca01953659201d9be1784'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'MSVC installation unavailable.' }
New-Item -ItemType Directory -Force $output | Out-Null
foreach ($arch in 'x86','x64') {
    $vcvars = if ($arch -eq 'x86') { 'vcvars32.bat' } else { 'vcvars64.bat' }
    $batch = @"
@echo off
call "$vs\VC\Auxiliary\Build\$vcvars" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++14 /O2 /W4 /EHsc /Fo"$output\checkpoint-$arch.obj" /Fe"$output\checkpoint-$arch.exe" "$PSScriptRoot\test_checkpoint_copy.cpp"
if errorlevel 1 exit /b 1
"$output\checkpoint-$arch.exe" "$repo\profiles"
"@
    $command = Join-Path $output "check-$arch.cmd"
    Set-Content -LiteralPath $command -Value $batch -Encoding ascii
    & cmd.exe /c $command | Tee-Object -FilePath (Join-Path $output "check-$arch.log")
    if ($LASTEXITCODE -ne 0) { throw "$arch checkpoint fixture failed." }
}
