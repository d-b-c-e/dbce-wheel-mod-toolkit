$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage-GameForeground.ps1')
$checks = 0
function Check([bool]$Condition, [string]$Name) {
    if (!$Condition) { throw "FAIL: $Name" }; $script:checks++
}
function WindowState([long]$window = 10, [long]$front = 20, [bool]$visible = $true, [bool]$minimized = $false) {
    [pscustomobject]@{ Window = $window; Foreground = $front; Visible = $visible; Minimized = $minimized }
}
function Run-Sequence($States, [bool]$Accepted = $true, [bool]$RefuseIdentity = $false) {
    $context = @{ next = 0; time = 0; requests = 0 }
    $result = Invoke-StageForegroundSequence -Observe {
        if ($RefuseIdentity) { throw 'identity mismatch' }
        $index = [Math]::Min($context.next, $States.Count - 1)
        $context.next++
        $States[$index]
    } -Request { param($window) $context.requests++; $Accepted } -Wait { $context.time += 100 } `
        -Elapsed { $context.time } -ReadyTimeoutMs 300 -ConfirmTimeoutMs 200
    [pscustomobject]@{ result = $result; requests = $context.requests }
}
$identity = [pscustomobject]@{ Id = 55; StartedUtcTicks = 123L; ExecutablePath = 'E:\private\game.exe' }
Assert-StageForegroundIdentity $identity 55 123 'e:\PRIVATE\game.exe'
Check $true 'exact identity accepts ordinary path case difference'
foreach ($wrong in @(
    [pscustomobject]@{ Id = 56; StartedUtcTicks = 123L; ExecutablePath = $identity.ExecutablePath },
    [pscustomobject]@{ Id = 55; StartedUtcTicks = 124L; ExecutablePath = $identity.ExecutablePath },
    [pscustomobject]@{ Id = 55; StartedUtcTicks = 123L; ExecutablePath = 'E:\owner\game.exe' })) {
    $refused = $false
    try { Assert-StageForegroundIdentity $wrong 55 123 $identity.ExecutablePath } catch { $refused = $true }
    Check $refused 'wrong PID, reused PID or different executable refuses'
}
$r = Run-Sequence @((WindowState 10 10))
Check ($r.result.osForegroundObserved -and $r.requests -eq 0) 'already foreground makes no request'
$r = Run-Sequence @((WindowState), (WindowState 10 20), (WindowState 10 10))
Check ($r.result.osForegroundObserved -and $r.requests -eq 1) 'asynchronous activation observed after one request'
$r = Run-Sequence @((WindowState)) $false
Check (!$r.result.osForegroundObserved -and $r.requests -eq 1 -and $r.result.reason -match 'refused') 'denial stops after one request'
$r = Run-Sequence @((WindowState), (WindowState 10 10)) $false
Check (!$r.result.osForegroundObserved -and $r.requests -eq 1 -and $r.result.reason -match 'refused') 'coincident foreground does not override a denied request'
$r = Run-Sequence @((WindowState))
Check (!$r.result.osForegroundObserved -and $r.requests -eq 1 -and $r.result.reason -match 'not observed') 'native true alone is not focus proof'
$r = Run-Sequence @((WindowState 10 20 $true $true))
Check (!$r.result.osForegroundObserved -and $r.requests -eq 0) 'minimized game is not restored'
$r = Run-Sequence @((WindowState 0 20 $false))
Check (!$r.result.osForegroundObserved -and $r.requests -eq 0 -and $r.result.observations -eq 4) 'missing visible window has a bounded wait'
$r = Run-Sequence @((WindowState), (WindowState 11 11))
Check (!$r.result.osForegroundObserved -and $r.requests -eq 1) 'window replacement invalidates confirmation'
$r = Run-Sequence @((WindowState)) $true $true
Check (!$r.result.osForegroundObserved -and $r.requests -eq 0) 'identity failure never activates'
Check (-not ('Dbce.StageForegroundNative' -as [type])) 'fixtures never load user32 or touch a real window'
"PASS: $checks focused checks; no real process, window, input, or device operations"
