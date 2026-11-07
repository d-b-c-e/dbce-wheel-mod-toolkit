#Requires -Version 7.0
# Dummy consumer for the shared Unity layer. It syncs this checkout's shared, unity, installer and dotnet parts into a
# scratch game folder with Sync-Toolkit -LocalRepo, and checks that the manifest covers them. Then it compiles a dummy
# plugin (tests/UnityConsumer) through lib/toolkit/unity/Dbce.Unity.props, as a game does, for Mono, Mono20 and Il2Cpp
# (and Il2Cpp with Rect and Urp). The Mono20 net472 build must also pass tools/Check-Net35Api.ps1 against the .NET 3.5
# reference assemblies. No game, device or display is touched. -SkipBuild reuses the existing Release build.
param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$case = Join-Path ([IO.Path]::GetTempPath()) ('unity-consumer-' + [guid]::NewGuid().ToString('N'))
$game = Join-Path $case 'game'
$script:checks = 0
function Assert($ok, $message) { $script:checks++; if (-not $ok) { throw $message } }
try {
    if (-not $SkipBuild) {
        dotnet build (Join-Path $root 'Dbce.Wheel.sln') -c Release -nologo -v q | Out-Host
        if ($LASTEXITCODE) { throw 'Toolkit build failed' }
    }
    New-Item -ItemType Directory -Path $case | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'tests/UnityConsumer') -Destination $game -Recurse
    & (Join-Path $root 'tools/Sync-Toolkit.ps1') -LocalRepo $root -Destination (Join-Path $game 'lib/toolkit') -Parts shared,unity,installer,dotnet
    $manifest = @(Get-Content -LiteralPath (Join-Path $game 'lib/toolkit/MANIFEST.txt'))
    foreach ($want in 'shared/ui/F6Style.cs', 'shared/input/ControlBinding.cs', 'shared/display/RacingRow.cs',
                      'shared/support/SupportReport.cs', 'shared/mod/DbceModFile.cs', 'unity/Dbce.Unity.props', 'unity/Runtime/ModDescriptor.cs',
                      'installer/README.md', 'dotnet/Dbce.Wheel.Input.dll', 'dotnet/Dbce.Wheel.Display.dll', 'dotnet/Dbce.Wheel.Ui.dll') {
        Assert (@($manifest | Where-Object { $_ -match ('^[A-F0-9]{64}  ' + [regex]::Escape($want) + '$') }).Count -eq 1) "Manifest lacks $want"
    }
    Assert (@($manifest | Where-Object { $_ -match '\.csproj$|/(bin|obj)/' }).Count -eq 0) 'Manifest lists a project file or build output'
    Set-Content -LiteralPath (Join-Path $game 'Directory.Build.props') '<Project><Import Project="$(MSBuildThisFileDirectory)lib\toolkit\unity\Dbce.Unity.props" /></Project>'
    $variants = [ordered]@{
        Mono = @('-p:DbceRuntime=Mono')
        # Mono20 also opts in to the dbce-mod.json reader, so the .NET 3.5 API check below covers it.
        Mono20 = @('-p:DbceRuntime=Mono20', '-p:DbceSharedSources=input%3Bdisplay%3Bui%3Bsupport%3Bmod')
        Il2Cpp = @('-p:DbceRuntime=Il2Cpp')
        Il2CppRectUrp = @('-p:DbceRuntime=Il2Cpp', '-p:DbceImgui=Rect', '-p:DbcePipeline=Urp')
    }
    foreach ($name in $variants.Keys) {
        # One copy per variant, so no build reuses another's restore or output.
        $dir = Join-Path $game "src/$name"
        Copy-Item -LiteralPath (Join-Path $game 'src/Plugin') -Destination $dir -Recurse
        dotnet build (Join-Path $dir 'Plugin.csproj') -c Release -nologo -v q @($variants[$name]) | Out-Host
        Assert ($LASTEXITCODE -eq 0) "Dummy plugin did not compile for $name"
    }
    $packages = ((dotnet nuget locals global-packages -l) -replace '^global-packages:\s*', '').Trim()
    $ref35 = Get-ChildItem (Join-Path $packages 'microsoft.netframework.referenceassemblies.net35') -Directory | Sort-Object Name |
        Select-Object -Last 1 | ForEach-Object { Join-Path $_.FullName 'build/.NETFramework/v3.5' }
    Assert ($ref35 -and (Test-Path (Join-Path $ref35 'mscorlib.dll'))) 'The .NET 3.5 reference assemblies were not restored'
    $mono20Dll = Join-Path $game 'src/Mono20/bin/Release/net472/DummyPlugin.dll'
    Assert ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($mono20Dll)).Contains('DbceModFile')) 'The Mono20 build lacks the opted-in dbce-mod.json reader'
    & (Join-Path $root 'tools/Check-Net35Api.ps1') -Dll $mono20Dll -GameManaged $ref35 | Out-Host
    Assert ($LASTEXITCODE -eq 0) 'The Mono20 build references APIs that .NET 3.5 lacks'
    [ordered]@{ status = 'passed'; assertions = $script:checks; variants = @($variants.Keys) } | ConvertTo-Json -Compress
} finally {
    $absolute = [IO.Path]::GetFullPath($case)
    $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
    if ((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'unity-consumer-*') { throw 'Unsafe test cleanup path' }
    if (Test-Path -LiteralPath $absolute) { Remove-Item -LiteralPath $absolute -Recurse -Force }
}
