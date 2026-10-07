# Lessons from Horizon Chase's first force adapter

Source/fake-sink review of `dbce-mods-horizon-chase-turbo` f789188, including
0.3.0 c99bb9b. The installed FFB remains off by default; no native DLL or wheel
was opened during this review. Consumer details:
`docs/reviews/2026-10-07-ffb-review.md` in that repo. Its existing 48 mapping/model
checks pass while 23 independent production-path assertions reproduce delivery
defects and normal-gate controls. Pure arithmetic tests do not cover the sink.

## Pin the component, not just the package number

HCT vendors toolkit **release v0.9.0**, source e7d9a404, native x86 SHA-256
`CC1D413AAFC9A10E90F3D4FE41A813DB0D9AAB5D27FD42F04511798C67DF9D50`.
That package contains **native 0.4.0 / 400 / 37 exports**, not the later native
0.9.0 / 47-export implementation. Review the header and implementation at the
exact pinned source. Architecture, export count, DLL hash, package version and
native component version should all be in a candidate receipt.

The old preferred GUID is a preference: init may try other candidates after
failure. This contradicts a consumer promise of force on a particular reader.
Require strict target identity and refuse missing GUID/open failures instead of
quietly accepting fallback. Historical auto-candidate advice in
DIRECTINPUT-FFB.md does not authorize fallback from an explicit association.
An enumeration result is not confirmation of the device opened for force.

## Read-only observation and output permission are separate

An existing slot number does not mean input is fresh, enabled or from the
currently selected instance. HCT can keep sending curve/rumble forces when its
reader has stopped supplying steering. A force adapter needs a current valid
input/source identity and explicit delivery eligibility. Keep the model and
telemetry observable with the output muted, but gate the physical sink.

In this old pin, a read slot uses the force handle only if that handle already
exists for the same GUID when the slot opens. Starting FFB after a read-only
open does not migrate that slot. Init does not explicitly close it either.
[Microsoft's cooperative-level contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417921(v=vs.85))
permits nonexclusive access alongside exclusive access; this is not evidence
that two handles must conflict. Test ordering, identity changes and reacquire
against the actual pin and driver. Do not transplant a lifecycle conclusion
from a different toolkit version.

## Exercise the production delivery transitions with a fake sink

The HCT reproductions show why these cases belong next to model tests:

- Disable while driving without opening the settings panel first. HCT's panel
  already zeros, masking the missing direct-disable stop.
- Lose input validity after FFB initialization, then recover or replace the car.
  Reset force history and re-ramp across the relevant identity/enable epochs.
- Native write returns failure without throwing. Cache only accepted writes;
  preserve the failure in status instead of immediately reporting `on`.
- Zero throws or is rejected. Do not mark zero accepted and skip later stops.
  Attempt zero/stop independently with bounded recovery and an unresolved-stop
  diagnostic. Cover partial initialization too.
- Non-finite tuning/time/events reach the final command. Validate early and
  again before float-to-int conversion; clamp alone does not neutralize NaN.
  Runtime-specific conversion is not a safety contract.

Normal pause/menu/focus/finish gates can pass while these transitions fail.
100 ms keepalive against a 500 ms watchdog is nominal scheduling headroom;
watchdog polling and thread scheduling add latency. It does not substitute for
an immediate explicit stop on disable/error.

## Independent sliders do not prove equal effects

Art is the fixed reference at Strength 50; see
`profiles/normalization/art-of-rally-50-v1.json`. Its impact controls are
independent of steering Strength, but its crash push and rattle use separate
native finite effects. HCT combines steering, a directional crash and aggregate
pad rumble into a clamped constant command once per rendered frame. A 25 Hz
sampled sine aliases at low frame rates, including a constant phase at 25 fps.
Do not call these delivery paths equivalent because the duration/frequency
constants or slider defaults match.

HCT's rumble helper aggregates all haptic events, including collisions; it is
not a road-only source. Labelling it as road can conceal double-counted crash
energy and make crash-strength trials misleading. Record source provenance,
event-time strength, effect-family requests, overlap policy and final clipping.
If adopting native periodic effects, first qualify their stop/watchdog coverage;
the old pin's constant-only watchdog is not a general all-effect mute contract.

These findings do not change any toolkit binary or consumer gain. HCT still
needs an original signal capture, qualified replay and an owner comparison
before entering the normalization evidence set.

## Follow-up: 38b5220 and old native failure semantics

HCT's next candidate passes 71 model/mapping checks and 29 production fake-sink
checks. Normal disable/read-loss and finite-setting gates improve, but an
independent extended fixture reproduces remaining boundary failures (11
reproduction/control assertions, not safety passes). Consumer detail:
`docs/reviews/2026-10-07-ffb-fix-review.md`.

- **One device in a prior enumeration is not strict selection.** Native 0.4.0
  re-enumerates inside InitDirectInput and permits fallback. It also returns
  success for an existing native device before checking new preferences. A
  managed singleton check certifies neither that later snapshot nor that handle.
  Use the tested strict-selection native contract, including existing-handle
  identity and reader continuity, rather than adding more managed enumerations.
- **Void zero is best effort.** ZeroForces discards the integer setter result.
  A normal return cannot acknowledge neutral output. Fake native boundaries must
  model ordinary rejected HRESULTs that do not throw, including a void wrapper
  hiding them. Cache only acknowledged commands; preserve unresolved stopping.
- **Stopped is not zero-valued.** A successful StopEffect may retain nonzero
  parameters after a failed zero write. Starting before replacing those values
  can replay stale force ahead of the intended ramp. Qualify the sequence using
  persistent fake device parameters, not only the last integer sent. Pending
  stopping must inhibit Init/Start/nonzero output even after an explicit toggle.
- **Capture the setting at the callback.** A pure model can correctly latch its
  strength while the adapter feeds last Tick's cached value. Test config changes
  immediately before and after the actual callback; preserve the consumed value
  in the event record.
- **A successful getter is not a valid observation.** Check finite raw values
  and derived ratios, and label optional fallback input unavailable. Substituting
  zero must not silently create an eligible measured-zero row later.

The installed 17-file receipt and 264 rebuilt/installed method IL bodies were
checked offline. FFB remains default Off; no device was opened or force sent by
the reviewer. Recommended next step is qualified x86 strict selection plus the
consumer stop/restart fixes before attended output, not a hardware experiment
to discover which actuator the old fallback happened to choose.

## Native0.6 proposal and retained-handle follow-up

HCT4dc4331/4c95664 closes void-zero acknowledgement, unresolved-stop continuation,
partial-init cleanup and event-time crash strength; the bundled native0.4.0 is
now refused before force opens.37 author sink checks and72 model checks pass.
Four independent production-path assertions still reproduce retained-parameter
EnsureInit restart, existing-handle identity bypass and non-finite authority
hidden by a conditional zero. Fixing Send's restart branch alone missed the
separate initialization branch. Checking a quotient alone missed invalid raw
authority when the division was skipped.

The suggested OutRun x86 native0.6.0 is exact f8f0619,166400 bytes,
SHA4F7FFBB590C30E2A8D33F2E7A3D7E8C770BDD2466053A2333D67C55D98BB08B4.
Its strict flag only constrains opening a new candidate: InitDirectInput returns
success for an existing g_device before validating its GUID. Export presence
does not establish the full identity contract.23 exact-source x86 fake-COM checks
reproduce this and test reader/free/watchdog behavior; no system DirectInput or
hardware is opened. The lock-publication successor does not itself fix identity.

SetHoldTimeoutMs after successful initialization is not the documented pre-init
publication-race sequence: the lock is already initialized before worker
creation, and Free joins while retaining the lock. Keep that scoped statement
separate from general watchdog qualification. OutRun's no-watchdog adoption
cannot stand in for HCT's use; prefer the reviewed41-export publication successor
as a narrow candidate base, then qualify exact x86 and consumer lifecycle paths.

Reader-first slots stay independent; force-first same-GUID slots borrow the
force handle. FreeDirectInput preserves both slot records and independent read
handles; borrowed slots become unavailable until the same GUID returns.
CloseReadDevices is separate. Do not silently invalidate consumer slot caches.
Microsoft's [cooperative-level contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417921(v=vs.85))
permits nonexclusive reads alongside exclusive access; this is not proof of a
particular driver's coexistence behavior or a requirement to close all readers.

Full consumer review: docs/reviews/2026-10-07-strict-native-review.md. Native pin,
consumer runtime and physical settings unchanged by this review. No attended
force or Art50 normalization acceptance follows from the offline checks.
