#Requires -Version 7.0
# Foreground for Play-Macro -RequireForeground. Dot-sourcing loads nothing native and touches no window; the player
# passes the real operations, the tests pass mocks.
#
# Order, once at startup and never again: the game window already in front -> the ordinary Stage-GameForeground
# request -> a reobservation -> ONE activation-key fallback (novi, 2026-10-28: unattended launches never get focus with
# the rig's foreground lock timeout at its maximum; one F24 through SendInput, then SetForegroundWindow, brings the
# game to the front). SendInput is global input: the F24 press reaches whatever window is in front at that moment.
# It is a deliberate, minimal, logged exception, attempted only when the ordinary request is genuinely unmet, after
# the lease, the game's identity and its exact visible, unminimized window are revalidated, and only when no
# modifier or F24 is already held. Success comes only from observing the exact window in front.

# Observe: {Window, Owner} in front. Request: the ordinary helper's result {status, window, reason}.
# Validate(window) throws unless lease, identity and the window are still right. KeysHeld: any modifier or F24 down.
# SendF24: inserts F24 down and up in ONE SendInput call, returns the events inserted. Release: F24 up, returns the
# events inserted (must be 1).
# Activate(window): SetForegroundWindow (its return is ignored). Log(line).
function Invoke-MacroForeground {
    param([Parameter(Mandatory)][long]$ProcessId, [scriptblock]$Observe, [scriptblock]$Request, [scriptblock]$Validate,
          [scriptblock]$KeysHeld, [scriptblock]$SendF24, [scriptblock]$Release, [scriptblock]$Activate, [scriptblock]$Log = {})
    $r = [ordered]@{ status = 'unmet'; via = $null; window = 0L; fallbackAttempted = $false; eventsInserted = 0; reason = $null }
    $inFront = { param($w) $f = & $Observe; $f.Window -and $f.Owner -eq $ProcessId -and (!$w -or $f.Window -eq $w) }
    $f = & $Observe
    if ($f.Window -and $f.Owner -eq $ProcessId) { $r.status = 'observed'; $r.via = 'already in front'; $r.window = [long]$f.Window; return [pscustomobject]$r }
    $fg = & $Request
    $r.window = [long]$fg.window
    if ($fg.status -eq 'observed') { $r.status = 'observed'; $r.via = 'ordinary request'; return [pscustomobject]$r }
    if (!$r.window) { $r.reason = "ordinary request unmet ($($fg.reason)); no game window for the fallback"; return [pscustomobject]$r }
    if (& $inFront $r.window) { $r.status = 'observed'; $r.via = 'reobserved after the ordinary request'; return [pscustomobject]$r }
    try {
        & $Validate $r.window
        if (& $KeysHeld) { throw 'a modifier or F24 is held; no key sent' }
        $r.fallbackAttempted = $true
        # One key-up, checked: exactly one event inserted, else the key may still be down and the report says so.
        $releaseF24 = {
            try { $k = [int](& $Release) } catch { return "F24 release failed ($($_.Exception.Message)); F24 may still be down" }
            if ($k -ne 1) { return "F24 release inserted $k of 1 event; F24 may still be down" }
            'F24 released'
        }
        try { $r.eventsInserted = [int](& $SendF24) }
        catch { throw "SendInput failed ($($_.Exception.Message)); $(& $releaseF24)" }
        if ($r.eventsInserted -ne 2) {
            throw "SendInput inserted $($r.eventsInserted) of 2 events$(if ($r.eventsInserted -gt 0) { '; ' + (& $releaseF24) })"
        }
        [void](& $Activate $r.window)
        if (& $inFront $r.window) { $r.status = 'observed'; $r.via = 'activation-key fallback' }
        else { $r.reason = 'the game window is not in front after the activation-key fallback' }
    } catch { $r.reason = "ordinary request unmet ($($fg.reason)); $($_.Exception.Message)" }
    & $Log ('foreground fallback: attempted={0} events inserted={1} result={2}{3}' -f $r.fallbackAttempted, $r.eventsInserted, $r.status, $(if ($r.reason) { " ($($r.reason))" }))
    [pscustomobject]$r
}

# During playback: read-only, never activates. Throws when the exact game window is no longer in front.
function Assert-MacroForeground([long]$Window, [long]$ProcessId, [scriptblock]$Observe) {
    $f = & $Observe
    if ($f.Window -ne $Window -or $f.Owner -ne $ProcessId) { throw 'the game window lost the foreground' }
}

# The real F24 and key-state operations (user32), loaded only by a run that reaches the fallback.
function Initialize-MacroActivation {
    if ('Dbce.MacroActivation' -as [type]) { return }
    Add-Type -Namespace Dbce -Name MacroActivation -MemberDefinition @'
[StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public System.IntPtr dwExtraInfo; }
[StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public System.IntPtr dwExtraInfo; }
[StructLayout(LayoutKind.Explicit)] public struct UNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
[StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public UNION u; }
[DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint n, INPUT[] inputs, int size);
[DllImport("user32.dll")] static extern short GetAsyncKeyState(int vk);
const ushort F24 = 0x87;
static INPUT Key(bool up) { var i = new INPUT { type = 1 }; i.u.ki.wVk = F24; i.u.ki.dwFlags = up ? 2u : 0u; return i; }
public static bool AnyHeld() {   // Shift, Ctrl, Alt, left and right Windows, F24
    foreach (int vk in new[] { 0x10, 0x11, 0x12, 0x5B, 0x5C, F24 }) if ((GetAsyncKeyState(vk) & 0x8000) != 0) return true;
    return false;
}
public static uint DownUp() { return SendInput(2, new[] { Key(false), Key(true) }, Marshal.SizeOf(typeof(INPUT))); }
public static uint Up() { return SendInput(1, new[] { Key(true) }, Marshal.SizeOf(typeof(INPUT))); }
'@
}
