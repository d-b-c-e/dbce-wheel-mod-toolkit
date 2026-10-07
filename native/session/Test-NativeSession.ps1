param([string]$Compiler = 'E:\msys64\mingw64\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$result = Join-Path ([IO.Path]::GetTempPath()) ('dbce-native-session-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($result) | Out-Null
& $Compiler -std=c++17 -Wall -Wextra -static -o (Join-Path $result 'test.exe') (Join-Path $PSScriptRoot 'test/session_test.cpp')
if ($LASTEXITCODE) { throw 'Native fixture build failed.' }
& (Join-Path $result 'test.exe') $result
if ($LASTEXITCODE) { throw 'Native fixture failed.' }
& dotnet run --project (Join-Path $PSScriptRoot 'test/managed/NativeConformance.csproj') -- $result
if ($LASTEXITCODE) { throw 'Managed/native conformance failed.' }
Write-Host "Evidence: $result"
