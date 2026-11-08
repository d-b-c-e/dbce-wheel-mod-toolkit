# Reader of the shared store (docs/STORE.md) for the installer and install scripts (PowerShell 5.1 and 7). Dot-source it:
#   . "$PSScriptRoot\DbceStore.ps1"; $store = Read-DbceStore; $choice = Get-DbceStoreChoice $store 'art-of-rally'
# profiles.json (profiles, the shared cockpit, which profile each game uses) is written by Wheelkit only;
# runtime\<game-id>.json (one game's runtime settings) by that game's mod only. Files only gain fields; this reader
# ignores every field it does not know. Format-DbceStore and Format-DbceRuntimeSettings write the canonical dump the
# C# (shared/mod/DbceStore.cs) and C++ (native/panel/dbce_store.h) readers' tests also compare with
# schema/fixtures/store/*.dump.txt.
Set-StrictMode -Version 2

function script:DsProp($o, [string]$name) {
    if (-not (DsIsObject $o)) { return $null }
    # Exact case: the C# and C++ readers match member names exactly.
    foreach ($p in $o.PSObject.Properties) { if ($p.Name -ceq $name) { return $p.Value } }
    return $null
}
function script:DsStr($o, [string]$name) { $v = DsProp $o $name; if ($v -is [string]) { $v } else { '' } }
function script:DsIsNum($v) { $v -is [int] -or $v -is [long] -or $v -is [double] -or $v -is [decimal] }
function script:DsNum($o, [string]$name, [double]$fallback) { $v = DsProp $o $name; if (DsIsNum $v) { [double]$v } else { $fallback } }
function script:DsInt($o, [string]$name, [int]$fallback) { [int][Math]::Truncate((DsNum $o $name $fallback)) }
function script:DsBool($o, [string]$name, [bool]$fallback) { $v = DsProp $o $name; if ($v -is [bool]) { $v } else { $fallback } }
function script:DsIsObject($v) { $v -is [Management.Automation.PSCustomObject] }
function script:DsMembers($o) { if (DsIsObject $o) { @($o.PSObject.Properties) } else { @() } }

function script:DsScreens($s) {
    if (-not (DsIsObject $s)) { return $null }
    [pscustomobject][ordered]@{
        Count = DsInt $s 'Count' 3; WidthPixels = DsInt $s 'WidthPixels' 2560; HeightPixels = DsInt $s 'HeightPixels' 1440
        SeparateMonitors = DsBool $s 'SeparateMonitors' $true
        DiagonalInches = DsNum $s 'DiagonalInches' 32; AspectWidth = DsNum $s 'AspectWidth' 16; AspectHeight = DsNum $s 'AspectHeight' 9
        EyeDistanceMm = DsNum $s 'EyeDistanceMm' 660; SideAngleDegrees = DsNum $s 'SideAngleDegrees' 70
        BezelGapMm = DsNum $s 'BezelGapMm' 8; CurveRadiusMm = DsNum $s 'CurveRadiusMm' 0
        MeasuredWidthMm = $(if (DsIsNum (DsProp $s 'MeasuredWidthMm')) { [double](DsProp $s 'MeasuredWidthMm') } else { $null })
    }
}
function script:DsNullableInt($o, [string]$name) { $v = DsProp $o $name; if (DsIsNum $v) { [int][Math]::Truncate([double]$v) } else { $null } }

# %LOCALAPPDATA%\Wheelkit\store, the store's fixed folder.
function Get-DbceStoreFolder { Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Wheelkit\store' }
function Get-DbceRuntimePath([Parameter(Mandatory)][string]$GameId) { Join-Path (Get-DbceStoreFolder) "runtime\$GameId.json" }

function Read-DbceStore {
    [CmdletBinding()]
    param([string]$Path = (Join-Path (Get-DbceStoreFolder) 'profiles.json'))
    $j = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
    if ((DsNum $j 'Schema' 0) -ne 2) { throw "$Path is not a store profiles.json (Schema 2)." }
    $entries = DsProp $j 'Profiles'
    if ($null -eq $entries -or @($entries).Count -eq 0) { throw "$Path needs at least one profile." }
    $profiles = New-Object System.Collections.ArrayList
    foreach ($e in @($entries)) {
        $c = DsProp $e 'Profile'
        $id = DsStr $e 'Id'
        if (-not $id -or -not (DsIsObject $c)) { throw "$Path`: each profile needs an Id and a Profile." }
        if (@($profiles | Where-Object { $_.Id -ceq $id }).Count) { throw "$Path`: duplicate profile Id '$id'." }
        $bindings = @(foreach ($m in (DsMembers (DsProp $c 'Bindings'))) {
            $b = $m.Value
            if (-not (DsIsObject $b)) { continue }
            [pscustomobject][ordered]@{
                Action = $m.Name; InstanceGuid = DsStr $b 'InstanceGuid'; ProductGuid = DsStr $b 'ProductGuid'; DeviceName = DsStr $b 'DeviceName'
                Kind = DsStr $b 'Kind'; Index = DsInt $b 'Index' 0; Direction = DsInt $b 'Direction' 0; Rest = DsInt $b 'Rest' 0
                Minimum = DsInt $b 'Minimum' 0; Maximum = DsInt $b 'Maximum' 65535
                Calibrated = DsBool $b 'Calibrated' $false; Inverted = DsBool $b 'Inverted' $false
                CalLow = DsNullableInt $b 'CalLow'; CalCentre = DsNullableInt $b 'CalCentre'; CalHigh = DsNullableInt $b 'CalHigh'
            }
        })
        $rig = DsProp $e 'Rig'
        $transmission = DsStr $c 'Transmission'
        [void]$profiles.Add([pscustomobject][ordered]@{
            Id = $id; Name = DsStr $c 'Name'; Transmission = $(if ($transmission) { $transmission } else { 'Automatic' })
            Wheel = DsStr $c 'Wheel'; Shifter = DsStr $c 'Shifter'
            FfbStrength = DsInt $rig 'FfbStrength' 50; FfbOn = DsBool $rig 'FfbOn' $true
            Screens = DsScreens (DsProp $rig 'Screens')
            Bindings = $bindings
        })
    }
    $games = @(foreach ($m in (DsMembers (DsProp $j 'Games'))) {
        if (DsIsObject $m.Value) { [pscustomobject][ordered]@{ Id = $m.Name; Profile = DsStr $m.Value 'Profile' } }
    })
    [pscustomobject][ordered]@{
        Schema = 2; ActiveId = DsStr $j 'ActiveId'; Cockpit = DsScreens (DsProp $j 'Cockpit')
        Profiles = $profiles.ToArray(); Games = $games
    }
}

# The profile a game uses: its assignment when that names a profile, else ActiveId's, else the first; with the profile's
# own screens, else the cockpit, else none (the mod keeps its own screen settings).
function Get-DbceStoreChoice {
    param([Parameter(Mandatory)]$Store, [Parameter(Mandatory)][string]$GameId)
    $find = { param($id) @($Store.Profiles | Where-Object { $_.Id -ceq $id }) | Select-Object -First 1 }
    $p = $null
    foreach ($g in @($Store.Games)) { if ($g.Id -ceq $GameId) { $p = & $find $g.Profile } }
    if ($null -eq $p) { $p = & $find $Store.ActiveId }
    if ($null -eq $p) { $p = @($Store.Profiles)[0] }
    $screens = $p.Screens; $from = 'profile'
    if ($null -eq $screens) { $screens = $Store.Cockpit; $from = $(if ($null -eq $screens) { 'none' } else { 'cockpit' }) }
    [pscustomobject][ordered]@{ Profile = $p; Screens = $screens; ScreensFrom = $from }
}

function Read-DbceRuntimeSettings {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)
    $j = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
    if ((DsNum $j 'Schema' 0) -ne 1) { throw "$Path is not a store runtime file (Schema 1)." }
    $game = DsStr $j 'Game'
    if (-not $game) { throw "$Path needs its Game." }
    $values = @(foreach ($m in (DsMembers (DsProp $j 'Values'))) {
        $v = $m.Value
        if (DsIsNum $v) { [pscustomobject][ordered]@{ Key = $m.Name; Type = 'number'; Value = [double]$v } }
        elseif ($v -is [bool]) { [pscustomobject][ordered]@{ Key = $m.Name; Type = 'bool'; Value = $v } }
        elseif ($v -is [string]) { [pscustomobject][ordered]@{ Key = $m.Name; Type = 'string'; Value = $v } }
    })
    [pscustomobject][ordered]@{ Schema = 1; Game = $game; Profile = DsStr $j 'Profile'; Values = $values }
}

function script:DsDumper {
    $out = New-Object System.Collections.Generic.List[string]
    $w = [pscustomobject]@{ Out = $out }
    $w | Add-Member ScriptMethod S { param($p, $v) $this.Out.Add($p + '=' + ([string]$v).Replace('\', '\\').Replace("`r", '\r').Replace("`n", '\n')) }
    $w | Add-Member ScriptMethod B { param($p, $v) $this.Out.Add($p + '=' + $(if ($v) { 'true' } else { 'false' })) }
    $w | Add-Member ScriptMethod N { param($p, $v) $this.Out.Add($p + '=' + ([double]$v).ToString('R', [Globalization.CultureInfo]::InvariantCulture)) }
    $w | Add-Member ScriptMethod I { param($p, $v) if ($null -eq $v) { $this.Out.Add($p + '=null') } else { $this.N($p, $v) } }
    $w | Add-Member ScriptMethod Screens {
        param($p, $s)
        if ($null -eq $s) { $this.Out.Add($p + '=null'); return }
        $this.N("$p.count", $s.Count); $this.N("$p.widthPixels", $s.WidthPixels); $this.N("$p.heightPixels", $s.HeightPixels); $this.B("$p.separateMonitors", $s.SeparateMonitors)
        $this.N("$p.diagonalInches", $s.DiagonalInches); $this.N("$p.aspectWidth", $s.AspectWidth); $this.N("$p.aspectHeight", $s.AspectHeight)
        $this.N("$p.eyeDistanceMm", $s.EyeDistanceMm); $this.N("$p.sideAngleDegrees", $s.SideAngleDegrees); $this.N("$p.bezelGapMm", $s.BezelGapMm)
        $this.N("$p.curveRadiusMm", $s.CurveRadiusMm); $this.I("$p.measuredWidthMm", $s.MeasuredWidthMm)
    }
    $w
}

function Format-DbceStore {
    param([Parameter(Mandatory)]$Store)
    $w = DsDumper
    $w.N('schema', $Store.Schema); $w.S('activeId', $Store.ActiveId); $w.Screens('cockpit', $Store.Cockpit)
    $ps = @($Store.Profiles); $w.N('profiles.count', $ps.Count)
    for ($k = 0; $k -lt $ps.Count; $k++) {
        $p = $ps[$k]; $q = "profiles[$k]"
        $w.S("$q.id", $p.Id); $w.S("$q.name", $p.Name); $w.S("$q.transmission", $p.Transmission); $w.S("$q.wheel", $p.Wheel); $w.S("$q.shifter", $p.Shifter)
        $w.N("$q.ffbStrength", $p.FfbStrength); $w.B("$q.ffbOn", $p.FfbOn); $w.Screens("$q.screens", $p.Screens)
        $bs = @($p.Bindings); $w.N("$q.bindings.count", $bs.Count)
        for ($i = 0; $i -lt $bs.Count; $i++) {
            $b = $bs[$i]; $r = "$q.bindings[$i]"
            $w.S("$r.action", $b.Action); $w.S("$r.instanceGuid", $b.InstanceGuid); $w.S("$r.productGuid", $b.ProductGuid); $w.S("$r.deviceName", $b.DeviceName)
            $w.S("$r.kind", $b.Kind); $w.N("$r.index", $b.Index); $w.N("$r.direction", $b.Direction); $w.N("$r.rest", $b.Rest)
            $w.N("$r.minimum", $b.Minimum); $w.N("$r.maximum", $b.Maximum); $w.B("$r.calibrated", $b.Calibrated); $w.B("$r.inverted", $b.Inverted)
            $w.I("$r.calLow", $b.CalLow); $w.I("$r.calCentre", $b.CalCentre); $w.I("$r.calHigh", $b.CalHigh)
        }
    }
    $gs = @($Store.Games); $w.N('games.count', $gs.Count)
    for ($k = 0; $k -lt $gs.Count; $k++) { $w.S("games[$k].id", $gs[$k].Id); $w.S("games[$k].profile", $gs[$k].Profile) }
    foreach ($id in @(@($gs | ForEach-Object { $_.Id }) + 'unassigned-game')) {
        $c = Get-DbceStoreChoice $Store $id
        $w.S("for[$id].profile", $c.Profile.Id); $w.S("for[$id].screens", $c.ScreensFrom)
    }
    ,$w.Out.ToArray()
}

function Format-DbceRuntimeSettings {
    param([Parameter(Mandatory)]$Settings)
    $w = DsDumper
    $w.N('schema', $Settings.Schema); $w.S('game', $Settings.Game); $w.S('profile', $Settings.Profile)
    $vs = @($Settings.Values); $w.N('values.count', $vs.Count)
    for ($k = 0; $k -lt $vs.Count; $k++) {
        $v = $vs[$k]; $q = "values[$k]"
        $w.S("$q.key", $v.Key)
        if ($v.Type -eq 'number') { $w.N("$q.number", $v.Value) } elseif ($v.Type -eq 'bool') { $w.B("$q.bool", $v.Value) } else { $w.S("$q.string", $v.Value) }
    }
    ,$w.Out.ToArray()
}
