<# Explicit developer stage commands. Physical outputs are muted by the game adapter;
   recordings retain original signals. Select the recorded stage/car in the game's menus. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('woden','drive','iracing')][string]$Game,
    [ValidateSet('Record','Replay','Stop','Status')][string]$Action = 'Status',
    [string]$Path,
    [string]$Output,
    [ValidateRange(1,1800)][int]$Seconds = 60,
    [switch]$Launch
)
$ErrorActionPreference = 'Stop'
$names = @{ woden='Super Woden Rally Edge'; drive='DRIVERally'; iracing='iracing-arcade' }
$apps = @{ woden='3218630'; drive='2494780'; iracing='3226450' }
$root = Join-Path $env:LOCALAPPDATA "Dbce/StagePlayback/$Game"
$request = Join-Path $root 'request.txt'
$status = Join-Path $root 'status.txt'
if ($Action -eq 'Status') { if (Test-Path -LiteralPath $status) { Get-Content -LiteralPath $status } else { 'No adapter status yet.' }; return }
if (Test-Path -LiteralPath $request) { throw "An unconsumed request already exists: $request" }
if ($Action -in @('Record','Replay') -and [string]::IsNullOrWhiteSpace($Path)) { throw '-Path is required.' }
if ($Launch -and $Action -notin @('Record','Replay')) { throw '-Launch requires Record or Replay.' }
if ($Action -in @('Record','Replay')) {
    $other = Get-Process -Name (@($names.Values) + 'artofrally' | Where-Object { $_ -ne $names[$Game] }) -ErrorAction SilentlyContinue
    if ($other) { throw ('Another game owns the test slot: ' + (($other | Select-Object -ExpandProperty ProcessName) -join ', ')) }
}
$id = [Guid]::NewGuid().ToString('N')
$values = [ordered]@{ id=$id; action=$Action.ToLowerInvariant(); expiresUtc=[DateTimeOffset]::UtcNow.AddMinutes(4).ToString('O') }
if ($Action -in @('Record','Replay')) {
    $values.path = [IO.Path]::GetFullPath($Path)
    if ($Action -eq 'Record') {
        if (Test-Path -LiteralPath $values.path) { throw 'Capture already exists.' }
        $values.seconds = $Seconds.ToString([Globalization.CultureInfo]::InvariantCulture)
    } else {
        if (-not (Test-Path -LiteralPath (Join-Path $values.path 'complete.tsv'))) { throw 'Recording has no completion seal.' }
        if (-not $Output) { $Output = Join-Path (Split-Path $values.path) ('replay-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + $id.Substring(0,8)) }
        $values.output = [IO.Path]::GetFullPath($Output)
        if (Test-Path -LiteralPath $values.output) { throw 'Result directory already exists.' }
    }
}
foreach($value in $values.Values) { if ([string]$value -match "[\r\n]") { throw 'Request values cannot contain newlines.' } }
New-Item -ItemType Directory -Path $root -Force | Out-Null
$temporary = Join-Path $root "$id.tmp"
[IO.File]::WriteAllLines($temporary, [string[]]@($values.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }), [Text.UTF8Encoding]::new($false))
[IO.File]::Move($temporary, $request)
if ($Launch -and -not (Get-Process -Name $names[$Game] -ErrorAction SilentlyContinue)) { Start-Process "steam://rungameid/$($apps[$Game])" }
Write-Output "Request $id prepared. The adapter will acknowledge this ID in $status."
Write-Output 'Select the matching single-player stage/car. F12 stops playback; restart the game to restore physical output.'
