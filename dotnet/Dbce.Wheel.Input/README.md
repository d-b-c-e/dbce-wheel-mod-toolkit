# Optional POV reader for existing managed pins

`PovSnapshotReader.cs` is MIT source that can be vendored when a consumer keeps
an older managed wrapper but its existing native module provides
`ReadDeviceStateWithPov` (native 0.9.0+). This is not a native DLL update.

The consumer supplies the export from the exact already-loaded module and a
slot owned by its normal input lifecycle. The helper never loads/unloads a
module or opens/acquires/releases a device. Serialize it with that owner.
Clear the binding before unloading its module. An absent export is a separate
capability decision; a failed coherent read must not retry through the legacy
reader. Gate all mapped values on success; raw zero is not pedal release.

Tests: `dotnet run --project tests/PovReader -c Release` (fake delegate only).
They cover coherent counts/data, absent export, rejected/partial/throwing reads,
invalid arrays/slot and cleared outputs. They send no hardware input or force.
The runtime ABI and lifecycle requirements remain in `docs/POV-INPUT.md`.
# Shared steering identity for FFB targets

`ShimSteering.cs` is MIT source to vendor beside `ControlBinding.cs` (netstandard and Unity Mono consumers). It
returns the steering binding (device instance GUID, product GUID, display name, ...) from a shared `[Controls]`
section only after native Core accepted those exact bytes (`dbce_shim_profile_info`: `ok=1` and the section's
SHA-256). It reads the single `steer` line with `ControlBinding`, the only grammar. Product policy is the consumer's:
an empty product GUID is returned as written (DRIVE accepts it; iRacing's adapter refuses it). Several steer lines, a value that does not fit steer, no device, or bytes that are
not UTF-8 give no identity and a reason; they never throw into the caller's input open. A wrong Core report throws
`InvalidDataException`. It opens no device and sends no force. The consumer's route policy (explicit selection,
pending, owned, stopped, actual refusal) is in `knowledge/SHIM-IDENTITY-AND-NATIVE-ACTIONS-2026-10-11.md`.

Tests: `dotnet run --project dotnet/Dbce.Wheel.Input.Tests -c Release` (bytes and reports only).

# Value shaping for declared extras

`ExtraControlValue.cs` is MIT source to vendor beside `ControlBinding.cs`. It holds the arithmetic native Core applies
to a game's declared extras (native/shim extension 2, `dbce_shim_extras.h`) after its status checks, for managed
previews in Wheelkit, the setup helper and F6.
- `Pair(neg, pos)`: a centred component from two digital halves. Each half is `ControlBinding.Normalize(ControlShape.Digital, raw)`. The result is pos minus neg, an unbound half passes 0, and opposite halves together give 0.
- `Stick(x, y, StickRange)`: each component is clamped to -1..1. `Circle` scales any vector longer than 1 back to
  length 1; `Square` keeps both components. +X is right and +Y up.
- `TryParseRange` accepts the declaration's words, exactly `circle` or `square`.

It reads no device and knows no context or status. Unit neutrality (a configured part that is not Live leaves the whole
destination neutral) stays in native Core. Non-finite input throws, and it is plain C# with no tuples, so netstandard
and Unity Mono consumers can vendor it. The tests in `Dbce.Wheel.Input.Tests` match native `test_extras`.
