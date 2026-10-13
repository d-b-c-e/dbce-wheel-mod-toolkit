#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$DllPath,
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$Sha256
)
$ErrorActionPreference = 'Stop'
$libraryPath = Join-Path $PSScriptRoot '..\Dbce.Wheel.Shim\bin\Release\net6.0\Dbce.Wheel.Shim.dll'
if (!(Test-Path -LiteralPath $libraryPath -PathType Leaf)) { throw "Build the Release managed shim first: $libraryPath" }
$DllPath = [IO.Path]::GetFullPath($DllPath)
$bytes = [IO.File]::ReadAllBytes($DllPath)
if ($bytes.Length -lt 0x40) { throw 'Native DLL is too small for a PE header.' }
$pe = [BitConverter]::ToInt32($bytes, 0x3c)
if ($pe -lt 0 -or $pe -gt $bytes.Length - 6 -or [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) {
    throw 'Native DLL is not an x64 PE image.'
}
Add-Type -Path $libraryPath
$loaded = [Dbce.Wheel.Shim.ShimNativeLibrary]::Load($DllPath, $Sha256)
if ($null -eq $loaded) { throw 'Managed shim loader returned no library.' }
"PASS loader-only ABI 2 and 31 action exports; sha256=$Sha256; dll=$DllPath"
# Deliberately no Open, Tick, Record, Stop, or device access.
