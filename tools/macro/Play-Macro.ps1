#Requires -Version 7.0
# Plays a macro (docs/MACRO.md) into a fenced game through %LOCALAPPDATA%\dbce\<Game>\inject.txt, with its timing:
# no screen reads between steps. The game is -ProcessId or started by -Launch (an exe, or a Steam appid/URL with
# -GameExe); its identity (PID + creation time, Stage-ProcessIdentity) and the rig lease (-LeasePath) are rechecked
# before every batch and during waits, and playback stops on loss. Fenced controllers are read in the background, so
# focus is opt-in (-RequireForeground, one Stage-GameForeground request under the lease). Holds still running when
# playback stops get a release sample. The last line is a JSON result; exit 0 only when the whole macro was sent.
# -DryRun prints "<seconds> <inject line>" for each step and sends nothing. A malformed macro throws (exit 1).
param(
    [Parameter(Mandatory)][string]$Macro,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Game,   # the %LOCALAPPDATA%\dbce folder: dirt3, ...
    [string[]]$Device = @(),                      # name={guid}: overrides or adds the macro's "dev" names
    [string]$Launch, [string[]]$LaunchArgs = @(), [ValidateRange(0, 600)][int]$LaunchWaitSeconds = 0,
    [string]$GameExe,                             # with a Steam -Launch: the game's exact executable path
    [int]$ProcessId,                              # the running game, when not launched here
    [string]$LeasePath, [string]$LeaseToken,
    [switch]$RequireForeground,
    [switch]$DryRun)
$ErrorActionPreference = 'Stop'
$inv = [Globalization.CultureInfo]::InvariantCulture
$maxSeconds = 3600   # a macro longer than an hour is a mistake, not a route
$checkEveryMs = 1000

function Get-Guid([string]$text) {
    $g = [Guid]::Empty
    if ([Guid]::TryParse($text, [ref]$g) -and $g -ne [Guid]::Empty) { return $g.ToString('B') }
}

# Parse: every line is checked; the first bad one throws with its number.
$devs = @{}
$events = [Collections.Generic.List[object]]::new()
$n = 0; $last = 0.0
foreach ($raw in Get-Content -LiteralPath $Macro) {
    $n++
    $t = @(($raw -replace '#.*$', '').Trim() -split '\s+' | Where-Object { $_ })
    if (!$t) { continue }
    $bad = { param($why) throw "${Macro} line ${n}: $why ($raw)" }
    if ($t[0] -eq 'dev') {
        if ($t.Count -ne 3 -or !($g = Get-Guid $t[2]) -or (Get-Guid $t[1])) { & $bad 'dev <name> <guid>' }
        $devs[$t[1]] = $g; continue
    }
    if ($t.Count -lt 5) { & $bad '<seconds> <dev> <button|hat|axis> <index> <value> [ms=<50-15000>] [range=<min>..<max>]' }
    $rel = $t[0].StartsWith('+')
    $s = 0.0
    if (![double]::TryParse($t[0].TrimStart('+'), [Globalization.NumberStyles]::Float, $inv, [ref]$s) -or ![double]::IsFinite($s) -or $s -lt 0) { & $bad 'bad time' }
    $at = if ($rel) { $last + $s } else { $s }
    if ($at -gt $maxSeconds) { & $bad "time past $maxSeconds s" }
    if ($at -lt $last) { & $bad 'time goes backwards' }
    $last = $at
    $kind = $t[2]
    $max = switch ($kind) { 'button' { 127 } 'hat' { 3 } 'axis' { 7 } default { & $bad "unknown kind $kind" } }
    $index = 0; $value = 0L
    if (![int]::TryParse($t[3], [ref]$index) -or $index -lt 0 -or $index -gt $max) { & $bad 'bad index' }
    if (![long]::TryParse($t[4], [ref]$value)) { & $bad 'bad value' }
    $ms = $null; $lo = 0L; $hi = 65535L; $ranged = $false
    foreach ($opt in $t | Select-Object -Skip 5) {
        if ($opt -match '^ms=(\d+)$' -and [int]$Matches[1] -ge 50 -and [int]$Matches[1] -le 15000) { $ms = [int]$Matches[1] }
        elseif ($kind -eq 'axis' -and $opt -match '^range=(-?\d+)\.\.(-?\d+)$' -and [long]$Matches[2] -gt [long]$Matches[1]) { $lo = [long]$Matches[1]; $hi = [long]$Matches[2]; $ranged = $true }
        else { & $bad "bad option $opt" }
    }
    if (($kind -eq 'button' -and $value -notin 0, 1) -or ($kind -eq 'hat' -and ($value -lt -1 -or $value -gt 35999)) -or
        ($kind -eq 'axis' -and ($value -lt $lo -or $value -gt $hi))) { & $bad 'value out of range' }
    $events.Add([pscustomobject]@{ At = $at; Dev = $t[1]; Kind = $kind; Index = $index; Value = $value; Ms = $ms; Lo = $lo; Hi = $hi; Ranged = $ranged; Line = $n })
}
foreach ($d in $Device) {
    $name, $id = $d -split '=', 2
    if (!($g = Get-Guid $id)) { throw "-Device ${d}: expected name={guid}" }
    $devs[$name] = $g
}

# Steps: a button down, a hat angle or an axis value is held until the same object's next event (or its ms=); a button
# up or a centred hat is the device's own rest, so it sends nothing. Each step carries the 50 ms sample that ends it.
# shortcut: a hold is cut at 15 s (the injection limit); re-send long holds if a game ever needs them.
$steps = @(for ($i = 0; $i -lt $events.Count; $i++) {
    $e = $events[$i]
    $g = if ($devs.ContainsKey($e.Dev)) { $devs[$e.Dev] } else { Get-Guid $e.Dev }
    if (!$g) { throw "${Macro} line $($e.Line): unknown device $($e.Dev) (add a dev line or -Device $($e.Dev)={guid})" }
    if (($e.Kind -eq 'button' -and $e.Value -eq 0) -or ($e.Kind -eq 'hat' -and $e.Value -eq -1)) { continue }
    $ms = $e.Ms
    if (!$ms) {
        $next = $events | Select-Object -Skip ($i + 1) | Where-Object { $_.Dev -eq $e.Dev -and $_.Kind -eq $e.Kind -and $_.Index -eq $e.Index } | Select-Object -First 1
        $ms = if ($next) { [Math]::Clamp([int][Math]::Round(($next.At - $e.At) * 1000), 50, 15000) } else { 300 }
    }
    $range = if ($e.Ranged) { " range=$($e.Lo)..$($e.Hi)" }
    $line, $release = switch ($e.Kind) {
        'button' { "inject raw button $($e.Index) dev=$g value=1 ms=$ms", "inject raw button $($e.Index) dev=$g value=0 ms=50" }
        'hat' { "inject raw hat $($e.Index) $($e.Value) dev=$g value=$($e.Value) ms=$ms", "inject raw hat $($e.Index) $($e.Value) dev=$g value=-1 ms=50" }
        'axis' { "inject raw axis $($e.Index) dev=$g value=$($e.Value) ms=$ms$range", "inject raw axis $($e.Index) dev=$g value=$($e.Value) ms=50$range" }
    }
    [pscustomobject]@{ At = $e.At; Line = $line; Ms = $ms; Key = "$g $($e.Kind) $($e.Index)"; Release = $release }
})
if ($DryRun) { foreach ($s in $steps) { '{0} {1}' -f $s.At.ToString('0.000', $inv), $s.Line }; return }

# Everything that can be refused is refused before a launch.
$dir = Join-Path $env:LOCALAPPDATA "dbce\$Game"
foreach ($flag in 'ffb-fence.on', 'inject.on') {
    if (!(Test-Path -LiteralPath (Join-Path $dir $flag))) { throw "Missing $dir\$flag (needed when the game starts); nothing sent." }
}
if (!$Launch -and !$ProcessId) { throw 'Name the game: -ProcessId or -Launch.' }
if ([bool]$LeasePath -ne [bool]$LeaseToken) { throw '-LeasePath and -LeaseToken go together.' }
if ($RequireForeground -and !$LeasePath) { throw '-RequireForeground needs the rig lease (-LeasePath, -LeaseToken).' }
$steam = $Launch -match '^\d+$' -or $Launch -like 'steam://*'
if ($steam -and !$GameExe) { throw 'A Steam -Launch needs -GameExe (the game executable to follow).' }
. (Join-Path $PSScriptRoot '..\playback\Stage-ProcessIdentity.ps1')
if ($LeasePath) { . (Join-Path $PSScriptRoot '..\playback\Stage-RigLease.ps1'); Assert-StageRigLease -Path $LeasePath -Token $LeaseToken }

$result = [ordered]@{ macroDispatched = $false; stepsSent = 0; stepsTotal = $steps.Count; stoppedReason = $null; gameplayReady = $null }
$holds = @{}
$clock = [Diagnostics.Stopwatch]::StartNew()
$path = Join-Path $dir 'inject.txt'
function Send([string[]]$lines) {
    $tmp = "$path.$PID.tmp"
    [IO.File]::WriteAllText($tmp, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    for ($try = 0; ; $try++) { try { [IO.File]::Move($tmp, $path, $true); break } catch [IO.IOException] { if ($try -ge 24) { throw }; Start-Sleep -Milliseconds 20 } }
}
function Assert-Game {
    if ($LeasePath) { Assert-StageRigLease -Path $LeasePath -Token $LeaseToken }
    $state = (Get-StageProcessIdentityDisposition -Recorded $script:identity -Snapshot (Get-StageProcessIdentitySnapshot)).state
    if ($state -ne 'OriginalAlive') { throw "game process $($script:identity.processId) is $state" }
}
try {
    if ($Launch -and $steam) {
        $url = if ($Launch -match '^\d+$') { "steam://rungameid/$Launch" } else { $Launch }
        $since = [DateTime]::Now
        Start-Process $url
        # The started process is Steam's, not the game's: follow the one new process of -GameExe (CIM identity).
        $want = [IO.Path]::GetFullPath($GameExe)
        for ($k = 0; !$script:identity; $k++) {
            if ($k -ge 120) { throw "no new $want within 120 s of $url" }
            Start-Sleep -Seconds 1
            $found = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object { $_.ExecutablePath -and $_.ExecutablePath -eq $want -and $_.CreationDate -ge $since })
            if ($found.Count -gt 1) { throw "more than one new $want" }
            if ($found) { $script:identity = [pscustomobject]@{ processId = $found[0].ProcessId; createdUtc = $found[0].CreationDate; precision = 'CimMicrosecond' } }
        }
    } else {
        if ($Launch) { $ProcessId = (Start-Process -FilePath $Launch -ArgumentList $LaunchArgs -WorkingDirectory (Split-Path -Parent $Launch) -PassThru).Id }
        $p = Get-Process -Id $ProcessId
        $script:identity = [pscustomobject]@{ processId = $p.Id; createdUtc = $p.StartTime; precision = 'Tick100ns' }
    }
    if ($Launch) { [Console]::Error.WriteLine("launched $Launch; game process $($identity.processId); waiting $LaunchWaitSeconds s"); Start-Sleep -Seconds $LaunchWaitSeconds }
    Assert-Game
    if ($RequireForeground) {
        . (Join-Path $PSScriptRoot '..\playback\Stage-GameForeground.ps1')
        $p = Get-Process -Id $identity.processId
        $fg = Request-StageGameForeground -ProcessId $p.Id -StartedUtcTicks $p.StartTime.ToUniversalTime().Ticks -ExecutablePath $p.Path -LeasePath $LeasePath -LeaseToken $LeaseToken
        if ($fg.status -ne 'observed') { throw "foreground unmet: $($fg.reason)" }
    }
    $clock.Restart()
    # The game's reader polls inject.txt every 40 ms and keeps only the newest write, so steps within 60 ms go in one write.
    for ($i = 0; $i -lt $steps.Count) {
        $start = $steps[$i].At
        $batch = @(); while ($i -lt $steps.Count -and $steps[$i].At - $start -lt 0.06) { $batch += $steps[$i]; $i++ }
        do {   # identity and lease before the batch, and at least every second of a long wait
            Assert-Game
            $wait = [long]($start * 1000) - $clock.ElapsedMilliseconds
            if ($wait -gt 0) { Start-Sleep -Milliseconds ([Math]::Min($wait, $checkEveryMs)) }
        } while ($wait -gt $checkEveryMs)
        Send $batch.Line
        foreach ($s in $batch) { $holds[$s.Key] = [pscustomobject]@{ Until = $clock.ElapsedMilliseconds + $s.Ms; Release = $s.Release } }
        $result.stepsSent += $batch.Count
        [Console]::Error.WriteLine(('{0} {1}' -f $start.ToString('0.000', $inv), ($batch.Line -join ' | ')))
    }
    $end = ($holds.Values | Measure-Object Until -Maximum).Maximum
    if ($end -gt $clock.ElapsedMilliseconds) { Start-Sleep -Milliseconds ([long]$end - $clock.ElapsedMilliseconds) }   # the last hold completes
    $result.macroDispatched = $true
} catch {
    $result.stoppedReason = $_.Exception.Message
} finally {
    if (!$result.macroDispatched) { $result.stoppedReason ??= 'cancelled' }
    $active = @($holds.Values | Where-Object { $_.Until -gt $clock.ElapsedMilliseconds })
    if ($active) {   # bounded: one release write, then one reader poll
        try { Send $active.Release; Start-Sleep -Milliseconds 100 } catch { $result.stoppedReason += "; release failed: $($_.Exception.Message)" }
    }
    $result | ConvertTo-Json -Compress
}
if (!$result.macroDispatched) { exit 1 }
