# Exercises only managed artifact copying and transactional sync; no native/device calls.
[CmdletBinding()]
param()
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
    [ordered]@{ status = 'passed'; assertions = $script:checks; scope = 'recording package/local sync; no devices' } | ConvertTo-Json -Compress
}
finally {
    $absolute = [IO.Path]::GetFullPath($caseRoot)
    $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
    if ((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'recording-package-test-*') { throw 'Unsafe test cleanup path.' }
    if (Test-Path -LiteralPath $absolute) { Remove-Item -LiteralPath $absolute -Recurse -Force }
}
