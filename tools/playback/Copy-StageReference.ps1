#Requires -Version 7.0
<# Copies a complete private trajectory recording to a NEW stable directory.
   No game, input, device or network output. Source files are never changed.
   Failed staging directories are retained for diagnosis, not silently deleted. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Source,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference = 'Stop'
$Source = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($Source))
$Destination = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($Destination))
function Assert-Unlinked([string]$Path) {
    $cursor = $Path
    while ($cursor) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked reference path refused: $cursor"
        }
        $cursor = Split-Path -Parent $cursor
    }
}
function Read-Seal([string]$Root) {
    Assert-Unlinked $Root
    $sealPath = Join-Path $Root 'complete.tsv'
    Assert-Unlinked $sealPath
    if ((Get-Item -LiteralPath $sealPath).Length -gt 65536) { throw 'Oversized completion seal' }
    $lines = [IO.File]::ReadAllLines($sealPath)
    if ($lines.Count -lt 3 -or $lines.Count -gt 257 -or $lines[0] -cne 'dbce-playback-seal-v1') { throw 'Not a supported sealed recording' }
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $entries = @(foreach ($line in $lines | Select-Object -Skip 1) {
        $parts = $line.Split("`t")
        if ($parts.Count -ne 3) { throw 'Malformed completion seal' }
        $name = $parts[0]
        if ([IO.Path]::IsPathRooted($name) -or $name -match '[\\:\t\r\n]' -or
            @($name.Split('/') | Where-Object { !$_ -or $_ -in @('.','..') -or $_.EndsWith('.') -or $_.EndsWith(' ') }).Count -or
            !$names.Add($name) -or $name -ieq 'complete.tsv') { throw 'Invalid or duplicate sealed file name' }
        $path = [IO.Path]::GetFullPath((Join-Path $Root $name))
        if (!$path.StartsWith($Root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Sealed path escaped recording' }
        Assert-Unlinked $path
        [long]$size = 0
        if (![long]::TryParse($parts[1], [ref]$size) -or $size -lt 0 -or $parts[2] -cnotmatch '^[a-f0-9]{64}$' -or
            (Get-Item -LiteralPath $path).Length -ne $size -or (Get-FileHash -LiteralPath $path).Hash -ine $parts[2]) { throw "Capture changed or incomplete: $name" }
        [pscustomobject]@{ name=$name; path=$path; size=$size; sha256=$parts[2] }
    })
    foreach ($required in @('trajectory.meta','trajectory.tsv')) { if (!$names.Contains($required)) { throw "Required stream missing: $required" } }
    return [pscustomobject]@{ entries=$entries; sealHash=(Get-FileHash -LiteralPath $sealPath).Hash }
}
Assert-Unlinked $Destination
if (Test-Path -LiteralPath $Destination) { throw 'Reference destination already exists; never overwrite accepted evidence' }
if ($Destination.StartsWith($Source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Put the stable reference outside its source directory' }
$before = Read-Seal $Source
$stage = $Destination + '.partial-' + [guid]::NewGuid().ToString('N')
[IO.Directory]::CreateDirectory($stage) | Out-Null
foreach ($entry in $before.entries) {
    $target = Join-Path $stage $entry.name
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    [IO.File]::Copy($entry.path, $target, $false)
}
[IO.File]::Copy((Join-Path $Source 'complete.tsv'), (Join-Path $stage 'complete.tsv'), $false)
$copied = Read-Seal $stage
$after = Read-Seal $Source
if ($before.sealHash -cne $after.sealHash -or $copied.sealHash -cne $before.sealHash) { throw 'Recording changed during preservation; partial copy retained' }
Assert-Unlinked $Destination
[IO.Directory]::Move($stage, $Destination)
[ordered]@{ schema='dbce.stage-reference-copy@1'; source=$Source; destination=$Destination;
    sealedFiles=$before.entries.Count; bytes=($before.entries | Measure-Object size -Sum).Sum;
    completeSha256=$before.sealHash; verifiedUtc=[DateTime]::UtcNow.ToString('O'); sourceChanged=$false } | ConvertTo-Json
