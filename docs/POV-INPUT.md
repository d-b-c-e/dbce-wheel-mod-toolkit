# Coherent wheel input with POV hats

Local candidate `0.16.0-pov.1`, native component **0.9.0** (`900`), based on
the exact `v0.15.0` runtime source. This is not an official release or a claim
that Sonic Racing: CrossWorlds accepts a particular input/FFB integration.

## Contract

```c
int ReadDeviceStateWithPov(int slot, int* axes, int axisCount,
    unsigned char* buttons, int buttonCount, int* pov, int povCount);
```

- `slot` comes from `OpenReadDevice`; valid until `CloseReadDevices`.
- `axes` is required, `axisCount` must be exactly **8**, in order
  X, Y, Z, Rx, Ry, Rz, Slider1, Slider2. Values retain the device's axis range;
  independent readers request 0..65535, but the driver may reject that range.
- `pov` is required, `povCount` must be exactly **4**. Values are clockwise
  hundredths of a degree: north 0, northeast 4500, east 9000, southeast 13500,
  south 18000, southwest 22500, west 27000, northwest 31500. Intermediate
  angles are retained; the consumer owns directional quantization.
- `-1` means centered, absent, or invalid. DWORD `0xFFFFFFFF`, any value whose
  low word is `0xFFFF` (including `0x0000FFFF`), and values above 35999 normalize
  to `-1`. An invalid **angle alone** does not fail the snapshot; other hats,
  axes and buttons remain usable. Unwritten POV fields start neutral, not north.
- `buttonCount` is **0..128**; `buttons` may be null only when count is zero.
  Bytes are 0 or 1, using DirectInput's high pressed bit. A shorter buffer
  gets only the requested leading buttons; no writes occur past its count.
- Pointers must describe writable, non-overlapping buffers of their declared
  extents. On entry, independently valid buffers reset: axes/buttons to zero,
  POV to -1. Invalid counts do not authorize arbitrary writes; that particular
  invalid buffer remains untouched. Other valid buffers still reset if the
  slot, another argument, or the device read fails.
- Return **1** means one successful `GetDeviceState` supplied the entire
  snapshot. Return **0** means no usable snapshot. A driver that writes data
  then returns failure cannot leak that data into the outputs.

**Gate all mapped output on success.** Zero axes on failure are invalid data
and can represent a fully pressed pedal after calibration. On failure, release
virtual buttons/hat actions, set mapped pedals to their logical released state,
and suspend force output. Do not calibrate failed zero axes or retain output.

## One owner and one snapshot

The export uses the existing reader slot and DirectInput instance. When
`OpenReadDevice` is called for the active FFB wheel, it reuses its exclusive
handle. Failed `Poll` or `GetDeviceState` never causes the reader to acquire
that handle or alter effects. As with the original reader, a failed `Poll`
can still be followed by a successful state read. Independent non-FFB slots
retain the existing bounded acquisition retry behavior and still perform only
one `GetDeviceState` per call. The old `ReadDeviceState` export is unchanged,
including its historical output-buffer behavior on failure.

Serialize enumeration, open/read/close and FFB lifecycle calls on the consumer's
device-owner thread. This addition is not a thread-safety retrofit. Start FFB
before opening its read slot. If an input-only reader is already open and the
user enables FFB for that device, release virtual output, close readers,
initialize FFB, then reopen readers by saved GUIDs. Resume mapped output only
after a valid snapshot. Do not keep a second DirectInput owner for the same
wheel and assume two-process acquisition establishes compatibility.

The reverse transition matters too: disabling/shutting down FFB or changing its
device can release the handle that a shared reader uses. Neutralize virtual
output, serialize the lifecycle transition, close/reopen affected readers by
GUID (as independent readers when FFB is off), and resume only after valid
samples. An `FFB: Off` toggle must not leave driving input tied to a released
FFB handle. This is caller responsibility, not automatic reader migration.

A shared read slot is tied to its saved GUID. Switching the active FFB wheel
returns failure for the old slot; it does not silently read the newly selected
wheel. Restoring the original GUID can resume the slot. The consumer owns
explicit selection, missing-device recovery, and excluding its own virtual
controller from physical input choices.

## Managed adoption

`WheelFfbNative.SupportsPovRead` reports whether the loaded native module has
the additive export. `Read(int slot, int[] axes8, byte[] buttons, int[] pov4)`
is the coherent overload. Arrays must be exactly eight axes and four POVs;
buttons may be null or length 0..128. It clears all supplied managed arrays
even on invalid arguments, unavailable capability, failed reads, or exceptions.

```csharp
var axes = new int[8];
var buttons = new byte[128];
var pov = new int[4];
if (!WheelFfbNative.SupportsPovRead)
    ShowMissingPovCapability(); // Explain the required native component.
else if (!WheelFfbNative.Read(slot, axes, buttons, pov))
    ReleaseMappedControlsAndSuspendFfb(); // Logical pedal release, not raw zero.
else
    MapOneValidSnapshot(axes, buttons, pov);
```

Older native 0.8.0 and earlier supported modules load without this export.
The new overload fails explicitly; it never substitutes the old read or
combines two frames. Existing APIs remain available. Managed binding owns one
exact native module for process lifetime: `Load` refuses replacement by another
path, preserving the loaded module/capability; there is no live DLL-swap API.
Native consumers may check `GetProcAddress` or the new header member. The C
header loader retains its partial-result behavior when an export is missing;
callers must check the particular functions they require.

Hat-to-right-stick mapping, release timing, binding UI, calibration and virtual
controller output belong to the game consumer. POV input is not car-state
telemetry and does not itself provide meaningful vehicle signals for FFB.

## Verification and hardware gate

`native/wheelffb/test/build_pov_read_test.bat [x86]` executes production exports
against fake COM devices: eight directions across four hats, neutral encodings,
invalid angles, unwritten hats, bounds/canaries, omitted/short buttons, partial
failed driver writes, one state call, shared FFB handle without acquisition,
GUID changes, closed slots, independent retries and legacy failure behavior.
The package script runs it on x64 and x86.

`tools/tests/Test-NativeBinding.ps1` covers exact-module Cdecl marshaling on
x64 CoreCLR and x86 .NET Framework with current, 0.8.0, 0.7.0, 0.6.1 and 0.5.0
fake modules. It checks capability absence, valid snapshots, no split reads,
invalid arguments before native entry, poisoned failures and recovery. These
are not Unity Mono or game tests. The package helper's device inventory only
enumerates identities; it does not acquire/read a physical controller.

The C smoke now reads hats through the shared FFB-wheel slot and checks invalid
slot clearing without requesting torque. **Hardware execution remains NOT RUN**:
the standing desktop pause covers its foreground window and device acquisition.
Compile the harness, then arrange an attended slot before running it. Physical
hat directions, disconnect/reconnect, game input coexistence and real FFB remain
consumer acceptance work.

See `docs/STATE.md` for completed offline gates and `dist/POV-HANDOFF.json` in
this worktree for the local artifact's exact commit, dirty state and hashes.
