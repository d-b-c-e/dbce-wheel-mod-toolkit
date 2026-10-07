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
candidate remains uninstalled pending corrections, plugin fake-sink checks,
original live-physics evidence and attended feel. Its crash remains a sampled
constant-force fallback, not Art's independent native finite effects.

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
