#Requires -Version 7.0
# Plays a macro (docs/MACRO.md) into a fenced game through %LOCALAPPDATA%\dbce\<Game>\inject.txt, with its timing:
# no screen reads between steps. The game is -ProcessId or started by -Launch (an exe, or a Steam appid/URL with
# -GameExe); its identity (PID + creation time, Stage-ProcessIdentity) and the rig lease (-LeasePath) are rechecked
# before every batch and during every wait, and playback stops on loss. Fenced controllers are read in the background,
# so focus is opt-in (-RequireForeground: at startup only, the ordinary Stage-GameForeground request and, if that is
# genuinely unmet, one logged F24 activation-key fallback (MacroForeground.ps1); then only observed).
# "expect <ref.bmp>" lines compare one game frame with a reference and stop on mismatch; -RecordCheckpoint <name>
# saves the current frame as <name>.bmp beside the macro. "key <scan>" steps (SendInput) need -RequireForeground. Holds still running when playback stops get a release
# sample. The last line is a JSON result; exit 0 only when the whole macro was sent and every checkpoint matched.
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
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RecordCheckpoint,   # save the current frame as <name>.bmp and exit
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
    if ($t[0] -in 'expect', 'key') { $t = @("+0") + $t }   # untimed: right after the previous step
    if ($t[1] -notin 'expect', 'key' -and $t.Count -lt 5) { & $bad '<seconds> <dev> <button|hat|axis> <index> <value> [ms=<50-15000>] [range=<min>..<max>]' }
    $rel = $t[0].StartsWith('+')
    $s = 0.0
    if (![double]::TryParse($t[0].TrimStart('+'), [Globalization.NumberStyles]::Float, $inv, [ref]$s) -or ![double]::IsFinite($s) -or $s -lt 0) { & $bad 'bad time' }
    $at = if ($rel) { $last + $s } else { $s }
    if ($at -gt $maxSeconds) { & $bad "time past $maxSeconds s" }
    if ($at -lt $last) { & $bad 'time goes backwards' }
    $last = $at
    if ($t[1] -eq 'expect') {
        $limit = 20.0; $region = $null; $usage = '[<seconds>] expect <reference.bmp> [maxDiff 0-255] [region=x0,y0,x1,y1 in 0-1]'
        if ($t.Count -lt 3 -or $t.Count -gt 5) { & $bad $usage }
        foreach ($opt in $t | Select-Object -Skip 3) {
            if ($opt -like 'region=*' -and !$region) {
                $v = @($opt.Substring(7) -split ',' | ForEach-Object { $x = 0.0; if ([double]::TryParse($_, [Globalization.NumberStyles]::Float, $inv, [ref]$x)) { $x } else { [double]::NaN } })
                if ($v.Count -ne 4 -or ($v | Where-Object { ![double]::IsFinite($_) -or $_ -lt 0 -or $_ -gt 1 }) -or $v[2] -le $v[0] -or $v[3] -le $v[1]) { & $bad $usage }
                $region = [double[]]$v
            } elseif (![double]::TryParse($opt, [Globalization.NumberStyles]::Float, $inv, [ref]$limit) -or ![double]::IsFinite($limit) -or $limit -lt 0 -or $limit -gt 255) { & $bad $usage }
        }
        $ref = Join-Path (Split-Path -Parent (Resolve-Path -LiteralPath $Macro)) $t[2]
        if (!$RecordCheckpoint -and !(Test-Path -LiteralPath $ref -PathType Leaf)) { & $bad "no reference $ref (make it with -RecordCheckpoint)" }
        $events.Add([pscustomobject]@{ At = $at; Dev = ''; Kind = 'expect'; Ref = $ref; MaxDiff = $limit; Region = $region; Line = $n })
        continue
    }
    if ($t[1] -eq 'key') {   # a keyboard key by hardware scan code (SendInput): only for a game in the foreground
        # shortcut: plain scan codes 0x01-0x7F only (no E0-extended keys); extend if a game needs arrows or the like.
        $scan = -1; $kms = 300
        if ($t[2] -match '^0x([0-9A-Fa-f]{1,2})$') { $scan = [Convert]::ToInt32($Matches[1], 16) } elseif ($t[2] -match '^\d{1,3}$') { $scan = [int]$t[2] }
        if ($t.Count -notin 3, 4 -or $scan -lt 1 -or $scan -gt 0x7F -or ($t.Count -eq 4 -and !($t[3] -match '^ms=(\d+)$' -and [int]$Matches[1] -ge 50 -and [int]$Matches[1] -le 15000))) { & $bad '[<seconds>] key <scan 0x01-0x7F> [ms=<50-15000>]' }
        if ($t.Count -eq 4) { $kms = [int]$Matches[1] }
        $events.Add([pscustomobject]@{ At = $at; Dev = ''; Kind = 'key'; Scan = $scan; Ms = $kms; Line = $n })
        continue
    }
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
    if ($e.Kind -eq 'expect') {
        $where = if ($e.Region) { ' region=' + ($e.Region -join ',') }
        [pscustomobject]@{ At = $e.At; Expect = $e.Ref; MaxDiff = $e.MaxDiff; Region = $e.Region; Line = "expect $($e.Ref) $($e.MaxDiff)$where" }; continue
    }
    if ($e.Kind -eq 'key') {   # a key is released by its own step (SendInput keys do not expire like injections)
        $hex = '0x{0:X2}' -f $e.Scan
        [pscustomobject]@{ At = $e.At; Expect = $null; KeyScan = $e.Scan; KeyUp = $false; Ms = $e.Ms; Line = "key $hex down ms=$($e.Ms)" }
        [pscustomobject]@{ At = $e.At + $e.Ms / 1000; Expect = $null; KeyScan = $e.Scan; KeyUp = $true; Ms = 0; Line = "key $hex up" }
        continue
    }
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
    [pscustomobject]@{ At = $e.At; Expect = $null; KeyScan = $null; Line = $line; Ms = $ms; Key = "$g $($e.Kind) $($e.Index)"; Release = $release }
})
$steps = @($steps | Sort-Object At -Stable)   # key-ups land at their time
if (!$RecordCheckpoint -and !$RequireForeground -and ($steps | Where-Object { $null -ne $_.KeyScan })) {
    throw "${Macro}: key steps go to whatever window is in front, so they need -RequireForeground (and the rig lease)"
}
if ($DryRun) { foreach ($s in $steps) { '{0} {1}' -f $s.At.ToString('0.000', $inv), $s.Line }; return }

$dir = Join-Path $env:LOCALAPPDATA "dbce\$Game"

# Checkpoints: one frame from the game (frame-dump.req -> frame-HHMMSS-<tag>.bmp, 24-bit), reduced to a 64x12 grid of
# cell means (4x4 samples each), compared as the mean absolute difference per channel, 0-255.
# With a region (x0,y0,x1,y1 in 0-1 of the frame, top-left origin) only that crop is compared, on a finer 48x24 grid,
# so a menu highlight is not drowned by an animated background.
function Get-BmpGrid([byte[]]$b, [double[]]$region = @(0, 0, 1, 1), [int]$gw = 64, [int]$gh = 12) {
    if ($b.Length -lt 54 -or $b[0] -ne 0x42 -or $b[1] -ne 0x4D -or [BitConverter]::ToUInt16($b, 28) -ne 24 -or [BitConverter]::ToUInt32($b, 30) -ne 0) { throw 'not a 24-bit uncompressed BMP' }
    $off = [BitConverter]::ToUInt32($b, 10); $w = [BitConverter]::ToInt32($b, 18); $h = [BitConverter]::ToInt32($b, 22)
    $topDown = $h -lt 0; $h = [Math]::Abs($h); $row = ($w * 3 + 3) -band -4
    if ($w -le 0 -or $h -eq 0 -or $off + [long]$row * $h -gt $b.Length) { throw 'truncated BMP' }
    $grid = [double[]]::new($gw * $gh * 3)
    $cw = ($region[2] - $region[0]) * $w; $ch = ($region[3] - $region[1]) * $h
    for ($gy = 0; $gy -lt $gh; $gy++) { for ($gx = 0; $gx -lt $gw; $gx++) {
        $sb = 0; $sg = 0; $sr = 0
        for ($sy = 0; $sy -lt 4; $sy++) {
            $y = [Math]::Min($h - 1, [int][Math]::Floor($region[1] * $h + ($gy + ($sy + 0.5) / 4) * $ch / $gh))
            $base = $off + $(if ($topDown) { $y } else { $h - 1 - $y }) * $row
            for ($sx = 0; $sx -lt 4; $sx++) {
                $q = $base + [Math]::Min($w - 1, [int][Math]::Floor($region[0] * $w + ($gx + ($sx + 0.5) / 4) * $cw / $gw)) * 3
                $sb += $b[$q]; $sg += $b[$q + 1]; $sr += $b[$q + 2]
            }
        }
        $k = ($gy * $gw + $gx) * 3
        $grid[$k] = $sb / 16; $grid[$k + 1] = $sg / 16; $grid[$k + 2] = $sr / 16
    } }
    , $grid
}
function Compare-Bmp([byte[]]$a, [byte[]]$b, [double[]]$region) {
    $x, $y = if ($region) { (Get-BmpGrid $a $region 48 24), (Get-BmpGrid $b $region 48 24) } else { (Get-BmpGrid $a), (Get-BmpGrid $b) }
    $sum = 0.0
    for ($k = 0; $k -lt $x.Length; $k++) { $sum += [Math]::Abs($x[$k] - $y[$k]) }
    $sum / $x.Length
}
function Get-GameFrame([scriptblock]$Guard) {
    $since = [DateTime]::UtcNow
    $req = Join-Path $dir 'frame-dump.req'
    [IO.File]::WriteAllText($req, '')
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        while ($timer.ElapsedMilliseconds -lt 5000) {
            & $Guard
            Start-Sleep -Milliseconds 200
            $new = Get-ChildItem -LiteralPath $dir -Filter 'frame-*.bmp' | Where-Object { $_.LastWriteTimeUtc -ge $since } | Sort-Object LastWriteTimeUtc | Select-Object -First 1
            if (!$new) { continue }
            try { $bytes = [IO.File]::ReadAllBytes($new.FullName) } catch { continue }   # still being written
            if ($bytes.Length -ge 54 -and [BitConverter]::ToUInt32($bytes, 2) -eq $bytes.Length) { return , $bytes }
        }
        throw 'no frame within 5 s (does this game answer frame-dump.req?)'
    } finally { Remove-Item -LiteralPath $req -ErrorAction SilentlyContinue }   # never leave a request for later
}

if ($RecordCheckpoint) {
    $out = Join-Path (Split-Path -Parent (Resolve-Path -LiteralPath $Macro)) "$RecordCheckpoint.bmp"
    if (Test-Path -LiteralPath $out) { throw "$out exists; delete it first to replace it" }
    [IO.File]::WriteAllBytes($out, (Get-GameFrame {}))
    [ordered]@{ checkpointSaved = $out } | ConvertTo-Json -Compress
    return
}

# Everything that can be refused is refused before a launch.
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

$sendCount = @($steps | Where-Object { !$_.Expect }).Count
$result = [ordered]@{ macroDispatched = $false; stepsSent = 0; stepsTotal = $sendCount; checkpointsPassed = 0; checkpoints = @(); stoppedReason = $null; gameplayReady = $null }
$holds = @{}
$clock = [Diagnostics.Stopwatch]::StartNew()
$path = Join-Path $dir 'inject.txt'
$script:fgWindow = 0L
function Send([string[]]$lines) {
    $tmp = "$path.$PID.tmp"
    [IO.File]::WriteAllText($tmp, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    for ($try = 0; ; $try++) { try { [IO.File]::Move($tmp, $path, $true); break } catch [IO.IOException] { if ($try -ge 24) { throw }; Start-Sleep -Milliseconds 20 } }
}
# Read-only foreground observation (no activation).
function Get-Front {
    if (!('Dbce.MacroFront' -as [type])) {
        Add-Type -Namespace Dbce -Name MacroFront -MemberDefinition '[DllImport("user32.dll")] public static extern System.IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(System.IntPtr h, out uint p);'
    }
    $front = [Dbce.MacroFront]::GetForegroundWindow()
    [uint32]$owner = 0
    [void][Dbce.MacroFront]::GetWindowThreadProcessId($front, [ref]$owner)
    [pscustomobject]@{ Window = $front.ToInt64(); Owner = $owner }
}
# Lease, game identity and (once foreground was observed) the same game window still in front. Read-only.
function Assert-Game {
    if ($LeasePath) { Assert-StageRigLease -Path $LeasePath -Token $LeaseToken }
    $state = (Get-StageProcessIdentityDisposition -Recorded $script:identity -Snapshot (Get-StageProcessIdentitySnapshot)).state
    if ($state -ne 'OriginalAlive') { throw "game process $($script:identity.processId) is $state" }
    if ($script:fgWindow) { Assert-MacroForeground $script:fgWindow $script:identity.processId { Get-Front } }
}
# A key as a hardware scan code (KEYEVENTF_SCANCODE), the way a real keyboard reaches a DirectInput or raw-input game.
function Send-Key([int]$scan, [bool]$up) {
    if (!('Dbce.MacroKeys' -as [type])) {
        Add-Type -Namespace Dbce -Name MacroKeys -MemberDefinition @'
[StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public System.IntPtr dwExtraInfo; }
[StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public System.IntPtr dwExtraInfo; }
[StructLayout(LayoutKind.Explicit)] public struct UNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
[StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public UNION u; }
[DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint n, INPUT[] inputs, int size);
public static bool Key(ushort scan, bool up) {
    var i = new INPUT { type = 1 };
    i.u.ki.wScan = scan; i.u.ki.dwFlags = 0x0008u | (up ? 0x0002u : 0u);
    return SendInput(1, new[] { i }, Marshal.SizeOf(typeof(INPUT))) == 1;
}
'@
    }
    if (![Dbce.MacroKeys]::Key([uint16]$scan, $up)) { throw "SendInput refused key 0x$('{0:X2}' -f $scan)" }
}
# Every wait is guarded: checked at least every second and immediately after the last sleep.
function Wait-Until([long]$dueMs) {
    while ($true) {
        Assert-Game
        $left = $dueMs - $clock.ElapsedMilliseconds
        if ($left -le 0) { return }
        Start-Sleep -Milliseconds ([Math]::Min($left, $checkEveryMs))
    }
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
            if ($LeasePath) { Assert-StageRigLease -Path $LeasePath -Token $LeaseToken }
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
    if ($Launch) { [Console]::Error.WriteLine("launched $Launch; game process $($identity.processId); waiting $LaunchWaitSeconds s") }
    Wait-Until ($clock.ElapsedMilliseconds + 1000L * $LaunchWaitSeconds)
    if ($RequireForeground) {   # once, before playback (MacroForeground.ps1); afterwards only observed, never requested again
        . (Join-Path $PSScriptRoot 'MacroForeground.ps1')
        . (Join-Path $PSScriptRoot '..\playback\Stage-GameForeground.ps1')
        $p = Get-Process -Id $identity.processId
        $exe = $p.Path; $ticks = $p.StartTime.ToUniversalTime().Ticks
        $fg = Invoke-MacroForeground -ProcessId $identity.processId -Observe { Get-Front } -Log { param($l) [Console]::Error.WriteLine($l) } `
            -Request { Request-StageGameForeground -ProcessId $p.Id -StartedUtcTicks $ticks -ExecutablePath $exe -LeasePath $LeasePath -LeaseToken $LeaseToken } `
            -Validate { param($w)
                Assert-Game   # lease and birth time
                $q = Get-Process -Id $identity.processId
                Assert-StageForegroundIdentity ([pscustomobject]@{ Id = $q.Id; StartedUtcTicks = $q.StartTime.ToUniversalTime().Ticks; ExecutablePath = [string]$q.Path }) $p.Id $ticks $exe
                Initialize-StageForegroundNative
                [uint32]$owner = 0
                [void][Dbce.StageForegroundNative]::GetWindowThreadProcessId([IntPtr]$w, [ref]$owner)
                if ($owner -ne $p.Id -or ![Dbce.StageForegroundNative]::IsWindowVisible([IntPtr]$w) -or [Dbce.StageForegroundNative]::IsIconic([IntPtr]$w)) { throw 'the game window is gone, hidden or minimized' }
            } `
            -KeysHeld { Initialize-MacroActivation; [Dbce.MacroActivation]::AnyHeld() } `
            -SendF24 { Initialize-MacroActivation; [Dbce.MacroActivation]::DownUp() } `
            -Release { Initialize-MacroActivation; [void][Dbce.MacroActivation]::Up() } `
            -Activate { param($w) [Dbce.StageForegroundNative]::SetForegroundWindow([IntPtr]$w) }
        $result.foreground = $fg
        if ($fg.status -ne 'observed') { throw "foreground unmet: $($fg.reason)" }
        $script:fgWindow = $fg.window
        Assert-Game
    }
    $clock.Restart()
    $shift = 0L   # time spent in checkpoints; the steps after one keep their spacing
    # The game's reader polls inject.txt every 40 ms and keeps only the newest write, so steps within 60 ms go in one write.
    for ($i = 0; $i -lt $steps.Count) {
        $start = $steps[$i].At
        Wait-Until ([long]($start * 1000) + $shift)
        if ($steps[$i].Expect) {
            $c = $steps[$i++]; $t0 = $clock.ElapsedMilliseconds
            $name = [IO.Path]::GetFileNameWithoutExtension($c.Expect)
            $frame = try { Get-GameFrame { Assert-Game } } catch { throw "checkpoint $name failed: $($_.Exception.Message)" }
            $diff = Compare-Bmp ([IO.File]::ReadAllBytes($c.Expect)) $frame $c.Region
            $result.checkpoints += [ordered]@{ name = $name; diff = [Math]::Round($diff, 1); maxDiff = $c.MaxDiff; passed = $diff -le $c.MaxDiff }
            if ($diff -gt $c.MaxDiff) { throw ('checkpoint {0} mismatch {1}' -f $name, $diff.ToString('0.0', $inv)) }
            $result.checkpointsPassed++
            $shift += $clock.ElapsedMilliseconds - $t0
            [Console]::Error.WriteLine(('{0} checkpoint {1} matched ({2})' -f $start.ToString('0.000', $inv), $name, $diff.ToString('0.0', $inv)))
            continue
        }
        if ($null -ne $steps[$i].KeyScan) {   # Wait-Until has just checked the lease, the game and its foreground window
            $k = $steps[$i++]
            Send-Key $k.KeyScan $k.KeyUp
            if ($k.KeyUp) { $holds.Remove("key $($k.KeyScan)") }
            else { $holds["key $($k.KeyScan)"] = [pscustomobject]@{ Until = $clock.ElapsedMilliseconds + $k.Ms; Release = $null; KeyScan = $k.KeyScan } }
            $result.stepsSent++
            [Console]::Error.WriteLine(('{0} {1}' -f $start.ToString('0.000', $inv), $k.Line))
            continue
        }
        $batch = @(); while ($i -lt $steps.Count -and !$steps[$i].Expect -and $null -eq $steps[$i].KeyScan -and $steps[$i].At - $start -lt 0.06) { $batch += $steps[$i]; $i++ }
        Send $batch.Line
        foreach ($s in $batch) { $holds[$s.Key] = [pscustomobject]@{ Until = $clock.ElapsedMilliseconds + $s.Ms; Release = $s.Release } }
        $result.stepsSent += $batch.Count
        [Console]::Error.WriteLine(('{0} {1}' -f $start.ToString('0.000', $inv), ($batch.Line -join ' | ')))
    }
    $end = ($holds.Values | Measure-Object Until -Maximum).Maximum
    if ($end) { Wait-Until ([long]$end) }   # the last hold completes under the same checks
    $result.macroDispatched = $true
} catch {
    $result.stoppedReason = $_.Exception.Message
} finally {
    if (!$result.macroDispatched) { $result.stoppedReason ??= 'cancelled' }
    $active = @($holds.Values | Where-Object { !$_.KeyScan -and $_.Until -gt $clock.ElapsedMilliseconds })
    if ($active) {   # bounded: one release write, then one reader poll
        try { Send $active.Release; Start-Sleep -Milliseconds 100 } catch { $result.stoppedReason += "; release failed: $($_.Exception.Message)" }
    }
    foreach ($k in @($holds.Values | Where-Object { $_.KeyScan })) {   # a key stays down until its up is sent
        try { Send-Key $k.KeyScan $true } catch { $result.stoppedReason += "; key-up failed: $($_.Exception.Message)" }
    }
    $result | ConvertTo-Json -Compress
}
if (!$result.macroDispatched) { exit 1 }
