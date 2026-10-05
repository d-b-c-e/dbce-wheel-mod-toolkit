# Supervising one playback process

`tools/playback/Watch-StageProcess.ps1` is a PowerShell 7 companion to a game's
session runner. It supervises an already-launched process by PID, executable path
and UTC process-start ticks, retaining that process's handle. It never enumerates
processes to terminate by name, changes monitor profiles or adjusts windows.

The runner passes the unique session request ID, a heartbeat path, a new log path
and a stop-file path. A game adapter emits this once per render heartbeat:

```text
123 7680x1440 FullScreenWindow 2026-10-05T07:00:00.0000000Z native phase request=0123456789abcdef0123456789abcdef outputsMuted=true
```

The timestamp must be recent and later than the process start. The request ID
must match. `outputsMuted=true` is a producer contract: all physical wheel/pad
and motion-network output remains disabled for that process lifetime while the
original requested data is captured separately. A later false heartbeat revokes
the confirmation and stops supervision without forcibly terminating the process.
Old, malformed, stale or differently correlated heartbeats cannot grant it.

Stalled frames, Windows Display 4101 and NVIDIA critical/error events stop the
diagnostic. Forced termination is allowed only after this exact process/request
has confirmed muted output. Otherwise the tool records the failure for operator
recovery. A normal exit/stop file ends supervision; its finite deadline never
triggers a delayed process kill. The runner must retain/check the log and exit
code and must not accept a run with a trip or missing mute confirmation.

This replaces the unsuitable pattern of killing every process with a matching
name and applying a hardcoded monitor profile after any NVIDIA event. It does
not repair a display driver, prevent every hang, or diagnose the owner's earlier
garbled-screen incident. Windows performs its own timeout/recovery sequence;
see [Microsoft's TDR documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/timeout-detection-and-recovery).
No driver/TDR settings are modified here.

`Test-StageWatchdog.ps1` checks identity/freshness/request parsing and real
disposable headless processes: an unconfirmed or revoked mute leaves the process
alive, while a stalled confirmed-muted child is terminated. It starts no game,
opens no device, changes no display, and disables event monitoring for fixtures.
No deliberate GPU reset/hang test is appropriate for validating this tool.

Consumers vendor both the watcher and `Stage-WatchdogPolicy.ps1` from one commit,
pin their hashes and retain the toolkit license. The consumer owns game startup,
request/output authority, saved-state restoration and the final evidence review.
