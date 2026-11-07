# The C# wrapper (shared/ui/DbceUi.cs) against the built x64 dbce_ui.dll, in a PowerShell 7 process of its own (the DLL's
# hooks stay in the process that loads it): every P/Invoke signature is called once and its result checked. No window
# or swap chain; build.ps1 runs it after the host checks.
param([Parameter(Mandatory)][string]$Dll)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Add-Type -Path (Join-Path $root 'shared/ui/DbceUi.cs')
$ui = [Dbce.Wheel.Ui.DbceUi]
$checks = 0
function Check($ok, $what) { $script:checks++; if (-not $ok) { throw "wrapper check failed: $what" } }
$err = $null
Check (-not $ui::Configure('{}', $null, $false, [ref]$err)) 'Configure before Load'
Check ($ui::Load($Dll, [ref]$err)) "Load: $err"
Check (-not $ui::Configure('{"settings":{"product":"x","pages":{"ffb":[{"id":"a","kind":"knob"}]}}}', $null, $false, [ref]$err)) 'bad descriptor accepted'
Check ($err -like "*knob*") "refusal reason: $err"
$json = Get-Content -Raw (Join-Path $root 'native/panel/samples/art-of-rally.settings.json')
Check ($ui::Configure($json, $null, $true, [ref]$err)) "Configure: $err"
Check ($ui::SetToggle('ffb.enabled', $false)) 'SetToggle'
Check (-not $ui::SetToggle('ffb.strength', $true)) 'SetToggle on a percent row'
Check ($ui::SetChoice('ffb.device', 0)) 'SetChoice'
Check ($ui::SetNumber('ffb.strength', 55)) 'SetNumber'
Check ($ui::SetText('controls.steering', 'Wheel axis X', [Dbce.Wheel.Ui.DbceUiTone]::Good)) 'SetText'
Check ($ui::SetRowVisible('ffb.crash', $false)) 'SetRowVisible'
$ui::SetFfbOn($true)
$e = New-Object Dbce.Wheel.Ui.DbceUiEvent
Check (-not $ui::TryPollEvent([ref]$e)) 'values the mod sets are not reported back'
Check (-not $ui::Navigate([Dbce.Wheel.Ui.DbceUiNav]::Down)) 'Down while closed'
Check ($ui::Navigate([Dbce.Wheel.Ui.DbceUiNav]::Open) -and $ui::IsOpen) 'Open'
$page = [Dbce.Wheel.Ui.DbceUiPage]::Help; $more = $true; $focus = '?'
Check ($ui::GetState([ref]$page, [ref]$more, [ref]$focus)) 'GetState open'
Check ($page -eq [Dbce.Wheel.Ui.DbceUiPage]::Controls -and -not $more -and $focus -eq '') "state: $page $more '$focus'"
$ui::ShowToast('Bonnet camera', $null, 0)
$ui::NoteSaved($true, $null)
Check ($ui::Navigate([Dbce.Wheel.Ui.DbceUiNav]::Back) -and -not $ui::IsOpen) 'Back closes'
Check (-not $ui::AttachD3D9([IntPtr]::Zero)) 'AttachD3D9 of nothing'
Check ($ui::RenderEventFunc -ne [IntPtr]::Zero) 'RenderEventFunc'
Check (($ui::Status -band [Dbce.Wheel.Ui.DbceUiStatus]::Configured) -ne 0) "Status $($ui::Status)"
$line = $null
$lines = 0
while ($ui::TryPollLog([ref]$line)) { $lines++ }
Check ($lines -ge 2) "log lines: $lines"
"PASS C# wrapper against dbce_ui.dll (x64): $checks checks"
