# Shim identity and complete native actions

Sources: DRIVE Rally `e78feaf`, Art of Rally `3d18923`, native shared shim
`f49cfc5` / copied provider `574bb88`, wheel toolkit `64ccf67`, October 11, 2026.
These revisions are review candidates; check consumer receipts before assuming
that a user's installed package includes them.

## Device identifiers have namespaces

The same R12 interface has a DirectInput instance GUID beginning `d71b8350` and
a Rewired RawInput GUID beginning `9d03e6aa`. Comparing them directly found no
controller. DRIVE then treated that empty set as clean and could merge native
wheel input with shim input. Latched injected tests suppressed native composition
unconditionally, so their success did not cover this ordinary-play defect.

Obtain the exact interface path and product identity from the **owned input
handle**, tied to its adopted generation/epoch. Match it to the game backend's
live path and product, require uniqueness, and retain both namespaces in the
diagnostic report. Names, array order and product-only selection are insufficient.
Never acquire another handle solely to join identities. Reconnect or changed
metadata invalidates the old join; missing metadata is not a successful empty
join.

Suppress only the matched native controllers, remember their original flags,
and verify the game's cache has drained before merging native keyboard input.
Restoring a flag means restoring its captured value, not always setting true.
Treat controller replacement as a new object even if the displayed name is the
same. A disconnected configured device may be proved absent only from a complete
known inventory; product can prove absence but cannot select between endpoints.
An unknown backend/product prevents that proof.

The native startup report can precede the worker's first open. Label it initial
or pending and report successful adopted identities separately. A startup
`device-missing` row is not evidence of a persistent failure.

Art shares the existing toolkit reader through copied immutable snapshots.
Its additive path query validates the expected generation before and after
reading the existing handle. Publish all metadata and paths atomically before
publishing samples. Optional-export absence refuses the experiment; fixed ABI
layouts and the accepted force producer remain unchanged.

### Disabled controllers can hide their public identity

The first DRIVE live identity run (`identity.1`, `live-01`) joined both physical
devices, then lost the join immediately after disabling their Rewired controllers.
The shipped RawInput extension deliberately returns empty path/product/handle
properties when its controller is disabled. The original fake extension kept
returning them; its passing test therefore missed the real backend contract.

Capture the exact live path/product join **before** disabling. While disabled,
retain that identity only as an explicit owned witness: same controller object,
extension object, persistent GUID, connected state and connection-event epoch,
plus the same owned native handle's metadata/generation/path. A disconnect and
reconnect within one render frame must invalidate it too. An unrelated or
externally disabled controller cannot borrow this witness. Exposed partial or
contradictory identity still fails; do not hide it behind the cache. Invalidate
native-clean status before restoring and making a fresh enabled join.

Keep raw diagnostic fields empty when that is what the backend returned, and
label the separately retained identity's provenance. After restoring flags,
require a fresh live path/product join. Fixtures must model disabled getters,
object/extension replacement and connection events. Art's source candidate
`9f90df9` implements this bounded probe contract; DRIVE's matching correction
is in progress. Neither is live-qualified by this note.

## Native event acceptance is not complete UI behavior

The first DRIVE full-session cold replay reproduced its menu/car/region choices
and 2,446 poses, then stopped progressing after pause. Raising native `GoBack`
hid the popup but left simulation paused. The real Resume button also restores
gameplay mode, time scale, preview visibility, subtitles and lighting.

Record and replay the complete user-facing native action when its callback owns
those side effects. Invoke an adapter-owned, pinned native button/handler and
observe the resulting state. Do not relax readiness checks or patch time scale
to make the test pass. Do not execute arbitrary method names supplied by tapes.
Keep correlated click/event rows as one action, and refuse ambiguous targets.

DRIVE's corrected candidate `1135426` requires the observed Resume button and
invokes that same native callback. The second cold run confirmed timeScale=1,
input and camera readiness after the recorded 35.9-second pause. It then exposed
a separate watchdog bug: the last physics callback timestamp still included
paused time, so playback failed before the first resumed callback could run.

Scope a physics watchdog to each **requested pose interval**. Arm it when the
interval is requested, refresh only on an eligible real physics callback, and
disarm after verification. Menu/pause time does not count; missing first or
subsequent callbacks still time out. Render polling must not refresh it. DRIVE
`c05299f` tests this correction; the third cold run is pending. The original
recording remains unchanged. Failure journals include actual loop, time scale,
input and camera state at semantic boundaries.

## Match the claim to the test

- A fake device fixture can prove namespace handling, duplicate/missing paths,
  reconnect invalidation and partial joins. Run the new fixture on old code to
  establish that it catches the original defect.
- A muted unlatched shim during trajectory playback can prove actual hardware
  identity and controller-disable mechanics. It does not establish that ordinary
  driving consumes those values or that physical force coexistence works.
- An at-rest cache trace may be inconclusive if both enabled and disabled input
  read zero. Do not rename that observation a cache-clear proof.
- Original force/telemetry signals stay distinct from kinematic replay signals.
  A passed route is not proof of physical FFB or normalized feel.

Consumer details and private evidence indexes live in DRIVE's
`docs/reviews/2026-10-11-shim-device-identity.md` and
`docs/FULL-SESSION-PILOT.md`, Art's shim probe notes, and the portfolio hub's
`docs/2026-10-11-overnight-priorities.md`.
