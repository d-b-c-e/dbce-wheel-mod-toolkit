# Pure parsing/identity rules. No process, device or display actions.
function Read-StageHeartbeat {
    param([string]$Text, [string]$RequestId, [DateTime]$ProcessStartedUtc, [DateTime]$NowUtc)
    $match = [regex]::Match($Text.Trim(), '^(\d+) \d+x\d+ \S+ (\S+) .* request=([a-f0-9]{32}) outputsMuted=(true|false)$')
    if (!$match.Success -or $match.Groups[3].Value -cne $RequestId) { return $null }
    [long]$frame = 0
    $at = [DateTimeOffset]::MinValue
    if (![long]::TryParse($match.Groups[1].Value, [ref]$frame) -or
        ![DateTimeOffset]::TryParseExact($match.Groups[2].Value, 'O', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$at)) { return $null }
    if ($at.UtcDateTime -lt $ProcessStartedUtc -or ($NowUtc - $at.UtcDateTime).TotalSeconds -gt 5 -or
        ($at.UtcDateTime - $NowUtc).TotalSeconds -gt 3) { return $null }
    [pscustomobject]@{ frame=$frame; outputsMuted=($match.Groups[4].Value -ceq 'true'); utc=$at.UtcDateTime }
}

function Test-StageProcessIdentity {
    param([long]$ActualStartTicks, [string]$ActualImage, [long]$ExpectedStartTicks, [string]$ExpectedImage)
    return $ActualStartTicks -eq $ExpectedStartTicks -and
        [IO.Path]::GetFullPath($ActualImage).Equals([IO.Path]::GetFullPath($ExpectedImage), [StringComparison]::OrdinalIgnoreCase)
}
