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
invokes that same native callback. Its second cold proof is pending as of this
note. The original failed recording remains unchanged. Failure journals now
include actual loop, time scale, input and camera state at semantic boundaries.

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
