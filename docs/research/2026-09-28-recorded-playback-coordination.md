# Recorded playback coordination — 2026-09-28

The owner authorized shared playback work with **F-Zero Deluxe**, followed by
preparing **Woden Rally Edge** for their next seated recording. Ongoing two-way
coordination is authorized. The hourly `recorded-playback-and-woden-readiness`
heartbeat is recovery for this work; pause it when Woden's reviewed capture build
and clear handoff are ready, or a confirmed blocker needs an owner choice.

| Owner | Local task | Scope |
|---|---|---|
| Toolkit coordinator | `01a07ac3-e639-7762-a698-5365d447ee7e` | Shared case/observation/comparison contract and review |
| F-Zero Deluxe | `01a0c484-39ed-7242-9865-e067680baff2` | `.fzpt` inputs, machine-state sidecar, game replay and pure force adapter |
| Woden Rally Edge | `01a0a2bb-31fb-7cf2-997c-03c5d4c62176` | Recording preparation, game/model capture, offline reprocessing, package/install |

Toolkit work uses `codex/recorded-playback` in
`C:\Users\antho\.codex\worktrees\recorded-playback\dbce-wheel-mod-toolkit`.
Preserve the root dirty work, native POV/external-session worktree and the separate
F-Zero-owned `E:\Source\dbce-wheel-mod-toolkit-actuator-fix` checkout.
The existing unpublished `Dbce.Wheel.Recording` extension remains separately
pinned by Woden; these source-only tools neither replace nor release it.

## Boundary and evidence

[v1 contract and reference APIs](../RECORDED-PLAYBACK.md). Toolkit owns artifact
identity, ordered force-observation files and comparison. Consumers own model
signals, game execution/input codecs/checkpoints and restoration. No commands
are executed from a case manifest. The toolkit sink supports only observation.
No ROMs or real snapshots are committed; all toolkit tests use synthetic bytes.

Cruis'n read-only review: `harness/session_case.py`, `harness/ffb_worker.py` and
`docs/DIAGNOSTIC-REPLAY.md` retain MAME INP, initial configuration, actual worker
observation, frame/clock identity and comparison evidence. That established
workflow remains consumer-owned and unchanged. No Cruis'n adoption is claimed.

F-Zero reports a 2,000-frame synthetic replay and a separate 2,000-frame desktop
capture replay exactly against per-frame WRAM hashes. The coordinator has read
the adapter description, not independently reproduced those game runs. Its v1
`.fzpt` retains effective cartridge SHA, post-binding 32-bit SNES words and FNV64
WRAM hashes; `.state` is pre-frame-zero machine state. Pure `FzeroFfbCompute`
observations avoid native wheel initialization. F-Zero reviewed the shared draft
and found no blocking mismatch. Proposed integer timebase is 60098812 ticks/s
and 1000000 ticks/frame, an explicit nominal NTSC-rate approximation. It will
adapt normalized requests with coefficient/magnitude /10000, millihertz /1000
and actual event durations. Its independently authorized attended activity must
not be interrupted by this coordinator.

## Woden starting checkpoint

Clean private source HEAD `8b070eb8556f386c8bb539819aaf9c7d2cde2ad6`, branch
`codex/ux-simple-advanced-0.2.8`. Installed runtime 0.2.10 from
`25797b3f3cacb4a2ddaf671a7165af097abf4633`, installer revision 3/source `e5da2d5`.
Woden reports receipt/payloads and owner settings preserved. FFB On, strength
49.583332%, cap25%, load6000, slip.35, smoothing35ms, damping.05, invertfalse.
Do not reset bindings, camera/timer settings or unrelated triple-screen work.

Existing one-launch helper generates a 15-minute single-use request, with
physical force suppressed by default and explicit `-AttendedFfb` preserving
the normal gated setting. Capture is bounded to20min/64MiB; saved preferences
are untouched. Two previous files are structurally complete with zero drops,
but **zero driving samples**. Their validity is not useful driving coverage.
The old inspector summarizes files; it does not rerun the force model.

Woden's active implementation assignment:

1. Add request/case correlation and exact game/plugin/runtime/config/binding
   provenance; fix hard-coded recording pluginVersion0.2.8.
2. Capture explicit model reset/gate/validity events and all required actual
   model inputs/dt/tunes for sequential signal reprocessing.
3. Provide a durable simple command that creates an expiring request only when
   invoked, waits with bounds, never kills a game, correlates the output, fully
   validates it and prints the exact saved path/hash and driving coverage.
4. Add actual `ForceSignal` offline rerun and normalized observation output;
   refuse unsupported/missing/dropped captures and preserve unavailable values.
5. Build/test/package and present exact changes for coordinator review before
   closed-game deployment with backup, receipt and owner settings retained.

No deterministic Unity game replay is promised. Pause/focus/reset semantics must
be explicit; field presence and matching numeric fixtures alone are insufficient.
No real launch, input injection, wheel acquisition, virtual pad or physical force
is authorized by this coordination. First owner driving capture and physical
acceptance remain separate from source/package/deployment readiness.

Shared source checkpoint `87b47d53121c0b3fc8479e622ae8a79c8ca3fa58` is
clean/pushed and landed on private master via `8bd119c9d67e5e9e951c05394a062460ba13f141`.
Both consumers received the exact reference, including the final required
`caseSha256` header binding. Woden's first candidate reached preinstall review;
deployment has not been approved or performed.

## Woden preinstall review, 2026-09-29

Candidate source `fea6c996d89ed09a125e71524d4335a994078afc`, documentation
follow-up `b5e46c3`, product 0.2.11. Frozen ZIP
`WodenRallyEdgeWheel-0.2.11-dev.zip` SHA-256
`7600587155f9d45e16dc7c402127b3531017864213a991fff47c412f23d48e29`.
The coordinator independently verified the ZIP identity and all 14 manifest
payloads against both ZIP and stage, then ran 36 regression suites / 951
assertions. The task also reports zero-warning build, 532 UI assertions,
84 PowerShell installer checks and PrepareOnly fixtures. No game/device run.

The synthetic 100-sample actual-model case passes shared artifact validation and
self-comparison. Exact case hash
`9516dab277b506e7a94e161d0648fee2478e68a8126c44670741e9af66ac7129`;
observation hash
`8a54f18c803e3da34eb40d9511b89fd89fd83d579a0783c89712b3a4bc5d22ce`.
This establishes baseline signal reproduction, not real driving coverage.

Installation is held for two focused corrections. The adapter currently only
creates a verified baseline and refuses subsequent runs or changed tuning. Add a
separate trial command that verifies the unchanged original case/source/baseline,
uses a separately hashed candidate tune, and writes a new observation bound to
the original case hash. Also reject invalid identifiers, negative ticks and
structurally mismatched timelines/counts before calculating numeric deltas.
Regressions must cover a changed tune with original artifacts retained, tampered
inputs, malformed observations and actual-controller pause/camera/discontinuity
transitions. Preserve this first package and return a new immutable receipt.

Recorded preview is a model value, not delivered/measured torque. Default
force-disabled captures preserve the controller's gate resets, which reset
shaping on each blocked sample; trial documentation must state this limitation.
The installed game remained 0.2.10 at that first review checkpoint.

## Woden ready for the owner's next recording, 2026-09-29

The replacement **0.2.12** candidate closes both findings. Clean runtime source
`bf58042fbf6ebea9175d8a85f332dd1ffd203ea6`; package
`E:\Source\woden-rally-edge-wheel\dist\WodenRallyEdgeWheel-0.2.12-dev.zip`,
SHA-256 `09088883102cc212b8fd0119b09fe1440efb9e1bb2158b3a60666185df7d279c`.
The coordinator reviewed the trial/reader/controller fixtures, independently ran
37 suites / 965 assertions, and verified every one of the 14 manifest payloads
in ZIP and stage. The task also reports 532 UI assertions, 84 installer checks,
zero-warning build and PrepareOnly fixtures. A fresh coordinator-generated
synthetic case passed the shared reference: 77 of 100 magnitudes differ for a
20%-strength tune, maximum delta 0.14840785413980484, with original artifacts
preserved and the original case hash retained in the trial.

Installed with Woden closed at **2026-09-29T05:35:51.1480383Z**, installer
revision 3. The coordinator independently verified the installed receipt,
all nine payload hashes and all four byte-identical owner config/binding files.
Backup under the game: `WodenWheelBackups/before-install-20260929-003548-381c44f1`.
No capture request exists, game remains closed, and no device/force test occurred.
The original 0.2.11 candidate and native/Recording dependency pins are retained.

When seated, the owner can run this PowerShell command:

```powershell
& 'E:\Source\woden-rally-edge-wheel\tools\Start-RecordedGame.ps1' -AttendedFfb
```

It creates the expiring request at invocation, launches the attended session,
waits for normal exit and reports the validated capture/case/observation paths.
Drive with stable FFB tuning, then quit normally. Without `-AttendedFfb`, force
is suppressed and shaping history has the limitation described above. First
real driving capture, startup/title guard interaction and physical feel remain
unverified. This is capture readiness, not completed driving acceptance.

F-Zero then reported committed recorder `4784b3f` and an 11,364-frame real
playthrough replay with all WRAM checkpoints matching. Reproduction requires
its recorded `FZERO_ASPECT=16:9`; 4:3 diverges at race entry. The consumer is
assigned hashed effective aspect/MSU/gameplay configuration artifacts and adapter
enforcement. This is consumer-reported game evidence, not a toolkit test result.

F-Zero source `0fe1e6452de2b182300ccb72e96080d9054f38ba` now also supports
SDL/Surround replay with physical FFB initialization and SRAM writes disabled.
It rejects mutation hotkeys/state scripts during replay. The consumer reports
all 11,364 WRAM checkpoints matching in both headless and SDL paths. Coordinator
source review confirms these guards, but does not independently establish that
game-run result or physical output. The shared case/config adapter is still open.

Separate native actuator commits `43c5e3c`, `6b6e29b`, `ca24f1e` received
[source and x86/x64 fake-test review](2026-09-29-actuator-rearm-review.md).
Repairs `44ca79d` and `02fb715` close shaped retrigger, shared-device reacquire,
open/reopen coverage and coefficient-threshold findings. Coordinator tests pass
on both architectures. An immutable component receipt is requested; these fixes
remain separate from playback and every frozen candidate. F-Zero still owns the
remaining shared case/config adapter; Woden capture readiness is complete.

Latest compact cursors:
F-Zero `166a5771-74fa-4c61-bf87-f03dd911821b:12`;
Woden `5365458d-4949-42ac-a750-938f7b10372d:10`.

## F-Zero adapter checkpoint review, 2026-09-29

Consumer source `7a585a2` follows pure-model raw exporter `0e3ac07`. It consumes
the shared Python implementation directly, retaining game-owned `.fzpt` and
snapshot codecs. The clock now uses actual emulated master cycles at 21,477,272
ticks/second, superseding the earlier nominal frame-rate proposal. The explicit
profile assumes spring, damper and road slots, zero constant-force fallback,
and 140 ms / 32 Hz impact events. These are model requests, not physical torque.

The consumer reports two full 11,364-frame replays with matching WRAM checks at
strengths 12 and 20. The coordinator did not launch either replay. Independent
read-only validation accepts the original case hash
`aaec60f5e370734536820accc262c0a9c7a268fdc19d594f90e63db8332720ce`.
Shared comparison validates both complete 45,461-request streams on the same
timeline and case identity, with 29,083 changed requests, maximum magnitude
delta `0.08000000000000002`, RMS `0.042417871390897186`. Three ROM-free adapter
tests passed independently; the task additionally reports 13 CTest cases.

Adapter acceptance remains open for one concrete source finding: the manifest
hashes an arbitrary `initialState`, while the game loads the source recording's
`.state` sibling. Require those paths to identify the same validated artifact,
or stage the validated pair explicitly. The owning task is assigned a regression
that rejects a different manifest state before launching the runner. It is also
asked to confirm actual capture executable/source provenance; `create` currently
asserts `dirty:false` from caller input without independently checking a receipt.
Preserve the original case and both observations while making the repair.

The separate experimental native receipt committed at `ea25ea8` is reviewed;
source/tree and both DLL hashes, including the preview copy, match. See the
[actuator review](2026-09-29-actuator-rearm-review.md). No game/device/force was
opened by the coordinator. Woden readiness remains complete and its task idle.

## Shared contribution and readiness complete

Clean F-Zero repair `d84c60df7f91b16333b15ca139fd53e74c83bbd8`, tree
`b9b21f4122781789cc6def9beb154da6f55d4238`, closes the snapshot finding. Both
create and observe require the exact sibling the runner loads; a ROM-free test
rejects a validly hashed different manifest state before any process launch.
The coordinator reviewed the diff and independently passed all four adapter
tests. The original case and observations remain unchanged:

- Strength 12 observation: `097CD6B9B5A9111743142C39FE7949817841DF3877FD27CA0CCC37B4D9E78DFA`.
- Strength 20 observation: `EF6B7C8A598E082792C7954418E51DD8FFAE42EC0A0EF76893E3BD9532203CAC`.

F-Zero documents capture executable/source separately from the headless runner.
The existing `dirty:false` is its prior build-time assertion, not retroactive
binary attestation. Future cases default to `dirty:true` unless a matching
build-time receipt identifies source/tree and executable; this checks consistency,
not a cryptographic proof of the build. The adapter source and native receipt are
accepted for offline use with these limits.

The requested shared-playback contribution and Woden capture preparation are
complete. The coordination heartbeat is **paused**; Woden stays idle for the
owner's seated command above. No recording request is left running. First Woden
driving capture and physical output/feel acceptance remain future owner work.
