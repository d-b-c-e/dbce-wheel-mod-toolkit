#Requires -Version 7.0
# File-integrity checks only. These tiny files are not playable game recordings.
[CmdletBinding()]
param([string]$Results = (Join-Path ([IO.Path]::GetTempPath()) ('dbce-reference-test-' + [guid]::NewGuid().ToString('N'))))
$ErrorActionPreference = 'Stop'
$Results = [IO.Path]::GetFullPath($Results)
if (Test-Path -LiteralPath $Results) { throw 'Choose a new test result directory' }
[IO.Directory]::CreateDirectory($Results) | Out-Null
$copy = Join-Path $PSScriptRoot 'Copy-StageReference.ps1'
function Fixture([string]$Name) {
    $root = Join-Path $Results $Name
    [IO.Directory]::CreateDirectory((Join-Path $root 'signals')) | Out-Null
    foreach ($file in @('trajectory.meta','trajectory.tsv','signals/source.jsonl')) {
        [IO.File]::WriteAllText((Join-Path $root $file), "integrity test fixture: $file")
    }
    Seal $root
    return $root
}
function Seal([string]$Root) {
    $lines = @('dbce-playback-seal-v1') + @(foreach ($file in @('trajectory.meta','trajectory.tsv','signals/source.jsonl')) {
        $path = Join-Path $Root $file
        "$file`t$((Get-Item -LiteralPath $path).Length)`t$((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant())"
    })
    [IO.File]::WriteAllLines((Join-Path $Root 'complete.tsv'), $lines)
}
function Refuses([scriptblock]$Action, [string]$Expected) {
    $errorText = $null
    try { & $Action | Out-Null } catch { $errorText = $_.Exception.Message }
    if (!$errorText -or !$errorText.Contains($Expected)) { throw "Expected refusal '$Expected', got '$errorText'" }
    Write-Host "PASS $Expected"
}
$valid = Fixture 'valid'
$destination = Join-Path $Results 'preserved'
$receipt = & $copy -Source $valid -Destination $destination | ConvertFrom-Json
if ($receipt.sealedFiles -ne 3 -or $receipt.sourceChanged -or
    (Get-FileHash (Join-Path $valid 'complete.tsv')).Hash -ne (Get-FileHash (Join-Path $destination 'complete.tsv')).Hash) { throw 'Valid reference copy failed' }
Write-Host 'PASS complete reference and nested original stream preserved'
Refuses { & $copy -Source $valid -Destination $destination } 'destination already exists'
Refuses { & $copy -Source $valid -Destination (Join-Path $valid 'nested') } 'outside its source'
$tampered = Fixture 'tampered'
[IO.File]::AppendAllText((Join-Path $tampered 'signals/source.jsonl'), 'changed')
Refuses { & $copy -Source $tampered -Destination (Join-Path $Results 'tampered-copy') } 'changed or incomplete'
if (Test-Path (Join-Path $Results 'tampered-copy')) { throw 'Invalid reference was published' }
$escaped = Fixture 'escaped'
[IO.File]::AppendAllText((Join-Path $escaped 'complete.tsv'), "../outside`t1`t$('0' * 64)`n")
Refuses { & $copy -Source $escaped -Destination (Join-Path $Results 'escaped-copy') } 'Invalid or duplicate'
$duplicate = Fixture 'duplicate'
[IO.File]::AppendAllText((Join-Path $duplicate 'complete.tsv'), "TRAJECTORY.tsv`t1`t$('0' * 64)`n")
Refuses { & $copy -Source $duplicate -Destination (Join-Path $Results 'duplicate-copy') } 'Invalid or duplicate'
Write-Host "6 reference-preservation cases passed. Evidence retained: $Results"
