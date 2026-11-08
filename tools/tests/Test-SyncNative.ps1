# Sync-Native.ps1 against two throwaway fixture repos: fresh sync, dropped tests, VERSION, -Check pass and fail, -DryRun.
$ErrorActionPreference = 'Stop'
$script = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Sync-Native.ps1'))
$case = Join-Path ([IO.Path]::GetTempPath()) ('sync-native-test-' + [guid]::NewGuid().ToString('N'))
$wheel = Join-Path $case 'wheel'; $triple = Join-Path $case 'triple'; $game = Join-Path $case 'game'; $vendor = Join-Path $game 'src/vendor'
$script:checks = 0
function Assert($ok, $message) { $script:checks++; if (-not $ok) { throw "FAIL: $message" } }
function Fixture([string]$repo, [string[]]$files) {
  foreach ($f in $files) { $p = Join-Path $repo $f; New-Item -ItemType Directory -Path (Split-Path -Parent $p) -Force | Out-Null; [IO.File]::WriteAllText($p, "// $f`nline two`n") }
  & git -C $repo init -q; & git -C $repo config core.autocrlf false; & git -C $repo add -A
  & git -C $repo -c user.name=SyncTest -c user.email=sync-test@example.invalid commit -qm fixture
  if ($LASTEXITCODE -ne 0) { throw 'fixture git failed' }
  (& git -C $repo rev-parse HEAD).Trim() }
function Sync([hashtable]$more, [string]$commit = 'HEAD') {
  $out = & $script -GameRepo $game -ToolkitRepo $wheel -TripleRepo $triple -Commit $commit -TripleCommit HEAD @more 2>&1 | Out-String
  @{ Out = $out; Code = $LASTEXITCODE } }
function Fails([scriptblock]$action) { try { & $action | Out-Null; return $false } catch { return $true } }
try {
  $kept = 'native/cameras/camera_sets.hpp', 'native/controls/dbce_controls.hpp', 'native/controls/NATIVE-BINDING-EDITOR.md', 'native/dinput-proxy/dbce_ffb_layer.cpp',
    'native/forza/forza_packet.h', 'native/panel/dbce_panel.cpp', 'native/settings/dbce_ini_single_key_win.hpp',
    'native/support/support_report.cpp', 'native/third_party/imgui/imgui.cpp', 'native/third_party/imgui/backends/imgui_impl_dx11.cpp',
    'shared/display/dbce_racing_row.hpp'
  $dropped = 'native/cameras/test.cpp', 'native/controls/test_controls.cpp', 'native/controls/build_test.ps1', 'native/controls/controls-vectors.txt',
    'native/controls/fixtures/axis.hpp', 'native/controls/.gitignore', 'native/dinput-proxy/tools/probe.cpp',
    'native/dinput-proxy/test_input_suppression.inc', 'native/dinput-proxy/build_ffb_layer.ps1', 'native/settings/test.cpp',
    'native/support/CMakeLists.txt', 'shared/display/RacingRow.cs', 'native/wheelffb/WheelFfb.cpp', 'docs/README.md'
  $wheelSha = Fixture $wheel ($kept + $dropped)
  $tripleSha = Fixture $triple 'native/compositor/dbce_triple_compositor.hpp', 'native/geometry/dbce_triple_geometry.hpp',
    'native/geometry/geometry-vectors.txt', 'native/layout/dbce_triple_ini.hpp'
  # A hand-vendored tree from before: one stale copy and the old free-text pin file.
  New-Item -ItemType Directory -Path (Join-Path $vendor 'panel') -Force | Out-Null
  [IO.File]::WriteAllText((Join-Path $vendor 'panel/dbce_panel.cpp'), "old copy`n")
  [IO.File]::WriteAllText((Join-Path $vendor 'VENDORED.txt'), "per-file pins`n")

  $r = Sync @{ DryRun = $true }
  Assert ($r.Code -eq 0 -and $r.Out -match 'changed panel/dbce_panel.cpp' -and $r.Out -match 'removed VENDORED.txt' -and $r.Out -match 'added   VERSION') "dry run lists deltas: $($r.Out)"
  Assert ([IO.File]::ReadAllText((Join-Path $vendor 'panel/dbce_panel.cpp')) -eq "old copy`n" -and -not (Test-Path (Join-Path $vendor 'VERSION'))) 'dry run wrote nothing'

  $r = Sync @{}
  Assert ($r.Code -eq 0) "sync failed: $($r.Out)"
  $files = @(Get-ChildItem -LiteralPath $vendor -File -Recurse -Force | ForEach-Object { $_.FullName.Substring($vendor.Length + 1).Replace('\', '/') } | Sort-Object)
  $expected = @('cameras/camera_sets.hpp', 'controls/dbce_controls.hpp', 'controls/NATIVE-BINDING-EDITOR.md', 'dinput-proxy/dbce_ffb_layer.cpp', 'display/dbce_racing_row.hpp',
    'forza/forza_packet.h', 'panel/dbce_panel.cpp', 'settings/dbce_ini_single_key_win.hpp', 'support/support_report.cpp',
    'third_party/imgui/backends/imgui_impl_dx11.cpp', 'third_party/imgui/imgui.cpp', 'triple-screen/compositor/dbce_triple_compositor.hpp',
    'triple-screen/geometry/dbce_triple_geometry.hpp', 'triple-screen/layout/dbce_triple_ini.hpp', 'VERSION') | Sort-Object
  Assert (($files -join ',') -eq ($expected -join ',')) "vendored set (tests dropped, old files removed): $($files -join ', ')"
  Assert ([IO.File]::ReadAllText((Join-Path $vendor 'VERSION')) -eq "dbce-wheel-mod-toolkit $wheelSha`ndbce-triple-screen-toolkit $tripleSha`n") 'two-line VERSION'
  Assert ([IO.File]::ReadAllText((Join-Path $vendor 'panel/dbce_panel.cpp')) -eq "// native/panel/dbce_panel.cpp`nline two`n") 'exact commit bytes'

  $r = Sync @{ Check = $true }; Assert ($r.Code -eq 0 -and $r.Out -match 'src/vendor matches') "fresh sync passes -Check: $($r.Out)"
  $crlf = Join-Path $vendor 'forza/forza_packet.h'
  [IO.File]::WriteAllText($crlf, [IO.File]::ReadAllText($crlf).Replace("`n", "`r`n"))
  $r = Sync @{ Check = $true }; Assert ($r.Code -eq 0) "CRLF checkout passes -Check: $($r.Out)"

  $edited = Join-Path $vendor 'dinput-proxy/dbce_ffb_layer.cpp'; $original = [IO.File]::ReadAllText($edited)
  [IO.File]::WriteAllText($edited, $original + "// hand edit`n")
  $r = Sync @{ Check = $true }; Assert ($r.Code -eq 1 -and $r.Out -match 'changed dinput-proxy/dbce_ffb_layer.cpp') "hand edit fails -Check: $($r.Out)"
  [IO.File]::WriteAllText($edited, $original)
  [IO.File]::WriteAllText((Join-Path $vendor 'panel/extra.cpp'), "stray`n")
  $r = Sync @{ Check = $true }; Assert ($r.Code -eq 1 -and $r.Out -match 'removed panel/extra.cpp') "stray file fails -Check: $($r.Out)"
  Remove-Item -LiteralPath $vendor -Recurse -Force
  Assert (Fails { Sync @{ Check = $true } }) 'missing VERSION fails -Check'
  Assert ((Fails { Sync @{} 'no-such-commit' }) -and -not (Test-Path -LiteralPath $vendor)) 'unknown commit fails and writes nothing'
  Write-Host "Sync-Native: $script:checks checks passed"
} finally {
  if (Test-Path -LiteralPath $case) { Remove-Item -LiteralPath $case -Recurse -Force }
}
