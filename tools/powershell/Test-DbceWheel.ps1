<#
.SYNOPSIS
    Tests for DbceWheel.psm1. Run: powershell -File .\Test-DbceWheel.ps1

.DESCRIPTION
    Set-IniValueCrlf is covered exhaustively because line-ending corruption is
    silent - the game just stops reading its own config, and the diff looks
    fine. The discovery functions read real machine state, so they are checked
    for shape and reported, not asserted against a specific machine.

    Exit code 0 = all passed, 1 = at least one failure.
#>
[CmdletBinding()] param()

Import-Module (Join-Path $PSScriptRoot 'DbceWheel.psm1') -Force
$script:fails = 0
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("dbcewheel-" + [guid]::NewGuid().ToString('n'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

function Check($name, $condition) {
    if ($condition) { Write-Host "  ok   $name" -ForegroundColor Green }
    else { Write-Host "  FAIL $name" -ForegroundColor Red; $script:fails++ }
}

Write-Host "Set-IniValueCrlf" -ForegroundColor Cyan
$cases = @(
    @{ Name = 'append to the last section, file ended with a newline'
       Body = "[a]`r`nx=1`r`n`r`n[last]`r`nk=v`r`n"; Section = 'last'; Key = 'new'; Value = '1'; KeepsFinalNewline = $true }
    @{ Name = 'append to the last section, file did not'
       Body = "[a]`r`nx=1`r`n`r`n[last]`r`nk=v";     Section = 'last'; Key = 'new'; Value = '1'; KeepsFinalNewline = $false }
    @{ Name = 'update a key in a middle section'
       Body = "[mid]`r`nk=old`r`n`r`n[last]`r`nz=9`r`n"; Section = 'mid'; Key = 'k'; Value = 'new'; KeepsFinalNewline = $true }
    @{ Name = 'create a section that is absent'
       Body = "[a]`r`nx=1`r`n"; Section = 'fresh'; Key = 'k'; Value = 'v'; KeepsFinalNewline = $true }
    @{ Name = 'section name containing regex metacharacters'
       Body = "[a]`r`nx=1`r`n`r`n[/Wheel.Config/0006346e]`r`nWheel_Steer=X`r`n"
       Section = '/Wheel.Config/0006346e'; Key = 'Wheel_Accelerator'; Value = 'Y&-1.0&1.0'; KeepsFinalNewline = $true }
    @{ Name = 'replaces "key = value" rather than duplicating it'
       Body = "[FFB]`r`nFFBDiagnosticLog = false`r`nFFBGlobalStrength = 1.0`r`n"
       Section = 'FFB'; Key = 'FFBDiagnosticLog'; Value = 'true'; KeepsFinalNewline = $true }
    @{ Name = 'preserves the spacing around ='
       Body = "[s]`r`nk   =   old`r`n"; Section = 's'; Key = 'k'; Value = 'new'; KeepsFinalNewline = $true
       Expect = '(?m)^k   =   new\r?$' }
    @{ Name = 'a new key copies the section spacing'
       Body = "[s]`r`nexisting = 1`r`n"; Section = 's'; Key = 'fresh'; Value = '2'; KeepsFinalNewline = $true
       Expect = '(?m)^fresh = 2\r?$' }
    @{ Name = 'an indented key is replaced in place'
       Body = "[s]`r`n`tk = old`r`n"; Section = 's'; Key = 'k'; Value = 'new'; KeepsFinalNewline = $true
       Expect = "(?m)^`tk = new\r?$" }
    @{ Name = 'an empty value is allowed and written'
       Body = "[s]`r`nDeviceGuid = auto`r`n"; Section = 's'; Key = 'DeviceGuid'; Value = ''; KeepsFinalNewline = $true }
    @{ Name = 'an LF-only file stays LF-only'
       Body = "[a]`nx=1`n[last]`nk=v`n"; Section = 'last'; Key = 'new'; Value = '1'; KeepsFinalNewline = $true }
)
foreach ($c in $cases) {
    $path = Join-Path $tmp 'case.ini'
    [IO.File]::WriteAllText($path, $c.Body)
    Set-IniValueCrlf -Path $path -Section $c.Section -Key $c.Key -Value $c.Value -WarningAction SilentlyContinue
    $raw = [IO.File]::ReadAllText($path)
    $crlf = $c.Body.Contains("`r`n")
    $nl = if ($crlf) { "`r`n" } else { "`n" }

    $ok = $true
    # No CRLF may have been split into a bare LF.
    if ($crlf -and ([regex]::Matches($raw, "(?<!`r)`n")).Count -ne 0) { $ok = $false }
    # An LF-only file must not gain CRLFs.
    if (-not $crlf -and $raw.Contains("`r")) { $ok = $false }
    # The value must be there, exactly once, whatever the spacing.
    # A carriage return is allowed before the anchor: .NET's multiline '$'
    # matches before the line feed, so with CRLF the CR sits in front of it.
    if (([regex]::Matches($raw, "(?m)^[ \t]*$([regex]::Escape($c.Key))[ \t]*=[ \t]*$([regex]::Escape($c.Value))[ \t\r]*$")).Count -ne 1) { $ok = $false }
    # The key must appear exactly once in total - never duplicated.
    if (([regex]::Matches($raw, "(?m)^[ \t]*$([regex]::Escape($c.Key))[ \t]*=")).Count -ne 1) { $ok = $false }
    # Some cases pin the exact rendering.
    if ($c.Expect -and $raw -notmatch $c.Expect) { $ok = $false }
    # The trailing newline the file arrived with must survive.
    if ($c.KeepsFinalNewline -and -not $raw.EndsWith($nl)) { $ok = $false }
    # Nothing outside the edited section may move.
    if ($c.Section -ne 'a' -and $c.Body.Contains('[a]') -and $raw -notmatch "\[a\]$([regex]::Escape($nl))x=1") { $ok = $false }
    Check $c.Name $ok
}

# Updating the same key twice must not duplicate it.
$path = Join-Path $tmp 'twice.ini'
[IO.File]::WriteAllText($path, "[s]`r`nk=1`r`n")
Set-IniValueCrlf -Path $path -Section 's' -Key 'k' -Value '2' -WarningAction SilentlyContinue
Set-IniValueCrlf -Path $path -Section 's' -Key 'k' -Value '3' -WarningAction SilentlyContinue
$raw = [IO.File]::ReadAllText($path)
Check 'setting the same key twice replaces, never duplicates' (([regex]::Matches($raw, '(?m)^k=')).Count -eq 1 -and $raw -match '(?m)^k=3')

# A key whose name is a prefix of another must not be confused for it.
$path = Join-Path $tmp 'prefix.ini'
[IO.File]::WriteAllText($path, "[s]`r`nWheel_Steer=A`r`nWheel_SteerDeadzone=B`r`n")
Set-IniValueCrlf -Path $path -Section 's' -Key 'Wheel_Steer' -Value 'C' -WarningAction SilentlyContinue
$raw = [IO.File]::ReadAllText($path)
Check 'a key that is a prefix of another is not overwritten' ($raw -match '(?m)^Wheel_Steer=C' -and $raw -match '(?m)^Wheel_SteerDeadzone=B')

Write-Host "`nDiscovery (reports this machine; not asserted)" -ForegroundColor Cyan
$libs = @(Get-SteamLibraries)
Check 'Get-SteamLibraries returns at least one existing folder' ($libs.Count -gt 0 -and (Test-Path $libs[0]))
foreach ($l in $libs) { Write-Host "       $l" -ForegroundColor DarkGray }

$devs = @(Get-DirectInputDevices)
Write-Host "       $($devs.Count) DirectInput controller(s)" -ForegroundColor DarkGray
foreach ($d in $devs) { Write-Host "       $($d.Key)  $($d.Name)  type=0x$('{0:x}' -f $d.Type) ffb=$($d.ForceFeedback)" -ForegroundColor DarkGray }
Check 'every enumerated device has an 8-hex-digit product key' (($devs | Where-Object { $_.Key -notmatch '^[0-9a-f]{8}$' }).Count -eq 0)
Check 'Vid and Pid are derived consistently from the key' (($devs | Where-Object { $_.Key -ne ($_.Pid + $_.Vid) }).Count -eq 0)

Check 'Test-GameRunning is false for a name nothing uses' (-not (Test-GameRunning -ProcessName 'dbce-no-such-process'))

Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
if ($fails) { Write-Host "`n$fails check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host "`nDbceWheel.psm1: all checks passed" -ForegroundColor Green
exit 0
