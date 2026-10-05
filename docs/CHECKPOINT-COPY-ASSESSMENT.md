# Shared checkpoint assessment

This isolated, unpublished fixture assesses OutRun proposal commit
`7e48a75be5152c5ff798c09fa1c3d25ca9ef229e` against shared master
`af938a9055782cc9f8e592cf17b055e7e4544fa0`. The released toolkit source
`358add53af220eb95bf606e240b230e23a6ea197` has the same model and parser blobs
as this master. No production header, native ABI, profile, golden file or
package version changes are proposed.

The original proposal is incompatible with both shared revisions. Its pinned
model/parser differ; its Shaper friend-patch anchor does not match. Its 31-value
configuration omits smoothing_ms, impulse_bypass, settle_below and releveling.
Its state omits previous-input history and rejection flags. The fixture proves
that the old configuration projection can collide while actual outputs differ,
and that identical public output/ramp indicators can hide different continuations.
Do not widen acceptance of the original v1 numeric representation.

The alternative here uses the existing public C++ copy constructor/assignment
operations in a fixture-local anonymous namespace. Immutable owning copies retain
all current Model and Shaper fields without friends or a duplicated computation.
It checks its local revision, pinned source identifiers, profile identity and all
35 typed settings before either destination assignment. Compile-time checks require
trivially copyable classes and nonthrowing assignments. This provides failure
atomicity within the single-threaded fixture; concurrent observation is outside scope.

This is a trusted same-process value-copy experiment, not a public snapshot API,
serialized format, recorder schema, or validation of arbitrary private-state input.
The source identifiers are provenance checks, not authentication. The runner checks
actual raw Git blob identities before compilation. No raw object bytes are serialized.
The OutRun evidence of 303 replays and 856 malformed-state rejections belongs to its
old pinned implementation; those counts are neither rerun nor claimed for this fixture.
Full PC event capture and any external snapshot codec require separate review.

Run with PowerShell 7 and installed MSVC, using an absolute output directory outside
the checkout:

```powershell
./native/forcemodel/Test-CheckpointCopy.ps1 -OutputRoot C:/Temp/checkpoint-copy-review
./native/forcemodel/build_conformance.bat
```

Each x86/x64 run checks 264 exact continuations and 462 rejected context/configuration
restores across ten shipped profiles and one fixture-only configuration. Replay cases
cover normal state, reset, ramp restart, invalid-input recovery, variable timing and
the four newer settings. Rejected restores compare both destinations with untouched
controls through subsequent actual computation. The existing conformance golden,
events, signal-contract and recovery tests must also pass. These executables use
only model math and local profile files, with no native wheel layer or device output.
