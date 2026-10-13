# Managed shim ABI 2

`Dbce.Wheel.Shim` is the game-neutral managed side of
`native/shim/shim_dll/dbce_shim.h`. It supplies the 31-action table, status and
frame decoding, exact `[Controls]` byte extraction, Core hash verification,
steering binding text after Core acceptance, and an x64 full-path,
consumer-hash-pinned native export loader. It targets .NET 6
for BepInEx IL2CPP consumers and has no game, Unity or loader dependency.
The extraction follows Woden's `WodenRallyEdge.Core/ShimActionMap.cs` and
`ShimProfile.cs`, and `WodenRallyEdge.Plugin/ShimNative.cs` at game commit
`0565f392`; Woden remains unchanged and keeps its pinned production copy until
a separate consumer adoption is reviewed.
Art of Rally's copied-provider bridge uses a separate ABI variant and is not
covered by this loader.

The consumer supplies its reviewed DLL hash and verifies its installed receipt,
game build, device isolation and keyboard-only native residual before admitting
shared input. It owns the game-input thread for every handle call, and it must
check `Tick`'s result before decoding the returned frame and stop on that thread.
A resolved or unresolved native stop spends the handle;
the module deliberately remains loaded until process exit because an unresolved
worker may still need its code. Game-specific action delivery and FFB selection
stay in the consumer repo.

`Open2` mirrors the low-level developer capture/latched ABI. The caller owns
every pointer in `ShimOpenArgs` through the call and must independently enforce
its muted-force and run fences. This library does not arm those developer modes.

The tests compare the action names and constants with the canonical native
headers, check the 128-byte argument layout and 296-byte frame offsets, exercise
profile byte/hash handling and changed-export refusals, and prove a bad DLL pin
is rejected before loading. They do not open a device or qualify a game adapter.
