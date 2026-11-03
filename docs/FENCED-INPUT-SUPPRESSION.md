# Panel input suppression in the closed DirectInput wrapper

October 27, 2026. Candidate based on `e4d84fb`; registered checks PASS on
`09cc89f` (production identical to `566b2d2`). No installation or game run.

One leased `build_ffb_layer.ps1` batch first failed two A/W fixture admissions
because a test pedal binding omitted the required product GUID. Test-only
`09cc89f` adds that captured identity; the original failure remains retained.
The second registered batch passes 20 gain checks, 322 actual A/W force-fence
checks on each of x86/x64, 156 production-hook checks on each architecture,
and the existing x86 layer/panel compile. No warnings. Safe child census is
empty; lease `c534188d011f441ead44a883601c8309` released after completion.
Evidence: hub `artifacts/revo-input-neutralizer-dume-20261027/`, logs
`toolkit-01.log` and `toolkit-02.log`, cleanup JSON and lease/release receipts.

`fence::Settings::panelOpen` is an opt-in process-long predicate, configured
before DirectInput creation. Null preserves the old forwarding behavior. The
predicate runs on the reading thread; exceptions are contained, logged with the
existing bounded logger, and treated as open for input suppression. This adds
no panel dependency, real-device call, writer or force route to the fence.

The input-only `dbce_input_suppression.hpp` is shared with the ordinary FFB
hooks. Successful non-null immediate reads are neutralized **after** injection
and pedal shaping, before the const state observer. Both exact standard
joystick formats retain every axis/secondary field and clear only buttons and
POVs. Keyboard and mouse require a recognized object layout, not just byte
count. Custom, mismatched, unknown and failed reads remain unchanged. Injection
and shaping still accept only exact joystick format IDs 1/2. Successful action
maps invalidate tracked layouts; failed format/map changes retain them.

Buffered reads first call the real handler with the original arguments. While
open, successful reads report zero events, including PEEK, count-only and
DI_BUFFEROVERFLOW. HRESULT, array bytes and the underlying read's consumption
semantics are preserved. In particular, PEEK is never converted into a drain.
Null/invalid arguments and errors remain the real handler's responsibility.
These semantics follow [Microsoft's GetDeviceData contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417894(v=vs.85)).

Limits: buffered axis events are hidden too; data remains in the caller's array
beyond its reported zero count. PEEK-only/no-read intervals may leave queued
events for closing, and held physical controls return in closed immediate
samples. This is input suppression while open, not queue flushing or key-release
latching. Unknown layouts and untracked normal-layer devices (existing table
capacity) cannot claim suppression. Attended qualification starts at a stationary
menu, with released controls before closing; gameplay/alternate input APIs and
clean post-close delivery require actual-consumer evidence.

The full force fence stays closed: software effects, force-command/property
refusals, interface/aggregation restrictions and class-factory refusal in the
consumer. Do not enable FFB Init/HookDirectInput to use this feature. Ordinary
force-layer persistence fixes in the pinned base are retained unchanged.

Registered verification: `native/dinput-proxy/build_ffb_layer.ps1` exercises
actual production wrappers for A and W and actual patched hooks on x86/x64,
plus existing force-gain/persistence/compile checks. New assertions include
keyboard/mouse/joystick canaries, custom layouts, failed and successful format
transitions, injected buttons/POVs and shaped axes before the observer,
predicate/observer exceptions, PEEK/count-only/overflow/legacy strides, and
unchanged force-call witnesses. No real device/window is opened by these
fixtures. Build intervals still require the shared rig/heavy-build lease.

Revo adoption must copy only the qualified fence/helper files, wire the predicate
while fenced before DirectInput is answered, update VENDORED provenance and run
the existing actual-handler/x86 export/load checks. No blanket vendor refresh.
This document does not establish rendered interaction, device/force behavior or
owner acceptance.
