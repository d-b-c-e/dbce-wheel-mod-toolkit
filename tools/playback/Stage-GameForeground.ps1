# Ordinary activation for one recorded, owned game process. Dot-sourcing does not
# load user32, activate a window, change Windows policy, or start a process.
# https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setforegroundwindow

function Assert-StageForegroundIdentity {
    param($Actual, [int]$ProcessId, [long]$StartedUtcTicks, [string]$ExecutablePath)
    if ($Actual.Id -ne $ProcessId -or $Actual.StartedUtcTicks -ne $StartedUtcTicks -or
        ![string]::Equals($Actual.ExecutablePath, $ExecutablePath, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Recorded game process identity no longer matches; no activation'
    }
}

function Initialize-StageForegroundNative {
    if ('Dbce.StageForegroundNative' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Dbce {
    public static class StageForegroundNative {
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
        [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)] public static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)] public static extern bool IsIconic(IntPtr window);
        [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)] public static extern bool SetForegroundWindow(IntPtr window);
    }
}
'@
}

# Operation boundary permits focused tests without loading user32 or touching any
# real process/window. The ordinary entry point below supplies the real operations.
function Invoke-StageForegroundSequence {
    param([scriptblock]$Observe, [scriptblock]$Request, [scriptblock]$Wait,
          [scriptblock]$Elapsed, [int]$ReadyTimeoutMs = 30000, [int]$ConfirmTimeoutMs = 2000)
    $result = [ordered]@{ status = 'unmet'; requested = $false; requestAccepted = $null
        osForegroundObserved = $false; window = 0L; reason = $null; observations = 0 }
    try {
        $start = & $Elapsed
        while ($true) {
            $state = & $Observe
            $result.observations++
            if ($state.Window -ne 0 -and $state.Minimized) { throw 'Recorded game window is minimized; not restored or activated' }
            if ($state.Window -ne 0 -and $state.Visible) { break }
            if ((& $Elapsed) - $start -ge $ReadyTimeoutMs) { throw 'No visible recorded game window within the bound' }
            & $Wait
        }
        $result.window = [long]$state.Window
        if ($state.Foreground -eq $state.Window) {
            $result.status = 'observed'; $result.osForegroundObserved = $true
            $result.reason = 'Recorded game window was already foreground'
            return [pscustomobject]$result
        }
        # Request rechecks process and HWND ownership immediately before calling
        # Windows. Exactly one ordinary request; no retries or alternate methods.
        $result.requested = $true
        $result.requestAccepted = [bool](& $Request ([long]$state.Window))
        $requestedAt = & $Elapsed
        while ($true) {
            $current = & $Observe
            $result.observations++
            if ($current.Window -ne $result.window -or !$current.Visible -or $current.Minimized) {
                throw 'Recorded game window changed while confirming activation'
            }
            if (!$result.requestAccepted) { throw 'Windows refused the ordinary foreground request' }
            if ($current.Foreground -eq $result.window) {
                $result.status = 'observed'; $result.osForegroundObserved = $true
                $result.reason = 'Recorded game window observed as OS foreground'
                break
            }
            if ((& $Elapsed) - $requestedAt -ge $ConfirmTimeoutMs) { throw 'Windows accepted the request but foreground was not observed within the bound' }
            & $Wait
        }
    } catch { $result.reason = $_.Exception.GetBaseException().Message }
    return [pscustomobject]$result
}

function Request-StageGameForeground {
    param([Parameter(Mandatory)][ValidateRange(1, 2147483647)][int]$ProcessId,
          [Parameter(Mandatory)][ValidateRange(1, [long]::MaxValue)][long]$StartedUtcTicks,
          [Parameter(Mandatory)][string]$ExecutablePath,
          [Parameter(Mandatory)][string]$LeasePath,
          [Parameter(Mandatory)][string]$LeaseToken,
          [ValidateRange(100, 60000)][int]$ReadyTimeoutMs = 30000,
          [ValidateRange(100, 5000)][int]$ConfirmTimeoutMs = 2000)
    # Keep the existing shared lease contract; no separate focus lease or broker.
    . (Join-Path $PSScriptRoot 'Stage-RigLease.ps1')
    Assert-StageRigLease -Path $LeasePath -Token $LeaseToken
    $expectedPath = [IO.Path]::GetFullPath($ExecutablePath)
    Initialize-StageForegroundNative
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $observe = {
        Assert-StageRigLease -Path $LeasePath -Token $LeaseToken
        $owned = Get-Process -Id $ProcessId -ErrorAction Stop
        try {
            $owned.Refresh()
            if ($owned.HasExited) { throw 'Recorded game process has exited' }
            Assert-StageForegroundIdentity -Actual ([pscustomobject]@{
                Id = $owned.Id; StartedUtcTicks = $owned.StartTime.ToUniversalTime().Ticks
                ExecutablePath = [string]$owned.Path
            }) -ProcessId $ProcessId -StartedUtcTicks $StartedUtcTicks -ExecutablePath $expectedPath
            $window = $owned.MainWindowHandle
            if ($window -ne [IntPtr]::Zero) {
                [uint32]$windowProcess = 0
                $thread = [Dbce.StageForegroundNative]::GetWindowThreadProcessId($window, [ref]$windowProcess)
                if (!$thread -or $windowProcess -ne $ProcessId) { throw 'Window no longer belongs to the recorded game process' }
            }
            return [pscustomobject]@{ Window = $window.ToInt64()
                Foreground = [Dbce.StageForegroundNative]::GetForegroundWindow().ToInt64()
                Visible = ($window -ne [IntPtr]::Zero -and [Dbce.StageForegroundNative]::IsWindowVisible($window))
                Minimized = ($window -ne [IntPtr]::Zero -and [Dbce.StageForegroundNative]::IsIconic($window)) }
        } finally { $owned.Dispose() }
    }
    $request = {
        param([long]$Window)
        $latest = & $observe
        if ($latest.Window -ne $Window -or !$latest.Visible -or $latest.Minimized) { throw 'Recorded game window changed before activation' }
        [Dbce.StageForegroundNative]::SetForegroundWindow([IntPtr]$Window)
    }
    $result = Invoke-StageForegroundSequence -Observe $observe -Request $request -Wait { Start-Sleep -Milliseconds 100 } `
        -Elapsed { $timer.ElapsedMilliseconds } -ReadyTimeoutMs $ReadyTimeoutMs -ConfirmTimeoutMs $ConfirmTimeoutMs
    $result | Add-Member -NotePropertyName processId -NotePropertyValue $ProcessId
    $result | Add-Member -NotePropertyName startedUtcTicks -NotePropertyValue $StartedUtcTicks
    $result | Add-Member -NotePropertyName executablePath -NotePropertyValue $expectedPath
    $result | Add-Member -NotePropertyName elapsedMilliseconds -NotePropertyValue $timer.ElapsedMilliseconds
    # OS foreground is not Unity focus or delivered input. The game's independent
    # observer must still report those; its input/FFB gates stay unchanged.
    return $result
}
