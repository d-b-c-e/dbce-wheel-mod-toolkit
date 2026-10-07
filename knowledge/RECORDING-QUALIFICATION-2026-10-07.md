# Exact replay is separate from complete observation

October 7 independent DRIVE review (`ea0e2ba` / `5bf93e3`, report `f4aae60`)
reproduced a misleading success path: the producer, strict reader and production
force replay all worked, but a normalization export discarded a partial crash
qualification. These are reusable lessons for STD-012/021/025, not new force
arithmetic or a change to Art's fixed reference.

- Carry the validation result through every export boundary. Calling a validator
  and ignoring its return does not enforce its qualifications. A trace with
  nine harmless road contacts, eight retained and one dropped, numerically
  replayed but was correctly crash-unqualified. The next tool marked every row
  eligible and lost that warning. Either refuse full mixed-force comparisons or
  propagate partial status and unknown-state epochs explicitly.
- Record availability of each independent producer. A successfully installed
  stock rumble hook says nothing about a separate body-contact hook. Latch and
  record runtime hook failure. Missing contacts/motion cannot become known-zero
  crash evidence; a steering-only comparison may still be useful if labelled.
- A legacy model's validity flag may mean only "mapping exists." Comparison
  eligibility must check the required grounded-wheel signals. Preserve exact
  legacy arithmetic, exclude missing inputs from calibration and keep observed
  airborne/cancelling zeros. Do not retune to compensate for unavailable data.
- Car identity is a lifecycle boundary independent of clock or position.
  Preserve/reset pending contacts and filters in a defined order, record that
  boundary, and use the same ordering in the model trial. Nearby replacement
  vehicles can evade motion-discontinuity checks.
- Validate diagnostic lifecycle fields if claiming to reproduce them. Lifetime
  counters may start nonzero at capture; compare relative baselines/increments
  and require integer domains. A complete-looking field that is never inspected
  is not checked evidence.

DRIVE's unchanged 75/1023 suite and seven independent suites (82/1052 total)
confirmed the findings and the positive pause/gap/teleport/reset controls. The
initial candidate was held pending corrections and plugin fake-sink checks.
Those source findings are now closed at DRIVE `e7e8c06`:85 suites/1087 plus59
actual-plugin boundary cases/258 checks. Original live-physics evidence and
attended feel remain open. Its crash remains a sampled
constant-force fallback, not Art's independent native finite effects.

## Exercise the actual producer and delivery boundary

Pure model tests did not expose two later DRIVE bugs. A source-linked harness
compiled byte-exact ForceFeedback.cs with production Core and fake native/game
boundaries, invoked its registered prefix bodies and replayed its emitted rows.
This added the following reusable requirements:

- Incomplete contact coverage may coexist with valid earlier contacts in the
  same tick. A successful callback followed by a read fault must retain ordered
  observed contacts, replay available intent and report partial coverage; it
  must not reject that honest producer row as contradictory.
- A capture reset must reset or record every state that a fresh replay needs,
  including the previous car identity. Warming the real producer on one car,
  resetting for capture, then sampling another found state that fresh-pipeline
  fixtures did not contain.
- Test the delivery gate itself: cold-muted must calculate and record while
  making zero acquisitions/writes; transition from an active sink must zero;
  initialization, focus, frame gaps, invalid input and shutdown need distinct
  cases. Preserve requested versus attempted/accepted commands.
- Inject native lifecycle failures. DRIVE previously logged a failed window
  exit guard and continued output; it now zeroes/stops/closes and retries later.
  Fake-sink success proves the call policy, not the guard ABI or physical torque.
- A post-gap model may intentionally seed at zero. A recovery test should allow
  that documented step before expecting force; never remove a valid release to
  satisfy an over-eager test.

The harness substitutes Unity/config/Harmony registration/native boundaries;
it does not execute real detours or load a driver. Keep this distinction in the
review and retain full rows/call order for the next agent. DRIVE's review doc
names the frozen private source, receipts and exact repeat commands. Original
recording, cold route/restoration and physical acceptance remain separate gates.

## Source identity includes checkout bytes

iRacing's October7 EuroTruck capture had zero numeric replay mismatches but an
archived strict report with passed=false: two fingerprinted source files used LF
in the analyzer checkout and CRLF in the recorded build. The four recorded hashes
were reproduced from da709ce with CRLF. Copying the matching analyzer privately
and verifying its bytes produced a strict3001-row pass without a mismatch
override. The actual Grip Strength50 export still excluded all3001 rows because
tyre force was unavailable. These are separate results: matched implementation,
exact reproduction and usable calibration input must each be checked.

Retain the original source/artifact bytes with captures. If introducing canonical
newline hashing later, version that identity contract and preserve legacy
matching; never rewrite sealed capture metadata or silently waive source checks.
Consumer evidence: iRacing docs/reviews/2026-10-07-diagnostic-grip-unavailable.md.

## A null check can invoke IL2CPP game code

iRacing's next instrumented playback (October7, 03:38 CT, consumer06095c7)
identified the missing force read precisely: `ReadFrontSideForce` called
`Unity.Collections.NativeArray<T>.op_Equality`, then
`IL2CPP.Il2CppObjectBaseToPtrNotNull`, which threw. The source's `forces == null`
was not a plain reference test. This generated wrapper represents a value type;
its equality operator unboxes both managed operands through the non-null helper.
The null operand makes the comparison itself fail, even with a valid array.
The same `ids == null` check would repeat the defect after the first is fixed.

For these generated value wrappers use `is null` or `ReferenceEquals`, retain
the native buffer/length/index/identity checks, and instrument the first exception
with both read stage and stack. Do not infer that a particular vehicle lacks tyre
forces from a catch-all unavailable result. This is not a reason to replace
UnityEngine.Object's destroyed-object null semantics throughout the camera code.
The correction still needs an original muted capture demonstrating available
forces; an old route playback and a successful null test cannot qualify new data.

Private evidence: `SessionEvidence/iracing-nativearray-null-20261007`, original
log snapshot, exact consumer source and private generated wrapper excerpt;
manifest SHA256 `4A0E593DA68D1AE99A56F17747223817754B8D5AB96234E0C99309089B8C98A1`.
No game assembly or decompiled source is distributed here. Claude received the
runtime finding through the portfolio inbox; no tuning change was proposed.

**Original-capture follow-up:** consumer5c7b3a5 / repack.138 is independently
qualified on a fresh automated EuroTruck drive: all3001 rows have valid tyre
force and active Grip, strict steering/crash replay has zero error, both actual
Strength50 exports have3001 eligible rows, and native attempts stay zero.
Full suspension observation and3001 pre-mute324-byte packets are retained.
The initial spawn rejection (wheel index -1) precedes the sealed capture and
does not justify removing index/identity guards. Prior unavailable captures
remain ineligible; no gain retune follows from this short low-speed workload.

Keep the outcomes separate: the sealed capture is valid but the original game
still exits0xC0000005; successful restoration does not convert it to a normal
exit. The frozen analyzer and complete run remain outside disposable worktrees
at `SessionEvidence/iracing-grip-fixed-20261007` (134 files; manifest
`BA1844B475C3BBC5F6244FDD9E131351810B3343DF537B63EC0ACD127740F56D`).

## Native proxy capture: guard every device-return route

Gravel's cross-review found an additional path after CreateDevice and the proxy
class factory were guarded: EnumDevicesBySemantics directly supplies a device to
the application callback. Consumer `d10e020` guards both ANSI and Unicode
callbacks before exposing the borrowed interface and returns failure when the
guard cannot be installed. ConfigureDevices is refused during bounded capture;
ordinary sessions forward unchanged. Tests derive both actual SDK interfaces,
enumerate before CreateDevice and include guard failure plus ordinary forwarding.
See Microsoft's
[method contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417807(v=vs.85)).

This is a narrow route fix. Shared-vtable behavior under concurrent creation,
system32 COM activation that bypasses the proxy factory, device-property
restoration and real driver behavior remain separate open qualifications. Never
describe copied zero-gain effect parameters or passing fake COM tests as complete
hardware suppression. The accepted Gravel renderer was not replaced; its
sampled recorder still reports hardware/trajectory/force-replay qualification
false. Device-return interception stays in the consumer; reusable writer and
effect-parameter copying stay in the toolkit.

## Audit every capture entry point when the signal schema grows

DRIVE player-guide review at059798a / installed686414c found that supervised
stage recording used a512-channel bound but ordinary F11 still used256.
Production DriveCrashStage/DriveForceTrace2 emits seven channels per contact,
up to eight contacts. With a representative original base sample (stage-only
trajectory index removed), zero contacts produces231 channels, three252,
four259 and eight287. The pinned SessionRecorder rejects the entire four-to-eight
contact row at256, incrementing InvalidCount and DroppedSamples; all nine cases
pass at512. This was reproduced offline using exact installed managed DLLs,
not an observed lost row in the sealed live capture. That original had232..242
channels per row and its stage qualification remains valid.

Test every user/developer recording route against the largest actual production
row and a reader round trip after adding contact/effect channels. A schema's
entry count is not the number present in every row. Keep bounded limits, preserve
all observations and report loss; do not trim events to fit the old bound.

Also review metadata at each entry point. DRIVE's ordinary F11 block still said
physicsChanges=None and lacked the stage force-source/reset contract. Reusing
stage metadata verbatim would falsely claim muted physical output, trajectory
alignment and a capture-start reset. Describe ordinary observations accurately;
don't reset a live force model silently just to satisfy an analysis contract.
Consumer review/fix handoff:57539c9,
docs/reviews/2026-10-07-player-docs-review.md. Fixes pending at this checkpoint;
no runtime, installed setting or native output changed by the review.

**Correction follow-up:** DRIVE8af74e6 / review at9552e7e centralizes both
profiles at512 channels and labels ordinary F11's actual handling/force state
without muting, alignment or a reset. An independent fixture using the installed
managed DLLs and production profiles round-trips every297 schema key with a
distinct value through both routes, no loss; the old256 bound rejects the row.
79 suites/1069 assertions and29 PS5.1 package cases pass. This is synthetic
serialization coverage, not fresh game or force qualification. The prior
f26d937 original remains the live signal evidence.

The delivered-artifact review also exposed a reusable deployment trap: dev and
player packages used one ownership receipt but different file inventories.
Deploy-Dev retained player-only game-mod.json bytes while replacing the receipt
with its14 runtime entries. The next player install correctly refused the now
unowned file. Actual player -> dev -> player installers reproduced this on a
dummy game even with the corrected manifest. Fresh-install/upgrade checks alone
had missed this transition. Preserve verified ownership for retained files or
align package inventories; never fix it by unconditionally adopting unknown or
modified files. A complete current-receipt hash pass does not cover orphaned
files. Add mixed-package update/uninstall cases and reconcile an already-orphaned
install from verified prior receipts with a backup. Consumer review:
docs/reviews/2026-10-07-player-doc-fixes-review.md. No live installation was changed.
