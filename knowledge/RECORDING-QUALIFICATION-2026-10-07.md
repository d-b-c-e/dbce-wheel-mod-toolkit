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
