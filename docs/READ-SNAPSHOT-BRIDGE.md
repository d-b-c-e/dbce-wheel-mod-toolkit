# Optional read snapshot bridge, source candidate 2026-10-11

## Port onto native-0.9.1 (2026-10-12, branch `mufa/native-091-read-snapshots`)

Romi 32354 chose this port for Art's shared input. Without it, a shared input package
would have to repin onto the diverged native line (`DD301314`).

- **Base:** exact `c8ec2ee`, the `native-0.9.1-fleet-candidate` that Art ships as
  `F8361A08`.
- **What is carried over:** only `ecc49bd` (snapshots) and `64ccf67` (interface path).
  - The three exports are `GetReadDeviceInfoV1`, `ReadDeviceSnapshotV1` and
    `GetReadDevicePathV1`.
  - The header, state and test are taken unchanged from `64ccf67`.
- **What is left out:** the baseline has no external FFB session and no test-injection
  table. Their guards are therefore absent from `read_snapshot_impl.h`. The
  `WHEEL_READ_INJECTION_ACTIVE` code stays defined but is never returned.
- **Changes to `WheelFfb.cpp`:** additions only, nothing removed.
  - A handle generation, incremented when `OpenCandidate` creates the force device.
  - Snapshot invalidation on read-slot open, on a failed legacy read, and on close.
  - The snapshot implementation is included before `CloseReadDevices`.
  - No force producer, init, send, effect or hold path changes.
- **Version and exports:** `GetWheelFfbVersion` stays 901. The export count goes from
  48 to 51 (`abi_lifecycle_test`).

The original candidate notes follow.

Based on toolkit `25195af1d839aec5bfadb29a819de49ad76be6b1`, isolated branch
`codex/art-shim-snapshots`. This is an additive, uninstalled API candidate for
sharing Art's existing WheelFfb input handle with shared Core. It changes no
force producer, settings, recorded signals, or installed payload.

`GetReadDeviceInfoV1(slot, WheelReadDeviceInfoV1*)` qualifies an already open
reader by actual instance/product GUID, individual object presence and actual
read-back ranges. It opens no device, acquires nothing and changes no property.
The generation covers the handle lifetime; even a reused COM pointer with the
same GUID after recreation gets a new generation. Call in idle preparation.

`ReadDeviceSnapshotV1(slot, WheelReadSnapshotV1*)` requires that qualified
generation. It performs one Poll and, only after successful Poll, one
GetDeviceState. It never retries/acquires or injects values. It returns raw
DirectInput button bits, separate HRESULT/classification, validity, monotonically
increasing source sequence and original QPC start/end in microseconds. Failed
reads invalidate qualification. The owning application must recover and prepare
a new generation; a copied provider must never reacquire on its behalf.

The structures are explicitly versioned 1, 144 and 232 bytes at pack 8 on x64
and x86. The caller initializes exact size and version. Original exports remain;
`GetWheelFfbVersion()` remains 1000 because no published release was replaced.
Optional managed bindings work on netstandard2.0 and return -4 when absent.
Existing native consumers do not need the new exports. A future release must
assign its release/version/provenance as usual before distributing this candidate.

Buffers are copied; no COM pointer or managed callback crosses the API. Existing
API/external-device locks and logical reentry guard apply. An AddRef holds the
borrowed object across reentrant cleanup; a changed slot/revision/generation
invalidates the returned sample. An active legacy injection workload refuses
this physical-only route. A callback-invalidated sample may have successful
call HRESULTs but validity false; callers must honor validity and refuse it.

Validation performed in the isolated worktree, with no real device/window:

- Snapshot fake-COM tests pass x64/x86: sparse metadata, actual GUID/product,
  ranges, original sequence/time, failure/requalification, same-pointer epoch,
  switched wheel, reentrant close, size/version errors and zero CreateDevice,
  Acquire, SetProperty or force calls.
- Actual candidate DLL builds x64 and resolves all 61 exports with unchanged
  version; 32 empty lifecycle unload cycles pass without enumeration/initialization.
- Existing POV/acquisition fake suite passes; retained identity 37 checks,
  external session 871 checks and existing injection/race suite pass.
- Managed binding builds netstandard2.0 with zero warnings/errors.

Run `native/wheelffb/test/build_read_snapshot_test.bat x64` and `x86` for the
new fake suite. The existing lifecycle/POV/retained-identity/external-session/
injection build scripts supply the regression checks. No hardware smoke was run.

Shared shim and Art integration live in their isolated branches. The shim
copied provider owns only memory; Core remains the sole profile parser and
resolver. Its bridge API refuses capture/control flags until original source
generation, sequence and timing have a versioned recording contract. Runtime
adoption still needs reviewed candidate pins, Art's identity-based Rewired
isolation, explicit profile activation, lifecycle/rearm policy and serialized
muted then attended rig qualification. Accepted Art v2 settings/recordings remain
the baseline.

## Optional owned interface path (2026-10-11 follow-on)

`GetReadDevicePathV1(slot, expectedGeneration, uint16_t* path, capacity)` is an
additive export. It reads `DIPROP_GUIDANDPATH` on the already-qualified slot's
actual device, retains an AddRef during the call, checks actual instance/product
and source generation/revision afterward, and clears the output on failure.
It never polls, reads state, acquires, changes properties, opens a device, or
calls a force API. Capacity counts UTF-16 code units including NUL; 260 fits the
native property. No truncation. An empty/unterminated path or a path change within
the same source generation refuses; a changed path ends that generation.

DirectInput and Rewired RawInput instance GUIDs can differ for the same device.
Consumers may compare this **exact owned interface path plus product GUID** with
Rewired's public RawInput extension, retaining both GUID namespaces. A name,
product alone, array position, or shortened collection path is not that proof.
An unavailable path must remain unqualified. The managed wrapper exposes this
optional operation without requiring it from old DLLs.

Both V1 structs remain 144/232 bytes, native version remains 1000, and the x64
candidate keeps all old exports (62 total after this addition). The extended fake
COM suite passes x64/x86: successful path, wrong generation, undersized buffer,
failed/unterminated property, changed path, reentrant close, and zero device/force
acquisition. Native and managed builds pass. No hardware path was queried.
