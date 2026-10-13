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

## The no-force build (internal test deployments only)

Arming at runtime only protects what happens after the arm call. A consumer that
loads WheelFfb.dll and opens force before the harness arms (Woden's ordinary
companion, October 13) gets force. For automation, use the no-force build
instead: `native/wheelffb/build.ps1 -NoForce [-Arch x86]` compiles the same
source with `WHEELFFB_NO_FORCE_BUILD` into `build\noforce[\x86]\WheelFfb.dll`.

- The no-force latch is constant-initialized, so it holds from process load,
  with no call at all. Every force export refuses before touching a device, an
  effect or a hook, and InstallExitGuards and the hold watchdog stay off.
- Reads and test injection work from the first call. EnableTestInjection(1)
  returns 1 (already armed), so this client's Arm succeeds unchanged.
- The exports and version are the product's. GetTestInjectionStatus reads
  `armed: no force (no-force build); ...` from the first call, which identifies
  the running module. The file carries the string
  `DBCE-WHEELFFB-NO-FORCE-TEST-BUILD`. The link is deterministic (`/Brepro`), and
  `WheelFfb.noforce.provenance.json` beside the DLL records its SHA-256, the
  commit, a dirty flag, the toolchain and flags, and the SHA-256 of every source
  it was compiled from.
- It is for internal, hash-pinned, temporary test deployments that put the
  product back afterwards, such as a test package that a runner installs and
  then restores. It is never a release, a product pin or a lasting install.
  build.ps1 refuses `-CopyTo` with `-NoForce`, so the runner that owns the
  deployment copies it itself. A product build fails if it ever carries the
  marker.

Qualify it with `native/wheelffb/test/Test-NoForceBuild.ps1 [-Arch x86]`. That
runs both builds, checks that their export names are identical, runs the
fake-device injection scenarios plus the no-force harness, and makes the actual
DLL's first calls through `wheelffb.h`.

Recorded follow-up, not changed here: in the product build's runtime test mode,
InstallExitGuards still subclasses the game window after arming, although the
header says armed mode touches no hook. It installs no force, since PanicStop
and ZeroForces find no device. The no-force build refuses it.
