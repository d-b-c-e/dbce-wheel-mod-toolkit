# Managed developer test injection

`dotnet/Dbce.Wheel.Input/NativeTestInjectionClient.cs` is source-vendorable beside
`PovSnapshotReader`. It targets the additive developer ABI proposed on October 10:
EnableTestInjection, SubmitTestInjection, GetTestInjectionStatus and
LastReadInjected. It has no engine dependency. The native implementation and
consumer installation are separate qualification gates; this is not a release.

The client resolves exports from a caller-supplied **resident** module. The caller
must verify the module's complete path and hash and that this is the module
owning its OpenReadDevice/ReadDeviceState slots. It must retain the module and
serialize calls with that reader's lifecycle. Do not resolve by basename or load
another copy to obtain test exports. Art has two installed native copies: a
fence in one module cannot certify the other. Retain consumer force/telemetry
output muting and prohibit any unverified force provider.

Only a fresh, bounded developer request may call Arm, before any force opening.
It sends exactly flags=1, then requires an armed status from native code. Arming
does not open any device. After any arm attempt, BlockAssignments stays true:
the consumer must refuse binding assignment and calibration capture until exit,
including when arming fails or its result is uncertain. This property concerns
this client only; it is not discovery of another caller's native test session.
Missing/partial exports never fall back to a managed sample override. Any
exception, malformed native response or loss of the no-force confirmation
ends this client's submission path; restart the process to retry.

SubmitRaw accepts one `inject raw ...` command, at most 512 printable ASCII
bytes, matching native candidate 6d90b45. It passes those bytes unchanged and NUL-terminated;
the native shared parser owns grammar, device identity, range and duration.
No current binding is inverted to calculate a test sample. The harness must
derive raw stimuli and expected actions independently from the original profile
so a wrong written axis or polarity cannot cancel itself out in the test.

Submission acceptance, LastReadInjected immediately after that same slot's read,
the normalized binding value, and the actual game action/response are distinct
evidence. RefreshStatus records native state, not physical force measurements.
No stop/unload/device cleanup is hidden here: the owning test runner bounds its
duration, observes native expiry, and closes readers/exits under the normal
consumer lifecycle. The irreversible native no-force latch remains through it.

Run these fake-delegate checks without a module, device or game:

```powershell
dotnet run --project tests/NativeTestInjection -c Release -f net8.0
dotnet build tests/NativeTestInjection -c Release -f net48
./tests/NativeTestInjection/bin/Release/net48/NativeTestInjection.exe
```

Both runtimes pass 79 checks: partial ABI, exact flags and bytes, unarmed calls,
sticky failure/assignment guard, acceptance versus delivery, native refusal,
invalid return values, exceptions, unterminated and malformed UTF-8 responses,
command bounds and invalid slot checks. These do not qualify the native no-force
implementation or an installed consumer.
