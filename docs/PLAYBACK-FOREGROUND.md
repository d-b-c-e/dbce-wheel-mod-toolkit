# Ordinary foreground activation in a supervised test

`tools/playback/Stage-GameForeground.ps1` adds a shared, opt-in operation for an
already launched, recorded private game. It never starts another process or
changes game settings. Dot-sourcing the helper performs no native operations.

The caller supplies the actual PID, UTC start ticks, executable path and current
shared rig lease. Each observation checks that identity and the window's owner.
The helper waits at most 30 seconds for a visible window, then requests ordinary
Windows activation once. A minimized window is refused, not restored. If the
window is already foreground, no activation is requested. A denied request or
unobserved result is unmet; there is no alternate method or retry. Confirmation
of an accepted request is bounded to two seconds.

Windows can refuse activation even when a documented condition appears to hold.
The helper uses the ordinary [SetForegroundWindow API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
without changing foreground timeouts, attaching input queues, synthesizing
input, making windows topmost or changing process tokens.

`osForegroundObserved` is only an OS observation. The game must independently
report actual focus and consumed input. Input and force gates remain intact.
Callers retain their existing game lifetime bound, mute, close, restore and
lease ownership. The Over the Hill runner records `foreground-request.json`
and closes/restores on an unmet result before starting its input sender.

`Test-GameForeground.ps1` exercises the actual sequencing with fake operations:
wrong/reused process identity, no-window timeout, minimized refusal, accepted
but unobserved requests, asynchronous observation, window replacement and
denied activation coinciding with a foreground change. It calls no real native
APIs and does not qualify a game launch or physical input.
