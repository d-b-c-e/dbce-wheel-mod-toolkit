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
