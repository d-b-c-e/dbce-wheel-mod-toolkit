# Art of rally adoption support (0.12.0)

The managed `AxleForceCurve` compatibility version 1 moves the consumer's released
axle-force formula into the toolkit. It preserves gain, Unity-style speed fade,
hard clipping and subsequent EMA arithmetic (including smoothing 1). Positive
lateral force produces positive output unless inverted. User gain is applied once.
Existing `ForceModel`, `ForceShaper`, and `simlite@2` profiles are unaffected by this
compatibility API. It is deliberately independent of Unity; game signal extraction
and the previous-output state remain the consumer's responsibility.

`WheelFfbNative.Load(directory, fileName)` binds Cdecl exports from the exact loaded
handle, supporting `UnityForceFeedback.dll` on both old and modern managed runtimes.
Successful handles stay pinned for process lifetime; a different path is rejected.
Original three-argument Initialise/Reinitialise signatures remain binary-compatible.
The four-argument overload adds an explicit nullable GUID; null clears selection.
`PreferGuid` remains available as a next-initialisation preference for callers of the original overload. Strict GUID
selection requires new native 0.5 `SetStrictDeviceSelection`; missing devices and
failed selected candidates cannot fall back to another device. Older native 0.4
continues to support legacy selection, but rejects the strict GUID request.

`Shutdown` releases force resources while retaining independent readers. Final
consumer exit calls `ShutdownAll` to close input/aux readers and free an enumeration-
only DirectInput instance. Panic clears managed readiness; a new native lifecycle
resets panic state and re-enables actuators before any new force. Shared FFB read
slots retain their original GUID and refuse to follow a different FFB wheel.

Sync validates all requested parts/conflicts first, stages a complete replacement
on the same volume, and rolls back a failed commit rename. Local edits require
`-Force` and retain backups; unselected files and manifest entries are preserved.
Local builds are labeled `local+<full revision>.clean|dirty`, never as releases.
The manifest records file integrity; it does not authenticate release provenance.
An abrupt process termination between renames can leave the original in the
`.toolkit-transaction-*/previous` recovery directory. A caught rollback failure
retains that directory and reports its path rather than deleting evidence.

Validation entry points: `tools/tests/Test-SyncToolkit.ps1` (24 assertions, real
filesystem including injected commit failure); `tools/tests/Test-NativeBinding.ps1`
(40 assertions each on x64 CoreCLR and x86 .NET Framework, fake native DLL, no device
access); 640-row axle vector plus clipping/filter tests in the managed suite.
The art-of-sim-rally consumer independently compares 100,000 dynamic steps with its
frozen original formula using the installed game's actual managed Mathf helpers.

Native 0.5 builds retain all old exports and add one (38 total). The no-force smoke
must run after native changes. Its damper probe previously applied nonzero damping
even without `--force`; this change gates that operation on the flag. Zero-output
hardware checks cannot establish force feel or Unity/Mono runtime compatibility.
Attended consumer testing remains required.

Historical validation of the adoption-only commit de5e6d5 on 2026-09-07: 111 managed tests passed
(70 FFB, 41 telemetry); fake-native checks passed on both runtimes; x64 and x86
native builds succeeded. The x86 no-force smoke passed on the owner's MOZA R12,
including missing strict GUID refusal, zero constant/periodic/condition output,
and panic/reinitialisation recovery. No nonzero test force was requested.
This is device lifecycle evidence, not a gameplay or force-feel test.

The combined release adds further model/shaper recovery and consumer diagnostics.
The fake binding harness exercises CoreCLR and Microsoft .NET Framework 4.8;
.NET Framework is not Mono. An installed Unity 2019 Mono game run remains an
explicit consumer validation step. The export binding avoids the unsupported
alias-search assumption, but these offline checks do not claim a Mono game run.

Combined v0.12.0 packaging passed with 121 managed tests, 24 sync assertions and
40 binding assertions on each tested runtime. See STATE.md for the full gate list.
