# Sonic Racing: CrossWorlds wheel project

The owner requested a dedicated research/implementation task on 2026-09-28,
coordinated from this toolkit. The goal is usable wheel input, game-informed FFB
and meaningful telemetry, with a mod UI or launcher. Digital wheel controls,
especially the POV hat, must be able to supply right-stick directions.

## Ownership and checkpoints

- Implementation task: **Sonic CrossWorlds wheel support**,
  `01a0eb21-1a99-7320-a305-a752b6064a28`, local host.
- Assigned private repository: `E:\Source\sonic-crossworlds-wheel`. The task owns
  creating it, its source, research notes, tests, installer and documentation.
- Coordinator: this toolkit task, `01a07ac3-e639-7762-a698-5365d447ee7e`.
  Two-way task messaging is authorized by the owner.
- Hourly heartbeat: `sonic-crossworlds-wheel-coordination`, active. Review
  substantive evidence/checkpoints, remain quiet on unchanged status, and pause
  when a usable first candidate or a confirmed blocker needs owner handoff.
- Checkpoints: installed-build evidence; integration choice; first offline
  implementation/build; reversible deployment; live input/telemetry/force checks.

This is a **research candidate**, not a verified toolkit consumer. Neither the
task's creation nor the planned repository establishes working game support.
Inspect its `docs/STATE.md` and `docs/RESEARCH.md` for subsequent progress.

The private [repository](https://github.com/d-b-c-e/sonic-crossworlds-wheel) is
now created and verified private. The task proposes a companion launcher with
calibrated wheel-to-virtual-controller input, followed by a game-state bridge
for telemetry. ViGEmBus availability is a local prerequisite observation, not
proof that this game accepts the output. The coordinator accepted this as an
investigation route, with the following integration gate: v0.15.0's toolkit
reader does not expose POV state, so a separate DirectInput reader must not
silently create a second DirectInput instance alongside toolkit FFB in the
same process. Resolve a single-owner reader/output design before enabling FFB;
separate processes alone do not prove competing wheel handles will coexist.
An additive shared-reader API or a verified HID/RawInput route needs its own
identity, lifecycle and hardware evidence. Input/UI work can proceed meanwhile.

## First offline review, 2026-09-28

The Sonic task reported 11 passing mapper/parser checks and a matching installed
executable preflight. Read-only enumeration identified the MOZA base, DS-8X and
MOZA stalk; standalone pedals were not enumerated. No virtual controller was
attached, game launched or FFB initialized. Its initial console wizard and
uninstalled UE4SS diagnostic probe are prototypes, not accepted player UX or
verified game telemetry.

The coordinator inspected that first uncommitted source checkpoint and returned
concrete fixes: neutralize all driving output when a required pedal reader fails;
resolve opposing raw directions before flick timers; require a valid release
after initial activation/focus loss/device discontinuity; make the expected game
control layout explicit; capture new button edges while ignoring already-held
gear/switch states; and add a graphical, grouped binding workflow. The Lua probe
also needs single-flight scheduling so a stalled game thread cannot accumulate
callbacks. Its coarse source time is not sufficient for a future FFB freshness
gate. These findings are assigned work, not a claim that current source remains
broken or that the 11 checks establish game acceptance.

The toolkit POV subtask runs in the isolated `toolkit-pov-input` managed worktree,
branch `codex/toolkit-pov-input` from v0.15.0. Its additive native 0.9.0 design is
`ReadDeviceStateWithPov(slot, axes, axisCount, buttons, buttonCount, pov, povCount)`:
exactly eight axes/four hats and zero to 128 buttons, optional managed capability,
neutral hats on failure, and a single existing-handle snapshot. Failed raw axes
must not be treated as neutral pedals. Before promoting an input-only reader to
FFB ownership, neutralize virtual output, close readers, initialize FFB and reopen
the saved GUIDs to share the FFB handle. Disabling FFB or changing its device also
requires neutral output and reopening affected readers by GUID; FFB Off must
leave input operational. The consumer must not adopt partial files or treat
offline candidate tests as hardware acceptance.

## Reviewed toolkit candidate handoff, 2026-09-28

The POV subtask is complete and the coordinator has sent the Sonic task a
reviewed offline candidate. This is available for deliberate adoption, not
evidence that the consumer has adopted it. The source branch remains separate
from master and no official release was created.

| Identity | Value |
|---|---|
| Candidate / native component | `0.16.0-pov.1` / `0.9.0`, 47 exports |
| Clean source, pushed branch | `397a5f22af38b2459f18ffa2e1d685ee838cce8f`, `codex/toolkit-pov-input` |
| Worktree | `C:\Users\antho\.codex\worktrees\toolkit-pov-input\dbce-wheel-mod-toolkit` |
| Archive within worktree | `dist/dbce-wheel-mod-toolkit-0.16.0-pov.1.zip` |
| Archive SHA-256 | `0718CACC01718DF25F326E2C6B22C5CBA700170ECCF86A6790092B0FD5F69FB4` |
| Receipt | `dist/POV-HANDOFF.json` |
| Receipt SHA-256 | `93220C09ADE38DDF52EF33A3D4DB9ED0CD80BBAB64EBFED61D7F698175F08EB8` |
| Default local sync identity | `local+397a5f22af38b2459f18ffa2e1d685ee838cce8f.clean` |

The [contract at the reviewed source](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/397a5f22af38b2459f18ffa2e1d685ee838cce8f/docs/POV-INPUT.md)
defines the native API and optional managed `SupportsPovRead` capability with
`Read(slot, axes8, buttons, pov4)`. Existing native modules remain loadable;
missing capability fails explicitly. No live native-module replacement is
supported. Zeroed raw axes after failure must never be mapped as valid pedals.

The subtask reported 121 managed tests, 24 sync assertions, native production-path
POV fakes on x64/x86, current and legacy managed-binding checks on two runtimes,
and existing conformance suites passing. Actual DLL load, all exports, version
and invalid-slot clearing were checked on both architectures without acquiring
a wheel. The coordinator independently verified the clean source/remote branch,
archive and receipt hashes, and all 64 archived payload hashes. The receipt
records exact component hashes and the five default synced payloads. A pre-existing
`-Parts include` discrepancy omits `impact_mixer.h` from the package; default
native/managed adoption and the separately checked `wheelffb.h` are unaffected.

Both hardware smoke harnesses were **compiled only, not run**. No game, virtual
controller or physical force acceptance is established by this handoff.

## External companion ownership gate

POV support does not settle the companion's FFB integration. The v0.15.0 native
finite-burst paths require the foreground process to be the DLL's own process.
An external launcher therefore cannot use them unchanged while Sonic has focus.
[Microsoft's DirectInput cooperative-level contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417921(v=vs.85))
also requires the supplied top-level window to belong to the calling process.
Passing the game's window to a companion is not a valid shortcut. The current
input-only shared reader's window fallback also needs review before adoption.

The Sonic task must choose an in-process output backend or propose a separate,
reviewed external-session focus/lifecycle contract with its own owned window.
Do not weaken foreground safety checks or spoof process identity. Preserve
single-owner input/output lifecycle, explicit target identity, neutral output
on focus loss/disconnect/stale game state and both FFB transition directions.
This constraint was sent to the task; it is not part of the POV candidate and
does not prevent continued offline UI, mapping or telemetry-probe work.

The next Sonic checkpoint reports a building WinForms launcher and 15 passing
offline mapper/telemetry checks. The coordinator's follow-up source inspection
confirmed required-axis fail-closed handling, raw opposing-direction cancellation,
right-stick release/rearm and process-owned input HWND changes. The task reports
the probe callback is now single-flight. The source was still uncommitted at this
review, and no window was opened for visual acceptance. Follow-up findings sent
to the task cover Item/Pause rearming, virtual-controller exclusion by identity,
explicit GroupBox interior layout and meaningful calibration/config validation.
Request an immutable source/package checkpoint before deployment or adoption
claims; do not confuse this review with completion of those remaining fixes.

## First committed consumer checkpoint

Sonic pushed clean source `9e3f4cf849c07a99bd39c066432ce3ce64e432c0` to its private
`main` branch. The coordinator verified that local and remote identities agree,
and inspected the revised per-action rearm and calibration validation. The task
reports 18 offline tests and a release build passing. Its disposable installer
fixture reportedly rejects modified owned payloads and preserves settings and
an unrelated mod. No actual deployment, virtual controller attachment, UI render,
game drive or force output has occurred.

The existing `dist/package-0.1.0-review` launcher hashes to
`7C27D24703BD09AB5CC21FE046D76B76C12461D404A9099BD58826AD49712302`;
the optional probe hashes to
`7EDF275B20E97D4CEB3FD6B00D90D24400AB168449C38D73F6B1DE6D054D7330`.
The coordinator verified those hashes. This package's manifest lacks source
revision/tree/dirty provenance, so these are artifact identities, not a claim
that it was built from the later clean commit. The next assignment requests
provenance in the packaging script and a fresh build from committed source,
preserving the existing review package.

The Sonic task has **not adopted** the separate toolkit POV candidate. Its next
offline architecture task compares in-process ownership against an explicit
external-session contract, including process lifetime identity, foreground
ownership, monotonic freshness, independently enforced cancellation and both
input/FFB lifecycle transitions. Calling an older continuous-force path merely
to avoid finite-effect foreground checks is not an accepted workaround. The
implementation still needs verified player-driving telemetry before force work.

## Reproducible package and next architecture review

The packaging follow-up is complete. Source commit
`73474427a3321b87e4249ed344e940aeaf3a7af4`, tree
`aadab7a9c91d1200d31faffe2d6584e0e94dcde9`, now rejects dirty-source packaging
and records source and build-runtime identity. The fresh `dist/package-7347442`
manifest records `SourceDirty=false`, SDK `10.0.401`, `net10.0-windows` / `win-x64`
and framework-dependent output. Its manifest SHA-256 is
`696ED2A536DAFFF77B5E9CBD7675335A6F0D92B54862A6A9C27C9AF731FE2392`;
the launcher SHA-256 is
`8700BE0A783F90D1B6510445F63298B4D8F7E7CE13EEA1AE2778A8C2E7F7F8AD`.
The separately committed receipt `docs/receipts/package-7347442.json` is in
consumer commit `70a32d1889041dacfacf2193711f2cd13e826af7` and hashes to
`9B07F9385807F917F77DE3AFD7E4BB549AE4A4685BAEA93E424A274862BFD697`.
The coordinator verified those three hashes and the clean checkout. The task
reports dirty-source refusal and the fresh disposable install/uninstall fixture
passing. The earlier review package remains a separate artifact.

Sonic's revised `docs/EXTERNAL-FFB.md` chooses a dedicated external device-owner
process for the next prototype, with in-process ownership retained as a fallback.
The coordinator assigned the native toolkit specialist a **design-first** review
of a possible additive external-session API; runtime implementation has not been
approved or begun at this checkpoint. Reuse of the free POV worktree on a new
`codex/external-session-ffb` branch is planned if the design is accepted. Preserve
the frozen POV branch/artifacts and all published v0.15.0 identities.

Review priorities include finite constant output as the minimum surface, exact
target lifetime, own-window initialization, sticky prevention of legacy force
bypasses, explicit rearm, and age-aware source leases. The existing watchdog
takes a lock around driver calls: it cannot promise timely cancellation through
a stalled driver or after its own process has died. Short device-side effect
duration is therefore required, with physical driver behavior still unverified.
The Sonic task is assigned independent, device-free IPC/lease design and fixtures;
it must wait for a reviewed native interface before implementing FFB bindings.

Sonic commit `d2853b0b60ff2707c6ee94b31f266abb85d1938b` adds the IPC/source-lease
draft and 18 design vectors; those vectors had not executed at review. The
coordinator corrected its proposed global release/steering-center gate: held
optional actions must not trap unrelated input, and valid steering does not need
recentering after focus return. Per-action/pedal release masking remains separate
from FFB eligibility. Sonic is now assigned an executable fake-clock/mailbox
validator, including stale-at-use data and immediate invalidation by a new
non-driving/unknown sample. Windows QPC is suitable for same-system comparisons,
with a boot-fixed frequency and a documented one-tick cross-thread ambiguity;
see [Microsoft's timing guidance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
The current diagnostic JSONL cannot refresh a force lease.

## External-session implementation assigned

The native specialist's design review is complete and the coordinator approved
an **offline experimental implementation**, separate from the published runtime
and preserved POV candidate. The planned `codex/external-session-ffb` branch starts
from `397a5f22af38b2459f18ffa2e1d685ee838cce8f` in the existing free POV worktree.
No resulting native implementation or artifact has been accepted yet.

The first output surface is one finite constant effect, bounded to 50 ms and the
remaining source/client lease. It uses a new owned-window initialization path
that never creates or starts the legacy infinite steering effect. External mode
remains selected for process lifetime; legacy force paths cannot bypass it.
Native independently checks pinned target/client/producer lifetimes, foreground
target ownership, strict wheel identity, producer-capture age and a fresh native
arm-generation ticket. Revocation cannot wait for the driver lock. Device calls
and destruction remain serialized, including shared wheel reads. Old consumers
that never enter this mode retain their behavior.

The native-issued challenge must reach the actual producer and be echoed from
a newly captured game sample. Helper-side restamping of cached JSONL proves
nothing. Sonic is assigned the required bidirectional source control design and
verification of whether UE4SS Lua can provide genuine QPC/IPC or needs a native
producer bridge. Native cannot establish the truth of player/race attestations.
Hung driver calls may still complete late; physical duration is unverified and
must never be described as a guaranteed wall-clock stop bound. The next review
is the finalized ABI/status contract and lifecycle implementation, followed by
offline fake/legacy/architecture checks and a clean-source candidate receipt.

Consumer commit `62d338cdc27d63a163d06474ec0f29f55b702caa` adds a pure, device-free
mailbox/source-lease/input-gate implementation and reports 29 tests passing. It
does not replace the running launcher's input path or implement an OS pipe or
FFB call. Its 26 JSON vectors remain design data, separate from executed tests.
The coordinator inspected the helpers and tests and requested one further
regression: when the consumer loop itself stalls, accepting a fresh new input
frame must first notice that the previous sample expired and reset recovery.
It cannot rely on a timer having called the stale-sample check during the stall.
The current launcher package remains the separately recorded `7347442` artifact.

The 2026-09-29 04:09 UTC coordinator heartbeat reviewed Sonic checkpoint
`e31cbcb9514cea63148d4b0943a3065757346f41`. The stalled-loop fix and regressions are
present; the task reports 37 offline tests passing and 34 separate JSON design
vectors, which are not mechanically executed. Its producer review finds no
documented built-in UE4SS Lua QPC/pipe primitive and proposes a native Lua C
library for timing and bounded IPC, contingent on the exact loader/Lua ABI.
Neither that bridge nor real game-state sampling has been implemented or tested.

The heartbeat also resolved an inter-project timing ambiguity: ticket expiry
bounds the **initial producer echo**, not the whole healthy arm generation.
Fresh source/client leases may keep that generation eligible after the initial
ticket deadline. Any fault/revocation requires a new handshake; delayed cached
state cannot revive it. Both tasks received this clarification. The native
provisional header/policy review requested explicit source-first/client-first
startup handling and rejection of oversized handle values on x86. Runtime work
is active on `codex/external-session-ffb`; no candidate artifact has been accepted.

Sonic then committed the agreed initial-echo semantics and the analogous active
source-lease stall fix in `c484f038ee570740e0b9ad8e63e64b6b1e7f6948`, reporting
42 offline tests and 38 separate design vectors. The coordinator inspected the
old-lease-before-replacement check and exclusive initial deadline. The next
independent consumer assignment is bounded named-pipe transport and headless
peer-process tests using fake frames only: framing limits, backpressure, exact
OS peer lifetime, disconnect and capture-time preservation. These tests may not
open devices, attach a virtual pad, load FFB, launch a game or display windows.

The first uncommitted native implementation passed its reported x64 build and
55-export/header check, then received an intermediate coordinator review. The
review found that a recursive critical section permits same-thread window/driver
callback reentry: cleanup must not stop/release objects while their driver call
is still on the stack. The assigned fix must defer cleanup while independently
revoking authorization, with synchronous reentry fakes in addition to stalled
cross-thread calls. Further review requests cover copying call parameters before
blocking driver work, verifying trigger/start-delay as well as finite duration,
and reporting cleanup failures rather than unconditional successful Close.
Orderly cleanup must restore the actual prior autocentre value after unacquiring;
the task has incorporated that requirement and is adding ordering/failure tests.
Physical behavior remains unverified and no native artifact is accepted.

The reused worktree's live sources/build outputs now belong to the external-session
work in progress. **Do not sync the old POV candidate from that live checkout.**
Its preserved ZIP/manifest/receipt, at the hashes recorded above, remain the
reviewed frozen artifacts. Sonic received this provenance reminder and continues
to hold native adoption pending a separate external-session handoff.

The 04:38 UTC follow-up reviewed consumer transport checkpoint
`456155af34860b2978a08f2bce78e2ce48021800` and its executable tests. The task
reports 55/55 passing tests, including 13 framing, queue and headless peer-process
cases. This separate library is not wired into the launcher or its prior package.
Review found a synchronous server pipe and no actual blocked-peer cancellation
test; a partial prefix/body can keep a read waiting. The consumer is assigned
overlapped cancellable I/O, bounded whole-frame/drain deadlines, and real headless
tests of pending accept, partial messages and a peer that stops reading. The
review also requires length checks before parsing/retention, immutable bounded
payload snapshots, and endpoint/kind/epoch/sequence validation for every drained
frame. Fault/control messages must not be silently replaced on the state channel.
Expected executable identity and exact payload schemas remain integration gates.

The native specialist reports 814 x64 production-path fake checks after fixing
the intermediate review findings. The coordinator inspected the revised logical
in-flight guard, deferred same-thread cleanup, input-struct copies, finite
duration/trigger/start-delay readback, prior-autocentre restoration, and retained
closed cleanup-failure status. These address the source review findings.
x86/x64 ABI, managed optional/legacy bindings and an immutable clean-source
candidate receipt remain required before artifact acceptance. No consumer pin
changed, no official release was published, and no game/device was opened.

The 04:47 UTC follow-up reviewed consumer repair commit
`7e46ac88eff58ce321ccb7e7e21ac49ee11e4d40` and independently reran its Release
suite: **67 passed, zero failed/skipped**. Actual overlapped accept/read/write
cancellation, bounded immutable publication and every-drained-frame envelope
checks address the previous transport findings. The next assigned checkpoint
connects typed payloads, expected peer identity and existing eligibility gates
in a device-free headless scenario. In particular, a not-driving/unknown sample
must revoke its generation even if later eligible-looking state is coalesced or
drained: the producer must latch/reliably report invalidation and the receiver
must apply invalidating semantics before discarding intermediate frames. The
launcher and preserved clean package remain unchanged.

The native specialist's final self-review also bounded pulse duration by the
remaining native wheel-read lease, added read-only real Win32 process-identity
checks, and deferred owner WM_CLOSE forwarding while driver cleanup is pending.
The owner must keep its window pumping and retry close after final status.
The coordinator reviewed the lease clipping and managed optional API wrapper;
final original-WndProc forwarding regressions and clean packaging are in flight.
No candidate receipt is accepted yet.

The final external-session candidate is clean/pushed source
`06340b38ec5549ee3fa477668da097e559e61c6e`, tree
`4743e646552f69faefb305baa1487a9e69ba0873`, on `codex/external-session-ffb`.
Local candidate **0.16.0-external.1**, native **0.10.0 / 55 exports**, includes
the final pending/final WM_CLOSE forwarding regressions. The coordinator
verified the source/receipt identity and all66 archive payload hashes.

- ZIP SHA-256: `D99FFBBF1C52C95703021DF5032C98B609564D80C9D9C042F70411D66FE0B5A8`.
- `dist/EXTERNAL-HANDOFF.json` SHA-256: `1044CF6B409E0A5539663747725487B56E20C727C1D98A1687BA5DC1838D2E6F`.
- Payload manifest SHA-256: `4D493AF28BDCF8224011CB5131941F494D9ABAAE271D5B7460FFB132DA9A14B5`.
- Native x64: `968E548C8FFDFD1BEB1DE92C818316362C0F71E9FC1EA8CBB9CA15CB19491EF8`;
  x86: `94B519DDCC96E8371CA7E3F1C71EE1F4C69B2D212531E9181318C7BB523EB2A6`.
- Managed Ffb: `A5DFB5730861D5C6830EC1B7573B31523DA0C3743731DF071648E220268CDAC8`.

The specialist reports851/852 native production checks (x64/x86),121 managed,
24 sync, all55 exports, legacy/conformance checks and exact-packaged managed
optional/legacy fixtures on both runtimes. Hardware smoke was compiled only;
no enumeration/acquisition occurred. Artifacts remain in the toolkit-pov-input
worktree's `dist`; frozen POV and official v0.15 identities remain unchanged.
Sonic received this concrete API reference for offline work. No automatic
consumer repin, deployment or official release was performed; genuine producer
integration and attended force behavior remain open.

Consumer source `7a6967b252a747d82b54f34b9ec5831ad5c8b498` adds exact v1
payload decoding, independently pinned expected peers, producer invalidation
latching/reliable notification and an observation-only session coordinator.
The task reports80/80 tests, including actual headless pipe scenarios. The
coordinator reviewed the source/test checkpoint and assigned two defects:
target liveness gates force but not input, allowing healthy owner traffic to
retain nonneutral input after target exit; and ApplyControl replaces heartbeat
time before observing the previous lease, allowing a stalled timer to miss a
generation-revoking expiry. Required tests isolate a headless target exiting
while owner traffic stays fresh, and both heartbeat roles crossing expiry
without Tick while input/game samples stay young. Mutable settings/known-device
sets must also be frozen for a session. These were the open findings at that checkpoint.
Per-drained-frame semantic application and producer-side invalidation latching
address the prior coalescing defect. No launcher/package/native adoption changed.

Repair source `a41a9d1a801487f30af45d0e0360916579ff291a` closes those findings.
The coordinator reviewed the diff and independently reran all 83 tests with no
failures. A real separate headless target exits while owner process/pipe remain
live; input neutralizes and the session cannot restart. Both heartbeat roles
cross their old lease without a Tick while state remains young; renewal cannot
retain the old generation. Owner expiry retires input, while producer recovery
requires a new generation. Caller settings and GUID-set mutation no longer
changes a running session. No device/game/virtual-pad path was exercised.

The next focused assignment is an offline adapter to the reviewed toolkit
external-session wrapper, with a fake native API testing lifecycle, native-issued
ticket/QPC translation, expiry, revocation and cleanup. Production launcher
integration, consumer pins and the package remain unchanged. Exact UE4SS/Lua ABI,
the native producer bridge and verified local-player signals remain unverified.

Adapter checkpoint `b7cafe2a6e8047648ca4e36b0c4384dad6255cad` adds an opt-in
managed reference and five fake-API tests, all independently rerun successfully.
It is **not accepted**: the fake does not match actual native semantics and
conceals integration errors. Successful native Arm returns Waiting with a ticket,
not Ok. Native client authority must come from the separately pinned UI/client,
not the device owner's input samples; the first client capture must follow Arm.
The adapter also rejects zero despite native zero submission canceling the old
pulse. Repairs require distinct-role lifetime tests, genuine client-QPC frames,
faithful result/lease boundaries, and fresh nonzero-to-zero cancellation coverage.

The candidate-reference project also builds into the toolkit checkout's normal
managed output folders. Future optional tests must isolate those outputs. The
coordinator rechecked the immutable external ZIP and receipt hashes afterward;
both remain unchanged. No native DLL was loaded, no device was opened, and no
launcher/pin/package was changed. The Sonic task owns these focused repairs.

### Adapter repair review, 2026-09-29

Clean consumer source `4817f4649ebc31a57f5c1b00419de7b6c0ca4015` addresses the
prior result, identity, zero-force and fixture isolation findings. The UI/client
now has its own pinned process and authenticated bounded eligibility frames;
native renewal retains that client's original post-ticket QPC. The toolkit's
managed source reference builds in the consumer's ignored fixture directory.

This checkpoint remains **unaccepted**. The consumer reported 11 optional and
83 standard passing tests. The coordinator's independent optional run passed
10/11: `SeparateClientExitRevokesWhileOwnerAndProducerRemainHealthy` returned
Applied rather than Ineligible after the headless client exited. One immediate
isolated rerun passed; this is an intermittent failure requiring diagnosis,
not evidence that production process-lifetime checks are proven broken.

Source review also found a local Waiting return before source-sequence/capture
consumption. A source submitted before client renewal could be reused after
renewal; it must instead require a genuinely newer capture. The fake native API
does not observe old client/source deadlines before replacement and incorrectly
expects Waiting at the initial client deadline, where native revokes. Focused
regressions and faithful half-open expiry behavior are assigned to Sonic.

No native DLL was loaded, device acquired, game launched or virtual pad attached.
Production launcher, vendor pins and the clean `7347442` package remain unchanged.

### Accepted offline adapter repair

Clean `d1427217fd2ff5a4827bd5f171ea13811aa0ed6b` closes the focused findings.
The consumer reproduced the client-exit failure in 9 of 15 runs: the authenticated
pipe was disconnected while the separately pinned process still appeared live.
The gate now retains that authenticated pipe lease and checks it on every use;
the task reports 12 subsequent focused passes without a delay workaround.
Source-first Waiting consumes the capture, so renewal cannot replay it. The fake
now observes old client/source deadlines before replacement at exact expiry.
The coordinator reviewed the diff and independently passed all 14 optional and
83 standard tests. This accepts the offline adapter checkpoint only.

The next bounded assignment is actual producer feasibility: establish a specific
prospective UE4SS/native Lua ABI from official sources and read-only installed
evidence, then implement the smallest timing/IPC bridge only if that ABI can be
substantiated. Require designated-thread single-use capture tokens, actual QPC,
bounded capture duration, native ticket propagation, immutable bounded messages,
and reliable invalidation or abort before coalescing. Headless fixtures may test
transport behavior but must not claim real local-player/driving fields. If the
ABI cannot be established offline, return a precise feasibility handoff instead
of guessing bindings. Launcher, package, pins and all desktop/device paths remain
unchanged; the Sonic heartbeat continues.

### Producer ABI checkpoint

Clean consumer `eddb4e6787558580bc765358ed83614afa68ff08` pins prospective
UE4SS v3.0.1 source `d935b5b23bac03b65c14ae38382b02007204cc2e` and official
archive SHA-256 `4B47D4BCEDDD2F561A4E395BFA00924CCFC945AF576A2D0C613E6537846C57EC`.
The extracted DLL is `8AC18FBFFC1EF96B0662D4A2D537B3F224C26D65CAABA7989A9404C566102B26`.
The coordinator independently verified both hashes, x64 PE format, absence of a
Lua DLL import, and zero standard Lua C API export names. Tagged Lua 5.4.4 and
module search-path support alone do not establish a supported native Lua module
link/state ABI. No guessed bridge is accepted or installed.

This is a route limitation, not a project-wide blocker. The official
[tagged C++ mod guide](https://github.com/UE4SS-RE/RE-UE4SS/blob/v3.0.1/docs/guides/creating-a-c%2B%2B-mod.md)
and [base class](https://github.com/UE4SS-RE/RE-UE4SS/blob/v3.0.1/UE4SS/include/Mod/CppUserModBase.hpp)
provide an alternative worth a bounded offline check; the latter explicitly
requires matching C runtime/configuration. The inspected DLL has 12 base-class
export names, and the official release lists a developer archive. These facts
do not yet prove SDK compatibility or game-thread execution. Sonic is assigned
exact developer artifact/import/toolchain inspection, game-thread hook semantics
and missing dependency identification before implementation or an owner handoff.
The shared-desktop pause and all frozen pins/packages remain unchanged.

### Owner handoff: producer prerequisite unresolved

Consumer `1620dec945d94a975d9f28420c38d0393e67e308` completes the bounded C++
alternative review. The coordinator independently verified the official developer
ZIP SHA-256 `AF777D2AF33FCE4D520DE35A8665E417376BCC530FEC647778012CE6CDF51B2B`,
its 166-entry inventory with no `.lib`, `.h` or `.hpp`, and the included DLL's
exact match to the basic release. It contains a PDB, not a standalone SDK.
The tagged repository's `deps/first/Unreal` points to UEPseudo commit
`d09b7218bfe7392adeffb500fdeee0b42ca1cd27`; a fresh read-only GitHub request in
the coordinator's current context also returns HTTP 404. This establishes
unavailability in this context, not that the repository cannot be accessed by
an appropriately authorized owner.

The [tagged build instructions](https://github.com/UE4SS-RE/RE-UE4SS/blob/v3.0.1/README.md#build-instructions)
require Epic-linked source access. The coordinator inspected `UE4SSProgram.cpp`:
`on_update` runs from an auxiliary `std::jthread`, not a demonstrated game-thread
capture. The ProcessEvent callback used by Lua's game-thread helper has an
explicit asynchronous-UFunction caveat. Neither route is ready to produce
force-eligible game state merely from export names or a host fixture.

The **Sonic heartbeat is paused** at this confirmed prerequisite/owner choice.
The next step is legitimate access to the pinned dependency and a reproducible
matching loader/mod build, or selection of a separately reviewed supported SDK.
After that, a newly authorized game-only slot must verify loader compatibility,
callback thread/QPC/deadline/shutdown and real local-player/driving/mode fields.
Wheel input, virtual-pad and physical force acceptance remain separate.
No account/access change, game installation/launch, device acquisition or force
test was performed. This is not a conclusion that Sonic modding is impossible.

Preserved deliverables: clean input-only package `7347442`, accepted offline
adapter `d142721` with coordinator 14+83 passing tests, frozen toolkit external
candidate `06340b3` and its receipt, and the consumer's detailed
`docs/PRODUCER-BRIDGE.md`. No new consumer pin or official release is implied.

### Owner-requested source/build preparation, 2026-09-29

The owner separately requested dependency installation, source import and build
setup in the Sonic repository. Clean source
`750121fb882af9fee6f1000b342bf662a0878a70` adds the pinned UE4SS submodule,
`scripts/Build-Ue4ssCpp.ps1`, and a field-free C++ smoke target built alongside
UE4SS. It does not use the separate official release DLL as its link target.
Outputs and paired build hashes belong under ignored `obj/ue4ss-cpp`; nothing
is installed into the game or package by this script.

The consumer installed Rustup/stable and attempted recursive import/build under
that request. Import stopped at the same private Unreal dependency before
compilation. The coordinator independently reviewed the source, passed script
parsing and `-ToolsOnly` without installing tools or retrying the full import.
Preflight reports Git 2.55.0, bundled CMake 3.31.6, Rust 1.98.1 and VS2022
BuildTools. Recursive status pins UE4SS `d935b5b` and public patternsleuth
`33e731e99f2a6bb7f65a8e95e89fd1c06ce9d1d2`; Unreal `d09b721` is uninitialized.

This is prepared build setup, not successful compilation or a proven SDK/game
interface. Upstream configure still uses some moving dependency references;
the notes explicitly avoid a byte-for-byte reproducibility claim. After legitimate
source access is available, the consumer's rerun command is
`.\scripts\Build-Ue4ssCpp.ps1`. The coordination heartbeat stays paused. No new
game slot, loader installation, virtual pad, device acquisition or force testing
is authorized by this follow-up.

### Access resolved and paired source build reviewed, 2026-09-29

The owner's Epic-linked access now reaches the pinned Unreal dependency.
Local consumer commit `122336a996a204abf2eaec82ddffbdd77cd9df3d`, tree
`f5a89af389f4d4fb19962b85830975cdfbc79d07`, builds UE4SS and the field-free
smoke mod together in `Game__Shipping__Win64`. At review, the consumer's 11
commits after its remote main are local only. The official downloaded loader
and all earlier package/toolkit artifacts remain separate.

The build pins historical ImGuiColorTextEdit `af7821926251feca84e35f8fa83eee84dae90424`,
temporarily applies then reverses two UEPseudo compilation fixes, supplies actual
Rust compiler/Cargo paths, and filters a Corrosion/MSVC linker directive. Existing
notes retain the limit that other moving dependencies/toolchains prevent a
byte-for-byte rebuild promise. Source and nested submodules are clean after build.

The coordinator reviewed those changes, parsed the script, and independently
verified the local build receipt without rebuilding or loading the artifacts:

- UE4SS.dll: `F2BCAEA1AAB001AB9E85567B05EBB151A25C78C336AE16C1BA0F609460B4F9CA`.
- C++ main.dll: `F074E37FB62E31286E4ECDE3BEFC55334FAD7B3B59509B5A4E93F0AFC5DB8978`.
- Compatibility patch: `C4B3596606EAE53DA9637EE9645FEE79D620B29DC4D7A7173ED2C0F7F67D0AC2`.

Read-only PE inspection confirms x64, UE4SS/base-class imports, and the mod's
`start_mod` / `uninstall_mod` exports. This closes the source-access/build
prerequisite for this pair, not runtime compatibility or game-thread capture.

Offline coordination resumes. The bounded next assignment preserves this pair
and adds an immutable source/build receipt, then prepares a diagnostic candidate
and reversible staging/rollback plan without installing anything. First inspect
ProcessEvent callback lifetime: the now-available header registers a `void`
callback with no visible unregister API, while UE4SS can destroy/unload a C++
mod. Do not retain callbacks into a destroyed instance or unloaded module.
Return the lifecycle design before adopting a workaround. Diagnostics may record
bounded QPC/thread/lifecycle evidence, with no wheel, virtual pad, force or guessed
gameplay fields. No game slot is authorized by the access change. The existing
desktop pause remains until the owner separately authorizes runtime validation.

### Frozen diagnostic review, 2026-09-29

Consumer diagnostic source `afd6b533715344f63fdc994eae66f885a9948e5c`, tree
`f99c6d7f0aea855d4b9f486e4caffd3671145970`, and receipt follow-up
`f18432fa4f54480f706962eb6c296f595a8d42c3` preserve separate smoke and diagnostic
checkpoints. The coordinator independently checked both declared source trees,
all eight source-file identities with Git's working-file line endings accounted
for, and all 15 checkpoint/review-stage hashes and read-only attributes.

Frozen diagnostic identities:

- UE4SS.dll: `125BBF5E0A56B09BA41ECCC3424A2C3C59A39D45F676FD88659DE43A255901E9`.
- main.dll: `614F8D467E889D1BE56B904AC1400F33E9977FE4BA91C8C73936BB2683509ECC`.
- Source-built dwmapi.dll proxy: `AFCC9A731F3AD008356B01FE3B1C2D76411492EB9F5D51C95B2F8B0CDC0AB6C9`.
- Consumer receipt: `docs/receipts/ue4ss-diagnostic-afd6b53.json`.
- Offline layout: `obj/ue4ss-diagnostic/review-stage-125bbf5e-614f8d46`.

The field-free diagnostic logs at most 16 lifecycle/QPC/thread records, including
at most eight explicitly auxiliary `on_update` samples. It registers no
ProcessEvent callback. The coordinator confirmed that pinned Hooks.cpp retains
callbacks in a static vector while UE4SS can delete and unload a C++ mod;
the auxiliary event-loop pause does not guard that dispatch. This remains a
future game-thread producer design gate, not something the diagnostic solves.

The consumer's `docs/UE4SS-GAME-ONLY-PLAN.md` describes exact target/receipt
preflight, absence-only staging, a normal Steam launch, limited observations and
hash-owned rollback after exit. It stops on conflicting mods/files, mismatch or
unexpected behavior and does not silently switch injector/proxy strategies.
No coordinator or consumer game installation/launch is established by this review.

One reusable build-helper defect was returned for repair: recursive DLL searches
also count the preserved checkpoint and review-stage copies. Independent read-only
enumeration found three matches for each diagnostic DLL, making the next build's
exactly-one check fail. Consumer repair `29d1f7c464fdb46dab87b48e10abdfbe901b10a2`
uses exact target/configuration paths. The coordinator independently passed
PowerShell parsing, `-ToolsOnly` and `Get-BuildOutputs` against both existing
build trees, checking all five DLL paths and hashes against their original
manifests with frozen copies present. No DLL was rebuilt or replaced. The clean
consumer source is 14 commits ahead of remote main and remains local/unpushed.

**Offline preparation is complete; Sonic coordination is paused for an
owner-authorized game-only loader/lifecycle slot.** The reviewed consumer plan
above defines that check and rollback. Source access is resolved. Actual proxy
loading, lifecycle behavior, safe game-thread producer delivery and verified
local-player/driving/mode fields remain unverified; the field-free diagnostic
cannot establish all of them. Wheel/pedal/hat acceptance, virtual-pad output and
physical FFB remain separate. No additional offline assignment, consumer repin,
official toolkit release, game installation or launch follows from this handoff.

## Initial evidence

The coordinator read the installed Steam manifest on 2026-09-28:

| Item | Observed value |
|---|---|
| Steam app | `2486820`, Sonic Racing: CrossWorlds |
| Installed build | `23540645`, manifest StateFlags `4` |
| Install root | `D:\Program Files (x86)\Steam\steamapps\common\SonicRacingCrossWorlds` |
| Shipping executable | `UNION\Binaries\Win64\SonicRacingCrossWorldsSteam.exe` |
| Root bootstrapper | `SonicRacingCrossWorlds.exe` |

The implementation task reported the shipping executable SHA-256 as
`923FFDEA7265FE20736728AD2045278EAC46ED755D6503C81DDFF3FFFCF7521A`.
It found an Unreal-style Engine/UNION/Paks layout and no existing mod directory;
exact engine version, loader compatibility and input/telemetry hooks remain
unverified at this checkpoint. The
[official Steam listing](https://store.steampowered.com/app/2486820/Sonic_Racing_CrossWorlds/)
states Denuvo Anti-tamper; that does not establish a separate anti-cheat driver.

The [SEGA manual](https://manual.sega.jp/sonicracingcrossworlds/en/) embeds its
content in public `pC.push` data despite showing a browser-support shell to the
page reader. Its Steam tables assign steering, air tricks and flight movement
to the left stick across the listed layouts. Acceleration bindings differ by
layout and brake/reverse uses a button. These are manual observations, not
verification of the owner's bindings or this build's analog behavior.

Keep the requested right-stick mapping. Reconcile the actual actions and add
independent flight vertical control where needed; do not assume that right-stick
remapping alone makes flight usable or that analog pedal readings imply
proportional game acceleration/braking.

## Acceptance priorities

1. Stable wheel/pedal/button/POV identities, useful calibration, save/rebind/cancel
   and disconnect recovery. Preserve steering while using digital stick controls.
2. Four bindable stick directions with bounded diagonals, a defined opposing-input
   rule and neutral on release/focus loss/disconnect/cancel. Verify held versus
   fresh-press/flick semantics; no automatic trick repetition by assumption.
3. Real game-state telemetry with units, frames, timestamps, validity, freshness
   and field provenance. Identify land/water/flight and ownership transitions.
   Label derived fields and leave unavailable RPM/gears/contact data unavailable.
4. FFB based on verified game signals. Distinguish calculation, driver acceptance
   and physical feel. Any fallback must be explicit; controller rumble alone is
   not proof of useful steering-wheel FFB. Stop output on inactive/stale state.
5. Simple setup by default, Advanced tuning, bounded binding groups, compact
   commands and a device dropdown that follows Steering unless explicitly
   overridden. Apply the current toolkit UX guidance and document real exceptions.

Use the official [v0.15.0 artifacts and provenance](../RELEASE-0.15.0.md) where
applicable. The toolkit root has unrelated dirty work and is not a build source
for that frozen release. It does not provide a shared input/UI implementation.
No game physics changes belong here.

The prior shared-desktop pause remains in effect. The task can research and
build offline; game launch, input injection and force testing need a coordinated
slot. No game/device was opened for this kickoff. Preserve installed files and
owner settings; do not bypass DRM/anti-cheat or redistribute game assets.
