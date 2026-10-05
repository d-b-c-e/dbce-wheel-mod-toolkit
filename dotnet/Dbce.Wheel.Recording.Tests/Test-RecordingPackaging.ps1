# Exercises only managed artifact copying and transactional sync; no native/device calls.
[CmdletBinding()]
param([string]$ReleaseArchive)
$ErrorActionPreference = 'Stop'
$toolkitRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$caseRoot = Join-Path ([IO.Path]::GetTempPath()) ('recording-package-test-' + [guid]::NewGuid().ToString('N'))
$script:checks = 0
function Assert($condition, [string]$message) {
    $script:checks++
    if (-not $condition) { throw $message }
}
try {
    $source = Join-Path $toolkitRoot 'dotnet/Dbce.Wheel.Recording/bin/Release/netstandard2.0'
    Assert (Test-Path -LiteralPath (Join-Path $source 'Dbce.Wheel.Recording.dll')) 'Build the Release solution first.'
    $destination = Join-Path $caseRoot 'consumer/toolkit'
    & (Join-Path $toolkitRoot 'tools/Sync-Toolkit.ps1') -LocalRepo $toolkitRoot -Destination $destination -Parts dotnet
    foreach ($name in 'Dbce.Wheel.Recording.dll', 'Dbce.Wheel.Recording.xml') {
        $vendored = Join-Path $destination "dotnet/$name"
        Assert (Test-Path -LiteralPath $vendored) "Sync omitted $name"
        Assert ((Get-FileHash -LiteralPath $vendored).Hash -eq (Get-FileHash -LiteralPath (Join-Path $source $name)).Hash) "Sync changed $name"
        $manifest = Get-Content -LiteralPath (Join-Path $destination 'MANIFEST.txt')
        Assert (@($manifest | Where-Object { $_ -match ('  dotnet/' + [regex]::Escape($name) + '$') }).Count -eq 1) "Sync manifest omitted $name"
    }
    Assert ((Get-Content -LiteralPath (Join-Path $destination 'VERSION') -First 1) -match '^local\+[a-f0-9]{40}\.(dirty|clean)$') 'Local extension was labeled as a release.'
    # Execute the actual release script's Recording copy statements in an isolated stage.
    # This verifies source paths and XML inclusion without running its unrelated native build/probes.
    $tokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $toolkitRoot 'tools/package/package.ps1'), [ref]$tokens, [ref]$parseErrors)
    Assert ($parseErrors.Count -eq 0) 'Package script has syntax errors.'
    $statements = @($ast.EndBlock.Statements | Where-Object {
        $_.Extent.Text -match '^\$recordingBin\s*=' -or $_.Extent.Text -match '^Copy-Item\s+\(Join-Path\s+\$recordingBin\b'
    })
    Assert ($statements.Count -eq 3) 'Expected Recording output directory plus DLL/XML copy statements.'
    $root = $toolkitRoot
    $stage = Join-Path $caseRoot 'release'
    New-Item -ItemType Directory -Path (Join-Path $stage 'dotnet') -Force | Out-Null
    foreach ($statement in $statements) { . ([scriptblock]::Create($statement.Extent.Text)) }
    foreach ($name in 'Dbce.Wheel.Recording.dll', 'Dbce.Wheel.Recording.xml') {
        $packaged = Join-Path $stage "dotnet/$name"
        Assert (Test-Path -LiteralPath $packaged) "Package omitted $name"
        Assert ((Get-FileHash -LiteralPath $packaged).Hash -eq (Get-FileHash -LiteralPath (Join-Path $source $name)).Hash) "Package changed $name"
    }
    # Model the pre-Recording release layout offline. Only the download is stubbed;
    # real zip expansion, part selection, staging, hashes and manifest writes run.
    $legacyZip = Join-Path $caseRoot 'dbce-wheel-mod-toolkit-0.12.0.zip'
    if ($ReleaseArchive) {
        Copy-Item -LiteralPath $ReleaseArchive -Destination $legacyZip
    } else {
        $legacy = Join-Path $caseRoot 'dbce-wheel-mod-toolkit-0.12.0'
        New-Item -ItemType Directory -Path (Join-Path $legacy 'native'), (Join-Path $legacy 'dotnet') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $legacy 'native/WheelFfb.dll') 'offline fixture: never loaded'
        foreach ($name in 'Dbce.Wheel.Ffb.dll', 'Dbce.Wheel.Telemetry.dll') {
            Copy-Item -LiteralPath (Join-Path $destination "dotnet/$name") -Destination (Join-Path $legacy "dotnet/$name")
        }
        Compress-Archive -LiteralPath $legacy -DestinationPath $legacyZip
    }
    function gh {
        param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Arguments)
        Assert (($Arguments[0..2] -join ' ') -eq 'release download v0.12.0') 'Unexpected release download arguments.'
        $directoryIndex = [Array]::IndexOf($Arguments, '--dir')
        Assert ($directoryIndex -ge 0) 'Missing release download directory.'
        Copy-Item -LiteralPath $legacyZip -Destination $Arguments[$directoryIndex + 1]
        $global:LASTEXITCODE = 0
    }
    $releaseDestination = Join-Path $caseRoot 'released-consumer/toolkit'
    & (Join-Path $toolkitRoot 'tools/Sync-Toolkit.ps1') -Version v0.12.0 -Destination $releaseDestination -Parts native,dotnet
    Assert (Test-Path -LiteralPath (Join-Path $releaseDestination 'native/WheelFfb.dll')) 'Legacy release lost native DLL.'
    Assert (Test-Path -LiteralPath (Join-Path $releaseDestination 'dotnet/Dbce.Wheel.Ffb.dll')) 'Legacy release lost FFB DLL.'
    Assert (Test-Path -LiteralPath (Join-Path $releaseDestination 'dotnet/Dbce.Wheel.Telemetry.dll')) 'Legacy release lost telemetry DLL.'
    Assert (-not (Test-Path -LiteralPath (Join-Path $releaseDestination 'dotnet/Dbce.Wheel.Recording.dll'))) 'Legacy release invented a Recording DLL.'
    Assert ((Get-Content -LiteralPath (Join-Path $releaseDestination 'VERSION') -First 1) -eq 'v0.12.0') 'Legacy release pin changed.'
    [ordered]@{ status = 'passed'; assertions = $script:checks; scope = 'recording package/local sync and pre-Recording release sync; no devices' } | ConvertTo-Json -Compress
}
finally {
    $absolute = [IO.Path]::GetFullPath($caseRoot)
    $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
    if ((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'recording-package-test-*') { throw 'Unsafe test cleanup path.' }
    if (Test-Path -LiteralPath $absolute) { Remove-Item -LiteralPath $absolute -Recurse -Force }
}
