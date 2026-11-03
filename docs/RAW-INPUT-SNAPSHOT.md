# Optional raw input snapshot store

November 2: merged and qualified with fake-device CPU checks. See
[the reconciliation checkpoint](2026-11-02-raw-snapshot-reconciliation.md).
The source-only/UNRUN statements below describe the original increment.

Source-only increment for PM458555/458852, based on toolkit
`b8362d7f07538b814abc47a098b4526541c2ea7e`. No build, fixture, game,
device, lease, package or installation was performed for this increment.

`native/dinput-proxy/dbce_raw_snapshot.hpp` owns bounded value copies from
ordinary and fenced game-owned `GetDeviceState` calls. Raw means before DBCE
injection, shaping and panel suppression; DirectInput's format and properties
already apply. The fenced legacy observer still sees the final transformed data.

Set `ffb::Options::snapshots` before device creation, or
`fence::Settings::snapshots` before wrapper creation. Keep the store alive until
every associated call has ended; ordinary Options must remain fixed during use.
Null preserves existing behavior. No reference, acquisition or polling is added.
The ordinary route registers only successful hooked `CreateDevice` results;
direct `HookDevice` alone does not admit a snapshot identity.

The game adapter owns `BeginInvocation`, `Seal` and `EndInvocation`. Reserve
each read before forwarding and copy only successful exact standard joystick
layouts immediately on return. `ReadUnique` returns an owned copy only for the
current sealed invocation, owner thread, reported instance, descriptor and latest
read serial. A read failure or lifecycle mutation cannot revive a prior success.
Observation age uses monotonic milliseconds from read start; late completion
does not refresh it. Nested invocation entry poisons admission until outer End.

The store verifies the reported instance separately from the requested GUID.
Object generations are nonzero 64-bit values, monotonic across stores and
ordinary/fenced translation units within the containing module, and never wrap.
Addresses identify records only. Duplicate live GUIDs or any unidentified live
record refuse admission. An unqualified ordinary QI alias or failed required
hook admission disables the store. The bounds are 64 live records and eight
overlapping mutations per record; exhaustion disables admission.

Acquire, Unacquire, Poll, property/cooperative-level changes, format/action-map,
and every Release invalidate and hold mutation occupancy across the
forwarded call. Ordinary Initialize disables the optional store before forwarding
the original call, including failure; it cannot replace an entry during another
outstanding mutation. Poll/Acquire success is not a sample. Failed format calls preserve
legacy transformation layout while raw layout becomes unknown. Final Release
retires only its saved generation, including same-address reuse during Release.
Fenced Initialize remains refused, with raw descriptor invalidation.

`test_raw_snapshot.inc` is included by the existing layer/fence test targets.
New fake source covers standard prefixes, owned copies, age/clock rejection,
out-of-order and failed reads, mutation reentry, foreign/late completion,
invocation occupancy, duplicate/unknown identities, generation/address reuse,
property/cooperative changes, hook capacity/alias refusal and pre-transform raw
versus legacy transformed values. Existing null-option cases remain. The existing
`build_ffb_layer.ps1` already builds/runs those targets for x86 and x64; it was
not invoked. All new assertions remain unrun.

Next: bounded source review of the frozen commit, then separately authorized
CPU qualification. Native game invocation/calibration admission, emitted ABI,
vendoring, multi-span patch quiescence, installation, runtime and physical
acceptance remain outside this increment. No consumer is activated here.
