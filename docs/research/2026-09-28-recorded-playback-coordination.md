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
`caseSha256` header binding. Woden is implementing and testing its adapter and
owner command; deployment has not yet been reviewed or performed.

F-Zero then reported committed recorder `4784b3f` and an 11,364-frame real
playthrough replay with all WRAM checkpoints matching. Reproduction requires
its recorded `FZERO_ASPECT=16:9`; 4:3 diverges at race entry. The consumer is
assigned hashed effective aspect/MSU/gameplay configuration artifacts and adapter
enforcement. This is consumer-reported game evidence, not a toolkit test result.

Separate native actuator commits `43c5e3c`, `6b6e29b`, `ca24f1e` were reported
by F-Zero for future source/regression review. They are not part of this playback
contribution or any frozen official/POV/external-session candidate. Preserve the
actuator worktree; no native adoption or physical acceptance is inferred.

Latest compact cursors:
F-Zero `166a5771-74fa-4c61-bf87-f03dd911821b:10`;
Woden `5365458d-4949-42ac-a750-938f7b10372d:5`.
