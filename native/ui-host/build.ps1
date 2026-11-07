# Builds dbce_ui.dll (x64 and x86) into build\<arch>\ and runs the WARP host check on each (test_ui_host.cpp):
# ImGui (vendored, unchanged) at /W0, the panel and the host at /W4 /WX. Fails when a DLL is 2 MB or more, or its exports
# differ from dbce_ui.def. Last, test_wrapper.ps1 calls every P/Invoke of shared/ui/DbceUi.cs against the x64 DLL in a
# PowerShell 7 process of its own. -NoTest builds only.
param([switch]$NoTest)
$ErrorActionPreference = 'Stop'
$pwsh = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
if (-not $pwsh -and (Test-Path (Join-Path $PSHOME 'pwsh.exe'))) { $pwsh = Join-Path $PSHOME 'pwsh.exe' }
# The owner's user PATH is longer than cmd accepts; vcvars needs the machine PATH (session only).
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ";${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$here = $PSScriptRoot
$im = '..\third_party\imgui'
$p = '..\panel'
$imgui = "$im\imgui.cpp $im\imgui_draw.cpp $im\imgui_tables.cpp $im\imgui_widgets.cpp $im\backends\imgui_impl_dx9.cpp $im\backends\imgui_impl_dx11.cpp $im\backends\imgui_impl_dx12.cpp"
$panel = "$p\dbce_panel.cpp $p\dbce_panel_descriptor.cpp $p\dbce_panel_input.cpp $p\dbce_overlay_d3d11.cpp $p\dbce_overlay_d3d12.cpp $p\dbce_overlay_d3d9.cpp"
$want = @(Get-Content (Join-Path $here 'dbce_ui.def') | Where-Object { $_ -match '^\s+dbce_ui_\w+\s*$' } | ForEach-Object { $_.Trim() } | Sort-Object)
$sample = (Resolve-Path (Join-Path $here '..\panel\samples\art-of-rally.settings.json')).Path
foreach ($arch in 'x64', 'x86') {
    $vcvars = Join-Path $vs ('VC\Auxiliary\Build\vcvars{0}.bat' -f $(if ($arch -eq 'x64') { '64' } else { '32' }))
    $dir = Join-Path $here "build\$arch"
    New-Item -ItemType Directory -Force (Join-Path $dir 'obj') | Out-Null
    $obj = Join-Path $dir 'obj'
    $dll = Join-Path $dir 'dbce_ui.dll'
    $cmd = "`"$vcvars`" >nul && cd /d `"$here`" && " +
        "cl /nologo /c /O1 /MT /EHsc /std:c++17 /W0 /DNDEBUG /I$im $imgui /Fo`"$obj\\`" && " +
        "cl /nologo /c /O1 /MT /EHsc /std:c++17 /W4 /WX /DNDEBUG /I$im $panel dbce_ui.cpp /Fo`"$obj\\`" && " +
        "link /nologo /DLL /OPT:REF /OPT:ICF /DEF:dbce_ui.def /OUT:`"$dll`" `"$obj\*.obj`" user32.lib ole32.lib d3dcompiler.lib && " +
        "dumpbin /nologo /exports `"$dll`""
    $out = cmd /c $cmd 2>&1
    if ($LASTEXITCODE) { $out | Select-Object -Last 30; throw "dbce_ui.dll build failed ($arch)" }
    $out | Select-String -Pattern ' warning ' | ForEach-Object { "${arch}: $($_.Line)" }
    $got = @($out | Where-Object { $_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(dbce_ui_\w+)' } | ForEach-Object { $Matches[1] } | Sort-Object)
    if (Compare-Object $want $got) { throw "dbce_ui.dll exports differ from dbce_ui.def ($arch): $($got -join ', ')" }
    $size = (Get-Item $dll).Length
    "${arch}: dbce_ui.dll $([math]::Round($size / 1MB, 2)) MB, $($got.Count) exports"
    if ($size -ge 2MB) { throw "dbce_ui.dll is $size bytes ($arch); the limit is 2 MB" }
    if ($NoTest) { continue }
    $test = "`"$vcvars`" >nul && cd /d `"$here`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX test_ui_host.cpp /Fo`"$obj\\`" /Fe`"$dir\test_ui_host.exe`" && `"$dir\test_ui_host.exe`" `"$dll`" `"$sample`" `"$dir`""
    $outT = cmd /c $test 2>&1
    $outT | Select-String -Pattern ' warning | error |FAIL|PASS|SKIP|event:|focus:|refused:' | ForEach-Object { "${arch}: $($_.Line)" }
    if ($LASTEXITCODE) { $outT | Select-Object -Last 40; throw "dbce_ui host check failed ($arch)" }
}
if (-not $NoTest) {
    if (-not $pwsh) { throw 'PowerShell 7 (pwsh) is needed for the C# wrapper check' }
    & $pwsh -NoProfile -File (Join-Path $here 'test_wrapper.ps1') -Dll (Join-Path $here 'build\x64\dbce_ui.dll')
    if ($LASTEXITCODE) { throw 'C# wrapper check failed' }
}
