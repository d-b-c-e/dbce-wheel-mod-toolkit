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
