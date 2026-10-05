$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'tests/NativeBinding/NativeBinding.csproj'
foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'tests/NativeBinding/build_fixture.bat') $arch | Out-Host
 if ($LASTEXITCODE -ne 0) { throw "Fixture $arch build failed" }
}
foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'tests/NativeBinding/build_fixture.bat') $arch legacy | Out-Host
 if ($LASTEXITCODE -ne 0) { throw "Legacy fixture $arch build failed" }
}
& dotnet build $project -c Release --nologo | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Binding harness build failed' }
& dotnet run --no-build --project $project -c Release -f net8.0 -- (Join-Path $root 'tests/NativeBinding/build/UnityForceFeedback.dll')
if ($LASTEXITCODE -ne 0) { throw 'CoreCLR binding fixture failed' }
& (Join-Path $root 'tests/NativeBinding/bin/Release/net48/NativeBinding.exe') (Join-Path $root 'tests/NativeBinding/build/x86/UnityForceFeedback.dll')
if ($LASTEXITCODE -ne 0) { throw 'x86 .NET Framework binding fixture failed' }

& dotnet run --no-build --project $project -c Release -f net8.0 -- (Join-Path $root 'tests/NativeBinding/build/legacy/UnityForceFeedback.dll') legacy
if ($LASTEXITCODE -ne 0) { throw 'CoreCLR legacy optional binding failed' }
& (Join-Path $root 'tests/NativeBinding/bin/Release/net48/NativeBinding.exe') (Join-Path $root 'tests/NativeBinding/build/x86/legacy/UnityForceFeedback.dll') legacy
if ($LASTEXITCODE -ne 0) { throw 'Framework legacy optional binding failed' }

foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'tests/NativeBinding/build_fixture.bat') $arch burstonly | Out-Host
 if ($LASTEXITCODE -ne 0) { throw "Burst-only fixture build failed" }
}
& dotnet run --no-build --project $project -c Release -f net8.0 -- (Join-Path $root 'tests/NativeBinding/build/burstonly/UnityForceFeedback.dll') burstonly
if ($LASTEXITCODE -ne 0) { throw 'CoreCLR burst-only binding failed' }
& (Join-Path $root 'tests/NativeBinding/bin/Release/net48/NativeBinding.exe') (Join-Path $root 'tests/NativeBinding/build/x86/burstonly/UnityForceFeedback.dll') burstonly
if ($LASTEXITCODE -ne 0) { throw 'Framework burst-only binding failed' }

foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'tests/NativeBinding/build_fixture.bat') $arch shapeonly | Out-Host
 if ($LASTEXITCODE -ne 0) { throw 'Shape-only fixture build failed' }
}
& dotnet run --no-build --project $project -c Release -f net8.0 -- (Join-Path $root 'tests/NativeBinding/build/shapeonly/UnityForceFeedback.dll') shapeonly
if ($LASTEXITCODE -ne 0) { throw 'CoreCLR shape-only binding failed' }
& (Join-Path $root 'tests/NativeBinding/bin/Release/net48/NativeBinding.exe') (Join-Path $root 'tests/NativeBinding/build/x86/shapeonly/UnityForceFeedback.dll') shapeonly
if ($LASTEXITCODE -ne 0) { throw 'Framework shape-only binding failed' }

foreach ($arch in @('x64','x86')) {
 & cmd.exe /c (Join-Path $root 'tests/NativeBinding/build_fixture.bat') $arch nopov | Out-Host
 if ($LASTEXITCODE -ne 0) { throw 'Native0.8.0 fixture build failed' }
}
& dotnet run --no-build --project $project -c Release -f net8.0 -- (Join-Path $root 'tests/NativeBinding/build/nopov/UnityForceFeedback.dll') nopov
if ($LASTEXITCODE -ne 0) { throw 'CoreCLR optional POV binding failed' }
& (Join-Path $root 'tests/NativeBinding/bin/Release/net48/NativeBinding.exe') (Join-Path $root 'tests/NativeBinding/build/x86/nopov/UnityForceFeedback.dll') nopov
if ($LASTEXITCODE -ne 0) { throw 'Framework optional POV binding failed' }
