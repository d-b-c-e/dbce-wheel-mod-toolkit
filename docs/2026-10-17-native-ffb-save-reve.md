# Native FFB save consistency

Mivu239995 assigns the canonical native layer, its existing fake fixture and exact
Stop-feedback lines. Isolated successor:
`E:/Source/toolkits/.worktrees/reve-native-ffb-save-20261017`, branch
`reve/native-ffb-save-20261017`, base `e4d058afda5979e183ea07535aefaaebcd917f06`.
Implementation `10d6c63`; focused fixture follow-up `b92a3dd`.

Enable and Strength previously changed live state before knowing whether the
Win32 profile write succeeded. Enable now saves before resuming effects; Strength
saves before changing the reported value or reapplying the gain. Failed writes
leave their last-confirmed live state intact. A null INI remains explicitly
memory-only and applies changes immediately without claiming persistence.

Off exchanges the live flag and stops effects/STOPALL before attempting the
write. A failure never rolls back to enabled, and reports
`FFB stopped; Off could not be saved`. F8's generic feedback is set before its
callback, allowing the callback's save result to take precedence. Header Stop
already calls the same callback directly. Public function signatures, tuning,
gain calculation, effect iterations, mute, input, focus and device guards stay
unchanged.

Caller read: Rush Rally 3 `src/proxy.cpp:444` and V-Rally 4 `src/proxy.cpp:1014`
both invoke `dbce::ffb::SetOn(false)` from their Stop callback. Those frozen vendor
copies and packages belong to Mubi and are untouched. Future adoption needs
the layer CPP/header plus the narrow shared-panel F8 ordering change; it does
not require blanket vendoring or new configuration keys.

Qualification PASS: existing `test_layer.cpp`, x64 and x86 MSVC `/W4 /WX`,
128 checks per architecture (256 total). Both product compiles have no warnings;
the existing recipe compiles third-party ImGui separately at `/W0`.
New checks exercise successful and failed real private INI writes through the
production handlers, recording fake effect starts/stops/gain, memory-only
behavior, post-failure game Start/autocenter blocking, retry, clamping and mute.
CPU-only ImGui text emission checks that the actual F8 path retains the error.
This creates no renderer, window, GPU context or real DirectInput device.

Kobe request240124 was queued (ack240154), then granted by240300 after Mubi's
release. Exact hub runner `artifacts/native-ffb-save-reve-20261017/Run-01.ps1`
ran against clean docs HEAD `d900cb6058a0a80c3c29d4907067f8ee2ff64609`.
Lease entry 2026-10-17T23:34:09.017Z; actual release 23:34:31.548Z, 22.5 seconds
within the four-minute build/cleanup cap. Existing owned-process census reports
12 tracked identities, no remaining scoped children, query safe, quiescent, exact
lease released and literal absence. Both fixture runs verified private INI/temp
directory removal. Compiler environments were confined to exited child command
processes; no global environment change or owner files needed restoration.

Evidence: hub `artifacts/native-ffb-save-reve-20261017/run-01/result.json` and
`layer.stdout.log` (both PASS markers), empty `layer.stderr.log`. No game, UI,
input, real device, owner install or runtime qualification was performed. Actual
rendered UI/physical force and downstream adoption remain separate gates.
No current process/lease/restore custody. Frozen Mubi candidates remain untouched.
