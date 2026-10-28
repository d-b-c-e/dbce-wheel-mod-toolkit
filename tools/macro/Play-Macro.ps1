#Requires -Version 7.0
# Plays a macro (docs/MACRO.md) into a fenced game through %LOCALAPPDATA%\dbce\<Game>\inject.txt, with its timing:
# no screen reads between steps. Optionally launches the game first and asks for its foreground (Stage-GameForeground,
# which needs the rig lease); fenced controllers are read in the background, so focus is not required for injection.
# -DryRun prints "<seconds> <inject line>" for each step and writes nothing. A malformed macro throws (exit 1).
param(
    [Parameter(Mandatory)][string]$Macro,
    [Parameter(Mandatory)][string]$Game,          # the %LOCALAPPDATA%\dbce folder: dirt3, grid2019, ...
    [string[]]$Device = @(),                      # name={guid}: overrides or adds the macro's "dev" names
    [string]$Launch, [string[]]$LaunchArgs = @(), [ValidateRange(0, 600)][int]$LaunchWaitSeconds = 0,
    [int]$ProcessId,                              # stop if this process exits (set by -Launch)
    [string]$LeasePath, [string]$LeaseToken,      # with both: ask for the game's foreground before playing
    [switch]$DryRun)
$ErrorActionPreference = 'Stop'
$inv = [Globalization.CultureInfo]::InvariantCulture

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
    if (![double]::TryParse($t[0].TrimStart('+'), [Globalization.NumberStyles]::Float, $inv, [ref]$s) -or $s -lt 0) { & $bad 'bad time' }
    $at = if ($rel) { $last + $s } else { $s }
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
# up or a centred hat is the device's own rest, so it sends nothing.
# shortcut: a hold is cut at 15 s (the injection limit); re-send long holds if a game ever needs them.
$steps = for ($i = 0; $i -lt $events.Count; $i++) {
    $e = $events[$i]
    $g = if ($devs.ContainsKey($e.Dev)) { $devs[$e.Dev] } else { Get-Guid $e.Dev }
    if (!$g) { throw "${Macro} line $($e.Line): unknown device $($e.Dev) (add a dev line or -Device $($e.Dev)={guid})" }
    if (($e.Kind -eq 'button' -and $e.Value -eq 0) -or ($e.Kind -eq 'hat' -and $e.Value -eq -1)) { continue }
    $ms = $e.Ms
    if (!$ms) {
        $next = $events | Select-Object -Skip ($i + 1) | Where-Object { $_.Dev -eq $e.Dev -and $_.Kind -eq $e.Kind -and $_.Index -eq $e.Index } | Select-Object -First 1
        $ms = if ($next) { [Math]::Clamp([int][Math]::Round(($next.At - $e.At) * 1000), 50, 15000) } else { 300 }
    }
    $line = switch ($e.Kind) {
        'button' { "inject raw button $($e.Index) dev=$g value=1 ms=$ms" }
        'hat' { "inject raw hat $($e.Index) $($e.Value) dev=$g value=$($e.Value) ms=$ms" }
        'axis' { "inject raw axis $($e.Index) dev=$g value=$($e.Value) ms=$ms" + $(if ($e.Ranged) { " range=$($e.Lo)..$($e.Hi)" }) }
    }
    [pscustomobject]@{ At = $e.At; Line = $line }
}
if ($DryRun) { foreach ($s in $steps) { '{0} {1}' -f $s.At.ToString('0.000', $inv), $s.Line }; return }

$dir = Join-Path $env:LOCALAPPDATA "dbce\$Game"
foreach ($flag in 'ffb-fence.on', 'inject.on') {
    if (!(Test-Path -LiteralPath (Join-Path $dir $flag))) { throw "Missing $dir\$flag (needed when the game starts); nothing sent." }
}
if ($Launch) {
    $ProcessId = (Start-Process -FilePath $Launch -ArgumentList $LaunchArgs -WorkingDirectory (Split-Path -Parent $Launch) -PassThru).Id
    "launched $Launch as $ProcessId; waiting $LaunchWaitSeconds s"
    Start-Sleep -Seconds $LaunchWaitSeconds
}
if ($LeasePath -and $LeaseToken) {
    if (!$ProcessId) { throw '-LeasePath needs -ProcessId or -Launch' }
    . (Join-Path $PSScriptRoot '..\playback\Stage-GameForeground.ps1')
    $p = Get-Process -Id $ProcessId
    Request-StageGameForeground -ProcessId $ProcessId -StartedUtcTicks $p.StartTime.ToUniversalTime().Ticks -ExecutablePath $p.Path -LeasePath $LeasePath -LeaseToken $LeaseToken | Out-Null
}

# The game's reader polls inject.txt every 40 ms and keeps only the newest write, so steps within 60 ms go in one write.
$path = Join-Path $dir 'inject.txt'
$clock = [Diagnostics.Stopwatch]::StartNew()
for ($i = 0; $i -lt $steps.Count) {
    $start = $steps[$i].At
    $batch = @(); while ($i -lt $steps.Count -and $steps[$i].At - $start -lt 0.06) { $batch += $steps[$i].Line; $i++ }
    $wait = [int]($start * 1000 - $clock.ElapsedMilliseconds)
    if ($wait -gt 0) { Start-Sleep -Milliseconds $wait }
    if ($ProcessId -and !(Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)) { throw "process $ProcessId exited; stopped at $start s" }
    $tmp = "$path.$PID.tmp"
    [IO.File]::WriteAllText($tmp, ($batch -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    for ($try = 0; ; $try++) { try { [IO.File]::Move($tmp, $path, $true); break } catch [IO.IOException] { if ($try -ge 24) { throw }; Start-Sleep -Milliseconds 20 } }
    '{0} {1}' -f $start.ToString('0.000', $inv), ($batch -join ' | ')
}
