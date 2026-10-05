#Requires -Version 7.0
# Shared coordination-file ownership. No process, device, input or display actions.
function Enter-StageRigLease {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path,
          [Parameter(Mandatory)][ValidatePattern('^[^\r\n]+$')][string]$Owner,
          [Parameter(Mandatory)][ValidatePattern('^[^\r\n]+$')][string]$Purpose)
    $Path = [IO.Path]::GetFullPath($Path)
    $parent = [IO.Path]::GetDirectoryName($Path)
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    foreach ($item in @($parent, $Path)) {
        if ((Test-Path -LiteralPath $item) -and ((Get-Item -LiteralPath $item).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Rig lease path is a link' }
    }
    $stream = $null; $created = $false
    try {
        try { $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None); $created = $true }
        catch [IO.IOException] {
            if (![IO.File]::Exists($Path)) { throw }
            $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        }
        if ($stream.Length -gt 8192) { throw 'Oversized rig lease; inspect it before continuing' }
        $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true, 1024, $true)
        try { $previous = $reader.ReadToEnd() } finally { $reader.Dispose() }
        if (!$created -and [IO.File]::GetLastWriteTimeUtc($Path) -gt [DateTime]::UtcNow.AddHours(-2)) { throw ('Shared rig test slot is held: ' + $previous.Trim()) }
        $token = $Owner + ' ' + [DateTimeOffset]::Now.ToString('O') + ' ' + $Purpose + ' lease=' + [guid]::NewGuid().ToString('N')
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($token + [Environment]::NewLine)
        $stream.Position = 0; $stream.SetLength(0); $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true)
        return [pscustomobject]@{ path=$Path; token=$token; previous=$previous }
    } finally { if ($stream) { $stream.Dispose() } }
}

function Exit-StageRigLease {
    [CmdletBinding()]
    param([Parameter(Mandatory)]$Lease)
    $path = [IO.Path]::GetFullPath($Lease.path)
    if (![IO.File]::Exists($path)) { return $false }
    if ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Rig lease path changed to a link' }
    # ShareDelete permits deleting this exact file while retaining exclusive
    # read/write ownership. A replacement owner's text is never removed.
    $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Delete)
    try {
        if ($stream.Length -gt 8192) { return $false }
        $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true, 1024, $true)
        try { $current = $reader.ReadToEnd().TrimEnd("`r", "`n") } finally { $reader.Dispose() }
        if ($current -cne $Lease.token) { return $false }
        [IO.File]::Delete($path)
        return $true
    } finally { $stream.Dispose() }
}
