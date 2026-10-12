#Requires -Version 7.0
# Read-only launch admission. No window activation, input injection, waiting or launch.
function Test-StageOwnerActivityAllowed {
    [CmdletBinding()]
    param([string]$PolicyPath=(Join-Path $env:LOCALAPPDATA 'dbce/rig-activity-policy.json'))
    if (!(Test-Path -LiteralPath $PolicyPath)) { return $false }
    $policy = [IO.File]::ReadAllText($PolicyPath) | ConvertFrom-Json -ErrorAction Stop
    if ($policy.schemaVersion -ne 1 -or $policy.allowOwnerActivity -isnot [bool]) {
        throw 'Invalid rig activity policy; expected schemaVersion 1 and Boolean allowOwnerActivity.'
    }
    return $policy.allowOwnerActivity
}

function Get-StageOwnerIdleSeconds {
    [CmdletBinding()]
    param()
    if (!$IsWindows) { throw 'Owner idle observation requires Windows; launch refused.' }
    if (!('Dbce.StagePlayback.OwnerIdleNative' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Dbce.StagePlayback {
    public static class OwnerIdleNative {
        [StructLayout(LayoutKind.Sequential)]
        struct LastInput { public uint Size; public uint Tick; }
        [DllImport("user32.dll", SetLastError = true)]
        static extern bool GetLastInputInfo(ref LastInput input);
        public static double ReadSeconds() {
            var input = new LastInput { Size = (uint)Marshal.SizeOf(typeof(LastInput)) };
            if (!GetLastInputInfo(ref input))
                throw new InvalidOperationException("GetLastInputInfo failed; launch refused.");
            uint elapsed = unchecked((uint)Environment.TickCount - input.Tick);
            // uint subtraction handles the 49-day tick wrap. A future/ambiguous
            // timestamp is not evidence of idle; conservatively reject >24 days.
            if (elapsed > Int32.MaxValue)
                throw new InvalidOperationException("Ambiguous last-input timestamp; launch refused.");
            return elapsed / 1000.0;
        }
    }
}
'@ -ErrorAction Stop
    }
    return [Dbce.StagePlayback.OwnerIdleNative]::ReadSeconds()
}

function Assert-StageOwnerIdle {
    [CmdletBinding()]
    param([ValidateRange(1,86400)][int]$MinimumSeconds=300,
          [scriptblock]$ReadSeconds={ Get-StageOwnerIdleSeconds },
          [bool]$AllowOwnerActivity=(Test-StageOwnerActivityAllowed))
    if ($AllowOwnerActivity) {
        # Explicit owner direction supersedes idle admission. Do not invent an idle
        # observation; this policy does not waive the rig lease or force muting.
        return [pscustomobject]@{utc=[DateTime]::UtcNow.ToString('O');source='owner-activity-allowed';idleSeconds=$null;minimumSeconds=$MinimumSeconds;ownerActivityAllowed=$true}
    }
    $values=@(& $ReadSeconds)
    if ($values.Count -ne 1 -or $null -eq $values[0] -or
        $values[0] -isnot [ValueType] -or $values[0] -is [bool]) {
        throw 'Owner idle observation is invalid; launch refused.'
    }
    try { $seconds=[double]$values[0] } catch { throw 'Owner idle observation is not numeric; launch refused.' }
    if ([double]::IsNaN($seconds) -or [double]::IsInfinity($seconds) -or $seconds -lt 0) {
        throw 'Owner idle observation is invalid; launch refused.'
    }
    if ($seconds -lt $MinimumSeconds) {
        throw ('Recent keyboard/mouse input: idle {0:F1}s, need {1}s; no launch. Retry after the owner is idle.' -f $seconds,$MinimumSeconds)
    }
    [pscustomobject]@{utc=[DateTime]::UtcNow.ToString('O');source='GetLastInputInfo';idleSeconds=$seconds;minimumSeconds=$MinimumSeconds;ownerActivityAllowed=$false}
}
