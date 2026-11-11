# Reader of dbce-mod.json for the installer (PowerShell 5.1 and 7). Dot-source it:
#   . "$PSScriptRoot\DbceModFile.ps1"; $mod = Read-DbceModFile -Path $file; Format-DbceModFile $mod
# Read-DbceModFile returns the file with every default filled in (docs/DBCE-MOD-JSON.md, "Defaults"), so the
# installer, Wheelkit (C#) and the panel core (C++) see the same values. Format-DbceModFile writes the canonical
# dump the three readers' tests compare with schema/fixtures/<id>.dump.txt.
Set-StrictMode -Version 2

function script:DmProp($o, [string]$name) {
    if ($null -eq $o) { return $null }
    $p = $o.PSObject.Properties[$name]
    if ($null -eq $p) { return $null }
    return $p.Value
}
function script:DmStr($o, [string]$name) { $v = DmProp $o $name; if ($null -eq $v) { '' } else { [string]$v } }
function script:DmNum($o, [string]$name, [double]$fallback) { $v = DmProp $o $name; if ($null -eq $v) { $fallback } else { [double]$v } }
function script:DmBool($o, [string]$name) { $v = DmProp $o $name; if ($null -eq $v) { $false } else { [bool]$v } }
function script:DmList($o, [string]$name) { $v = DmProp $o $name; if ($null -eq $v) { ,@() } else { ,@($v) } }
function script:DmPairs($o) {
    $out = New-Object System.Collections.ArrayList
    if ($null -ne $o) { foreach ($p in $o.PSObject.Properties) { [void]$out.Add([pscustomobject]@{ Name = $p.Name; Value = $p.Value }) } }
    ,$out.ToArray()
}
function script:DmFromTo($o, [string]$name) {
    $r = @(foreach ($e in (DmList $o $name)) { [pscustomobject][ordered]@{ From = DmStr $e 'from'; To = DmStr $e 'to' } })
    ,$r
}

function Read-DbceModFile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)
    $text = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    $j = $text | ConvertFrom-Json
    if ((DmStr $j 'schema') -ne 'dbce.mod@1') { throw "$Path is not a dbce-mod.json (schema dbce.mod@1)." }
    foreach ($k in 'id', 'name', 'version') { if (-not (DmStr $j $k)) { throw "$Path has no '$k'." } }
    $pk = DmProp $j 'package'
    if ($null -eq $pk -or -not (DmStr $pk 'kind')) { throw "$Path has no package kind." }

    $loader = DmProp $pk 'loader'
    $package = [pscustomobject][ordered]@{
        Kind = DmStr $pk 'kind'
        Loader = if ($null -eq $loader) { $null } else { [pscustomobject][ordered]@{ Name = DmStr $loader 'name'; Version = DmStr $loader 'version'; Bundle = DmStr $loader 'bundle' } }
        Payload = DmFromTo $pk 'payload'; Optional = DmFromTo $pk 'optional'
        Conflicts = @(foreach ($c in (DmList $pk 'conflicts')) { [string]$c })
        SeedConfig = DmFromTo $pk 'seedConfig'
        KnownBuilds = @(foreach ($b in (DmList $pk 'knownBuilds')) { [pscustomobject][ordered]@{ File = DmStr $b 'file'; Sha256 = DmStr $b 'sha256'; Label = DmStr $b 'label' } })
        Strict = DmBool $pk 'strict'
        UiHost = $(if (DmStr $pk 'uiHost') { DmStr $pk 'uiHost' } else { 'game' })
        # Read by the installer only; not part of the canonical dump.
        OldReceipts = @(foreach ($o in (DmList $pk 'oldReceipts')) { [pscustomobject][ordered]@{ Receipt = DmStr $o 'receipt'; Backups = DmStr $o 'backups' } })
        # Installer only too: uninstall strips these marker blocks from game files (docs/DBCE-MOD-JSON.md).
        MarkerBlocks = @(foreach ($m in (DmList (DmProp $pk 'uninstall') 'markerBlocks')) { [pscustomobject][ordered]@{ File = DmStr $m 'file'; Begin = DmStr $m 'begin'; End = DmStr $m 'end'; Original = DmStr $m 'original' } })
    }

    $settings = $null
    $s = DmProp $j 'settings'
    if ($null -ne $s) {
        $config = DmStr $s 'config'
        $rows = New-Object System.Collections.ArrayList
        foreach ($page in (DmPairs (DmProp $s 'pages'))) {
            foreach ($r in @($page.Value)) {
                $kind = DmStr $r 'kind'
                $numeric = $kind -in 'percent', 'slider', 'number'
                $min = DmNum $r 'min' $(if ($kind -eq 'percent') { 0 } else { 0 })
                $max = DmNum $r 'max' $(if ($kind -eq 'percent') { 100 } else { 0 })
                $choices = @(foreach ($c in (DmList $r 'choices')) { [string]$c })
                $d = DmProp $r 'default'
                $default = 0.0
                if ($kind -eq 'toggle') { $default = $(if ($d -eq $true) { 1.0 } else { 0.0 }) }
                elseif ($kind -eq 'choice' -and $null -ne $d) {
                    if ($d -is [string]) { $default = [double][Array]::IndexOf([string[]]$choices, $d) } else { $default = [double]$d }
                }
                elseif ($numeric) { $default = $(if ($null -eq $d) { $min } else { [double]$d }) }
                $st = DmProp $r 'store'
                $store = $null
                if ($null -ne $st) {
                    $store = [pscustomobject][ordered]@{
                        File = $(if (DmStr $st 'file') { DmStr $st 'file' } else { $config })
                        Section = DmStr $st 'section'; Key = DmStr $st 'key'; On = DmStr $st 'on'; Off = DmStr $st 'off'
                        Scale = DmNum $st 'scale' 1
                    }
                }
                [void]$rows.Add([pscustomobject][ordered]@{
                    Page = $page.Name; Id = DmStr $r 'id'; Kind = $kind; Label = DmStr $r 'label'; Help = DmStr $r 'help'
                    Level = $(if (DmStr $r 'level') { DmStr $r 'level' } else { 'simple' })
                    Apply = $(if (DmStr $r 'apply') { DmStr $r 'apply' } else { 'live' })
                    Choices = $choices; Min = $min; Max = $max; Default = $default
                    Unit = DmStr $r 'unit'; Decimals = [int](DmNum $r 'decimals' 0); Text = DmStr $r 'text'
                    Role = DmStr $r 'role'; Store = $store
                })
            }
        }
        $settings = [pscustomobject][ordered]@{
            Product = DmStr $s 'product'; Version = DmStr $s 'version'; ModPage = DmStr $s 'modPage'; Keys = DmStr $s 'keys'
            Config = $config; Format = $(if (DmStr $s 'format') { DmStr $s 'format' } else { 'ini' }); Rows = $rows.ToArray()
        }
    }

    $controls = $null
    $c = DmProp $j 'controls'
    if ($null -ne $c) {
        $oi = DmProp $c 'optIn'
        $controls = [pscustomobject][ordered]@{
            Store = DmStr $c 'store'; Profile = DmStr $c 'profile'; Settings = DmStr $c 'settings'
            OptIn = if ($null -eq $oi) { $null } else { [pscustomobject][ordered]@{ Section = DmStr $oi 'section'; Key = DmStr $oi 'key'; Value = DmStr $oi 'value'; Default = DmStr $oi 'default'; AppliesAfterRestart = DmBool $oi 'appliesAfterRestart' } }
            Delivered = @(foreach ($a in (DmList $c 'delivered')) { [string]$a })
            Unsupported = @(foreach ($p in (DmPairs (DmProp $c 'unsupported'))) { [pscustomobject][ordered]@{ Action = $p.Name; Reason = [string]$p.Value } })
            Conditions = @(foreach ($x in (DmList $c 'conditions')) {
                [pscustomobject][ordered]@{
                    Actions = @(foreach ($a in (DmList $x 'actions')) { [string]$a }); Transmission = DmStr $x 'transmission'
                    SettingsKey = DmStr $x 'settingsKey'; SettingsValues = @(foreach ($v in (DmList $x 'settingsValues')) { [string]$v })
                    SettingsDefault = DmStr $x 'settingsDefault'; Note = DmStr $x 'note'
                } })
            GameControls = @(foreach ($g in (DmList $c 'gameControls')) { [pscustomobject][ordered]@{ Id = DmStr $g 'id'; Kind = DmStr $g 'kind'; Label = DmStr $g 'label'; Group = DmStr $g 'group' } })
        }
    }

    $cameras = @(foreach ($m in (DmList $j 'cameras')) {
        [pscustomobject][ordered]@{ Camera = DmStr $m 'camera'; Start = DmStr $m 'start'; File = DmStr $m 'file'; Section = DmStr $m 'section'
            Key = DmStr $m 'key'; Realistic = DmStr $m 'realistic'; Game = DmStr $m 'game' } })

    $selfTest = $null
    $t = DmProp $j 'selfTest'
    if ($null -ne $t) {
        $selfTest = [pscustomobject][ordered]@{
            Folder = DmStr $t 'folder'; Steps = @(foreach ($x in (DmList $t 'steps')) { [string]$x })
            Probes = @(foreach ($p in (DmList $t 'probes')) { [pscustomobject][ordered]@{ Id = DmStr $p 'id'; Type = DmStr $p 'type'; Label = DmStr $p 'label' } })
        }
    }

    $test = $null
    $tt = DmProp $j 'test'
    if ($null -ne $tt) {
        $launch = DmProp $tt 'launch'
        $entry = DmProp $tt 'entry'
        $quit = DmProp $tt 'quit'
        $ts = DmProp $tt 'testSave'
        $test = [pscustomobject][ordered]@{
            Launch = if ($null -eq $launch) { $null } else { [pscustomobject][ordered]@{ SteamAppId = [long](DmNum $launch 'steamAppId' 0); Exe = DmStr $launch 'exe'; Args = @(foreach ($a in (DmList $launch 'args')) { [string]$a }) } }
            Protect = @(foreach ($p in (DmList $tt 'protect')) { [string]$p })
            TestSave = if ($null -eq $ts) { $null } else { [pscustomobject][ordered]@{ From = DmStr $ts 'from'; To = DmStr $ts 'to' } }
            States = @(foreach ($s in (DmList $tt 'states')) { [pscustomobject][ordered]@{ State = DmStr $s 'state'; Field = DmStr $s 'field'; Is = DmStr $s 'is'; Text = DmStr $s 'text'; Picture = DmStr $s 'picture' } })
            Entry = if ($null -eq $entry) { $null } else { [pscustomobject][ordered]@{ Command = DmStr $entry 'command'; Route = DmStr $entry 'route' } }
            Route = @(foreach ($r in (DmList $tt 'route')) { [pscustomobject][ordered]@{ Key = DmStr $r 'key'; Input = DmStr $r 'input'; WaitState = DmStr $r 'waitState'; WaitText = DmStr $r 'waitText'; Timeout = [long](DmNum $r 'timeout' 0) } })
            Quit = if ($null -eq $quit) { $null } else { [pscustomobject][ordered]@{ Command = DmStr $quit 'command'; Key = DmStr $quit 'key'; Input = DmStr $quit 'input' } }
        }
    }

    $installed = $null
    $i = DmProp $j 'installed'
    if ($null -ne $i) {
        $installed = [pscustomobject][ordered]@{
            Version = DmStr $i 'version'; Commit = DmStr $i 'commit'; InstalledUtc = DmStr $i 'installedUtc'
            LoaderInstalledByUs = DmBool $i 'loaderInstalledByUs'
            Files = @(foreach ($f in (DmList $i 'files')) { [pscustomobject][ordered]@{ Path = DmStr $f 'path'; Sha256 = DmStr $f 'sha256' } })
            Backups = DmStr $i 'backups'
            Flags = @(foreach ($p in (DmPairs (DmProp $i 'flags'))) { [pscustomobject][ordered]@{ Name = $p.Name; Value = [bool]$p.Value } })
        }
    }
    $log = DmProp $j 'log'
    [pscustomobject][ordered]@{
        Schema = DmStr $j 'schema'; Id = DmStr $j 'id'; Name = DmStr $j 'name'; Version = DmStr $j 'version'
        SteamAppId = [long](DmNum $j 'steamAppId' 0); Folder = DmStr $j 'folder'
        Exe = @(foreach ($e in (DmList $j 'exe')) { [string]$e }); Repository = DmStr $j 'repository'
        LogFile = DmStr $log 'file'; LogStart = DmStr $log 'start'
        Package = $package; Settings = $settings; Controls = $controls; Cameras = $cameras; SelfTest = $selfTest; Test = $test; Installed = $installed
    }
}

# The canonical dump: one "path=value" line per value, in the order docs/DBCE-MOD-JSON.md gives.
function Format-DbceModFile {
    param([Parameter(Mandatory)]$Mod)
    $out = New-Object System.Collections.Generic.List[string]
    $wS = { param($p, $v) $out.Add($p + '=' + ([string]$v).Replace('\', '\\').Replace("`r", '\r').Replace("`n", '\n')) }
    $wB = { param($p, $v) $out.Add($p + '=' + $(if ($v) { 'true' } else { 'false' })) }
    $wN = { param($p, $v) $out.Add($p + '=' + ([double]$v).ToString('R', [Globalization.CultureInfo]::InvariantCulture)) }
    $wL = { param($p, $list) $a = @($list); & $wN "$p.count" $a.Count; for ($k = 0; $k -lt $a.Count; $k++) { & $wS "$p[$k]" $a[$k] } }
    $wFT = { param($p, $list) $a = @($list); & $wN "$p.count" $a.Count; for ($k = 0; $k -lt $a.Count; $k++) { & $wS "$p[$k].from" $a[$k].From; & $wS "$p[$k].to" $a[$k].To } }
    & $wS schema $Mod.Schema; & $wS id $Mod.Id; & $wS name $Mod.Name; & $wS version $Mod.Version
    & $wN steamAppId $Mod.SteamAppId; & $wS folder $Mod.Folder; & $wL exe $Mod.Exe; & $wS repository $Mod.Repository
    & $wS log.file $Mod.LogFile; & $wS log.start $Mod.LogStart
    $p = $Mod.Package
    & $wS package.kind $p.Kind
    if ($null -eq $p.Loader) { $out.Add('package.loader=null') } else { & $wS package.loader.name $p.Loader.Name; & $wS package.loader.version $p.Loader.Version; & $wS package.loader.bundle $p.Loader.Bundle }
    & $wFT package.payload $p.Payload; & $wFT package.optional $p.Optional; & $wL package.conflicts $p.Conflicts; & $wFT package.seedConfig $p.SeedConfig
    $kb = @($p.KnownBuilds); & $wN package.knownBuilds.count $kb.Count
    for ($k = 0; $k -lt $kb.Count; $k++) { & $wS "package.knownBuilds[$k].file" $kb[$k].File; & $wS "package.knownBuilds[$k].sha256" $kb[$k].Sha256; & $wS "package.knownBuilds[$k].label" $kb[$k].Label }
    & $wB package.strict $p.Strict; & $wS package.uiHost $p.UiHost
    $s = $Mod.Settings
    if ($null -eq $s) { $out.Add('settings=null') } else {
        & $wS settings.product $s.Product; & $wS settings.version $s.Version; & $wS settings.modPage $s.ModPage; & $wS settings.keys $s.Keys
        & $wS settings.config $s.Config; & $wS settings.format $s.Format
        $rows = @($s.Rows); & $wN settings.rows.count $rows.Count
        for ($k = 0; $k -lt $rows.Count; $k++) {
            $r = $rows[$k]; $q = "settings.rows[$k]"
            & $wS "$q.page" $r.Page; & $wS "$q.id" $r.Id; & $wS "$q.kind" $r.Kind; & $wS "$q.label" $r.Label; & $wS "$q.help" $r.Help
            & $wS "$q.level" $r.Level; & $wS "$q.apply" $r.Apply; & $wL "$q.choices" $r.Choices
            & $wN "$q.min" $r.Min; & $wN "$q.max" $r.Max; & $wN "$q.default" $r.Default; & $wS "$q.unit" $r.Unit; & $wN "$q.decimals" $r.Decimals
            & $wS "$q.text" $r.Text; & $wS "$q.role" $r.Role
            if ($null -eq $r.Store) { $out.Add("$q.store=null") } else {
                & $wS "$q.store.file" $r.Store.File; & $wS "$q.store.section" $r.Store.Section; & $wS "$q.store.key" $r.Store.Key
                & $wS "$q.store.on" $r.Store.On; & $wS "$q.store.off" $r.Store.Off; & $wN "$q.store.scale" $r.Store.Scale
            }
        }
    }
    $c = $Mod.Controls
    if ($null -eq $c) { $out.Add('controls=null') } else {
        & $wS controls.store $c.Store; & $wS controls.profile $c.Profile; & $wS controls.settings $c.Settings
        if ($null -eq $c.OptIn) { $out.Add('controls.optIn=null') } else {
            & $wS controls.optIn.section $c.OptIn.Section; & $wS controls.optIn.key $c.OptIn.Key; & $wS controls.optIn.value $c.OptIn.Value
            & $wS controls.optIn.default $c.OptIn.Default; & $wB controls.optIn.appliesAfterRestart $c.OptIn.AppliesAfterRestart
        }
        & $wL controls.delivered $c.Delivered
        $u = @($c.Unsupported); & $wN controls.unsupported.count $u.Count
        for ($k = 0; $k -lt $u.Count; $k++) { & $wS "controls.unsupported[$k].action" $u[$k].Action; & $wS "controls.unsupported[$k].reason" $u[$k].Reason }
        $cd = @($c.Conditions); & $wN controls.conditions.count $cd.Count
        for ($k = 0; $k -lt $cd.Count; $k++) {
            $q = "controls.conditions[$k]"
            & $wL "$q.actions" $cd[$k].Actions; & $wS "$q.transmission" $cd[$k].Transmission; & $wS "$q.settingsKey" $cd[$k].SettingsKey
            & $wL "$q.settingsValues" $cd[$k].SettingsValues; & $wS "$q.settingsDefault" $cd[$k].SettingsDefault; & $wS "$q.note" $cd[$k].Note
        }
        $g = @($c.GameControls); & $wN controls.gameControls.count $g.Count
        for ($k = 0; $k -lt $g.Count; $k++) { $q = "controls.gameControls[$k]"; & $wS "$q.id" $g[$k].Id; & $wS "$q.kind" $g[$k].Kind; & $wS "$q.label" $g[$k].Label; & $wS "$q.group" $g[$k].Group }
    }
    $m = @($Mod.Cameras); & $wN cameras.count $m.Count
    for ($k = 0; $k -lt $m.Count; $k++) {
        $q = "cameras[$k]"
        & $wS "$q.camera" $m[$k].Camera; & $wS "$q.start" $m[$k].Start; & $wS "$q.file" $m[$k].File; & $wS "$q.section" $m[$k].Section
        & $wS "$q.key" $m[$k].Key; & $wS "$q.realistic" $m[$k].Realistic; & $wS "$q.game" $m[$k].Game
    }
    $t = $Mod.SelfTest
    if ($null -eq $t) { $out.Add('selfTest=null') } else {
        & $wS selfTest.folder $t.Folder; & $wL selfTest.steps $t.Steps
        $pr = @($t.Probes); & $wN selfTest.probes.count $pr.Count
        for ($k = 0; $k -lt $pr.Count; $k++) { & $wS "selfTest.probes[$k].id" $pr[$k].Id; & $wS "selfTest.probes[$k].type" $pr[$k].Type; & $wS "selfTest.probes[$k].label" $pr[$k].Label }
    }
    $tt = $Mod.Test
    if ($null -eq $tt) { $out.Add('test=null') } else {
        if ($null -eq $tt.Launch) { $out.Add('test.launch=null') } else {
            & $wN test.launch.steamAppId $tt.Launch.SteamAppId; & $wS test.launch.exe $tt.Launch.Exe; & $wL test.launch.args $tt.Launch.Args
        }
        & $wL test.protect $tt.Protect
        if ($null -eq $tt.TestSave) { $out.Add('test.testSave=null') } else { & $wS test.testSave.from $tt.TestSave.From; & $wS test.testSave.to $tt.TestSave.To }
        $st = @($tt.States); & $wN test.states.count $st.Count
        for ($k = 0; $k -lt $st.Count; $k++) { $q = "test.states[$k]"; & $wS "$q.state" $st[$k].State; & $wS "$q.field" $st[$k].Field; & $wS "$q.is" $st[$k].Is; & $wS "$q.text" $st[$k].Text; & $wS "$q.picture" $st[$k].Picture }
        if ($null -eq $tt.Entry) { $out.Add('test.entry=null') } else { & $wS test.entry.command $tt.Entry.Command; & $wS test.entry.route $tt.Entry.Route }
        $rt = @($tt.Route); & $wN test.route.count $rt.Count
        for ($k = 0; $k -lt $rt.Count; $k++) { $q = "test.route[$k]"; & $wS "$q.key" $rt[$k].Key; & $wS "$q.input" $rt[$k].Input; & $wS "$q.waitState" $rt[$k].WaitState; & $wS "$q.waitText" $rt[$k].WaitText; & $wN "$q.timeout" $rt[$k].Timeout }
        if ($null -eq $tt.Quit) { $out.Add('test.quit=null') } else { & $wS test.quit.command $tt.Quit.Command; & $wS test.quit.key $tt.Quit.Key; & $wS test.quit.input $tt.Quit.Input }
    }
    $i = $Mod.Installed
    if ($null -eq $i) { $out.Add('installed=null') } else {
        & $wS installed.version $i.Version; & $wS installed.commit $i.Commit; & $wS installed.installedUtc $i.InstalledUtc
        & $wB installed.loaderInstalledByUs $i.LoaderInstalledByUs
        $f = @($i.Files); & $wN installed.files.count $f.Count
        for ($k = 0; $k -lt $f.Count; $k++) { & $wS "installed.files[$k].path" $f[$k].Path; & $wS "installed.files[$k].sha256" $f[$k].Sha256 }
        & $wS installed.backups $i.Backups
        $fl = @($i.Flags); & $wN installed.flags.count $fl.Count
        for ($k = 0; $k -lt $fl.Count; $k++) { & $wS "installed.flags[$k].name" $fl[$k].Name; & $wB "installed.flags[$k].value" $fl[$k].Value }
    }
    ,$out.ToArray()
}
