# Recording and playback: failures, fixes and acceptance

Provenance: [dbce-mods-art-of-rally, 2026-10-04], reviewed prototype
`c700c6bb0e7790353d533132b11ee5aa2536a457`, accepted runtime RC10
`dcacff43595101bb293714b86b272afc8e284f08`; shared playback 0.2 source
`50b6b679e3c6e01025b6a50f8dd0f3eb85d9d4bb`. This describes implementation
mistakes and evidence, not an evaluation of a particular AI model.

Start with [the operational runbook](../docs/RECORDING-PLAYBACK-RUNBOOK.md).
The [core contract](../docs/PLAYBACK-CORE.md) documents reusable APIs. Do not
copy Art's entire hook class into another game.

## The three capabilities must remain distinct

| Capability | What runs | What it can establish |
|---|---|---|
| Trajectory playback | Recorded body poses and presentation in the game | Route/camera reproduction and adapter lifecycle |
| Signal reprocessing | Force model against original captured physics | Arithmetic, candidate tune changes, comparable labelled signal statistics |
| Input diagnostic | Recorded controls through live game physics | Measured divergence; determinism only if independently established |

Kinematic pose playback does not recreate tyre contact forces. Input replay is
not made deterministic by setting a random seed or teleporting back to the route.
Native API acceptance is not measured torque. [Art/toolkit, 2026-10-04]

## What failed and why

1. **Rendered-frame controls were used to recreate physics.** Render rate,
   multiple fixed updates per frame, initial state and internal solver state
   differed. The car drifted off the route almost immediately. Moving car input
   to a fixed-update prefix was necessary but did not prove deterministic physics.
   Use physics-tick trajectory playback for a visual reference. Keep strict input
   diagnostics separate and fail on divergence; never silently correct a failing
   physics replay into a pass. [Art, reviewed c700c6b]

2. **Pose correction concealed failure.** An older receipt said passed after
   173 corrections, 430 gear sets and 4.22 m maximum pose error. That was assisted
   viewing, not accurate input replay. Completion must distinguish finished,
   failed and aborted, consume all required driving rows, and verify the final
   state. Forced gear changes are presentation evidence only. [Art review]

3. **The playback body had multiple writers.** The first trajectory version
   set a kinematic pose but the game cleared kinematic mode and continued running
   `Wheel`, `CarDynamics` and out-of-bounds/reset logic. The owner saw a flying
   camera and repeated stops/restarts. Owning the transform without owning the
   simulation lifecycle was insufficient. Guard those game-specific paths only
   while playback owns the active player body, retain original flags, and restore
   them on every exit. Art failed again at about 115 seconds with 0.516 m error
   before the remaining wheel-physics path was guarded. [Art replay170002/171011]

4. **Checking immediately after a write was too weak.** A setter can report the
   requested pose even though another component will overwrite it later. Verify
   on the following physics callback, including the last frame. Shared 0.2
   verifies position within 2 cm and rotation within 0.2 degrees. Art's reported
   application metric is position; do not claim its result covers the shared
   rotation contract. Also observe the camera/HUD: perfect pose metrics alone
   do not prove a usable rendered replay. [Art/shared 0.2]

5. **Timing alignment could skip required driving segments.** Matching only a
   repeated state name or treating an exhausted menu stream as success allowed
   missing driving rows. Separate render/menu and physics streams; track ordered
   occurrences, allow only declared transient non-driving skips, bound waiting,
   and require both driving completion and the recorded terminal state. Resolve
   and verify actual stage, car, weather and game build at load. [Art review]

6. **Muting output also risked destroying the experiment.** Disabling the whole
   FFB calculation path can produce an empty/zero force capture. Record original
   solver inputs, tune, calculated requests, requested effects and encoded packets
   before suppressing delivery. Latch wheel/network/result suppression for the
   process, including after abort, then restart to restore ordinary output.
   A separately enabled analysis model must be labelled hypothetical, distinct
   from actual output. Missing game channels remain unavailable, never zero.
   [Owner correction; Art/shared adapters]

7. **Storage and cleanup were treated as incidental.** Main-thread writes,
   unfinished buffers, stale requests and partial teardown can create misleading
   captures or unexpected later playback. Use exclusive output directories,
   bounded background queues, explicit failure on drops/errors, close writers
   before sealing hashes, finite startup/response deadlines, and matching request
   IDs where supported. Shared 0.2 has identified requests; Art's older launcher
   uses an expiring one-launch request and has not adopted that entire protocol.
   Heartbeat cleanup must run even when pause stops physics callbacks. Attempt
   every cleanup action even when another cleanup fails. [Art review/shared 0.2]

8. **A synthetic capture was confused with the reference drive.** At 20:40 local,
   a 12-second fixture drove off road using synthetic steering/throttle. Its
   playback correctly reproduced that poor trajectory, but presenting it to the
   owner as the main visual check looked like a regression. Use the owner-approved
   route first. Label synthetic data and explain its purpose: writer/reader and
   fresh-source-signal checks, not quality of the racing line. [Codex, 2026-10-04]

9. **A display test introduced an unrelated failure.** A temporary 1280x720 test
   wrote Art's `SETTINGS_FULLSCREEN=0`, incorrectly treating it as Boolean false.
   The actual enum is 0 exclusive, 1 borderless, 2 maximized, 3 windowed. Exclusive
   switching failed and fell back to native resolution, flashing/moving windows.
   The override was removed. Preserve the current rig presentation separately
   from recorded stage/car preferences; never guess display enum semantics or
   repeatedly change resolution to validate playback. [Codex/game code, Oct 4]

10. **A graphics incident was at risk of being over-diagnosed.** NVIDIA 153 and
    LiveKernelEvent141 reports overlapped failed captures and concurrent Woden
    work. The owner later hard-restarted after garbling/lost KBM. The trigger is
    unresolved; neither the probe hooks nor a particular game is proven causal.
    Preserve logs/times, serialize live games, stop on a new failure, and avoid
    turning correlation into a root-cause claim. A generated DLL was later found
    zero-filled; rebuilding recovered its prior hash. Check managed metadata as
    well as provenance/hash before vendoring. [Oct 4 incident record]

11. **A successful backup command did not prove a complete backup.** Windows
    `reg export` returned success while omitting 21 of Art's 58 preference values,
    including fullscreen and Rewired maps. The root cause of the omission is
    unknown. Saving/restoring the partial export could therefore lose owner state
    or repeat an unwanted display switch. New backups enumerate all value names,
    preserve raw registry types/bytes, hash the snapshot, validate count/names and
    read back every restored value. Invalid raw backups fail before file writes.
    Keep legacy limitations explicit and current presentation separate from the
    recorded scenario. [Art final source 30d421b; 58-value live roundtrip]

12. **Stage arming was mistaken for unattended playback.** iRacing's first
    adapter required the owner to choose Free Play and timed out at menus. A
    replacement still stalled at login because no local player was joined.
    The accepted flow uses native PlayerInputManager.JoinPlayer once when joining
    is permitted, waits for existing Steam authentication, queues offline Free
    Play, validates a sealed track/car setup, starts, replays, quits and restores.
    An accepted replay must work from a closed game. Never make the user provide
    the missing startup half of an advertised unattended workflow.

13. **Recording/playback phases were not clear enough.** The owner thought a
    replay was still a recording and paused/switched away. Add a visible mode
    banner and a distinct replay launch. Do not silently switch mid-drive. The
    pause/focus abort was expected behavior, not evidence that trajectory replay
    drifted. Keep physical outputs muted and original producer data available.

14. **A writer/reader fixture did not validate the real analyzer contract.**
    iRacing's actual 3,001-row stage source lacked input/context diagnostics the
    older analyzer required. Add an explicit stage profile that still validates
    required force inputs, alignment, clocks and delivery suppression. Report
    uncaptured diagnostics as unavailable. The actual source then passed with
    zero model error; do not invent zero-valued observations to satisfy a schema.

15. **The supervisor is part of the feature.** A status-file sharing race killed
    the first runner even though the game passed/quit. Treat atomic replacement
    gaps as transient and use ReadWrite/Delete sharing; retain exact request-ID
    correlation. A repeat passed with automatic restoration. Stop must fail an
    incomplete replay, release ownership, close normally and preserve evidence.

16. **Restoring the pre-test display state can preserve the wrong baseline.**
    The iRacing tests reused an already small window and restored it exactly;
    the owner expected their normal triple-screen setup. Validate the intended
    presentation baseline, not just backup equality. Ordinary driving and replay
    should share the same launch settings. Do not change resolutions for tests,
    restart Steam under another session, or rerun accepted routes just to finish
    paperwork. Steam's persistent borderless options were prepared separately;
    application remains pending until the client can close without interruption.

17. **A healthy old signal capture can still lack a replayable scenario.**
    Woden's original 116-second drive retains 6,981 continuous poses and the
    original force data, but lacks scene/track/setup identity. Hash-check and
    inspect it without rewriting the source or inventing a scene. New captures
    must seal observed stage context at capture time. Native in-memory replay
    objects may help presentation, but are not proof of durable file playback,
    cold startup or preservation of original tire/contact telemetry. Woden's
    new context sidecar and mode banner were subsequently installed and the
    Kenya reference passed; see the runbook for the exact candidate.

18. **Share normal eligibility instead of copying a weaker predicate.**
    Woden's first stage adapter duplicated driving checks but omitted photo mode,
    the native car lock and native replay during continuous playback. Reuse the
    game's established driving policy, excluding only the adapter's own replay
    ownership where necessary. Keep that ownership a force/input exclusion in
    every normal path; native replay takeover must still abort. Build and policy
    tests support the change, but do not certify actual native hook behavior.

19. **A Git merge does not preserve ignored private evidence.** Consolidation
    removed iRacing's old worktree and its accepted reference files. The source
    and committed report survived, the recording did not. Before removing any
    worktree, inventory recordings/results/dependency caches, copy the complete
    sealed reference to a stable private location, verify every sealed hash,
    then archive the remaining evidence with a file inventory. Never use
    `git worktree remove --force` as a substitute for that preservation step.
    Woden's accepted reference now lives under LocalAppData/Dbce/StagePlayback/
    references. Missing evidence must remain visibly missing in operator docs.

20. **Record the origin of control separately from the origin of motion.** An
    automated diagnostic can exercise real game physics and capture useful
    original signals. Label it `automated-diagnostic-v1`, preserve that label in
    metadata/review output, and never substitute it for an accepted owner drive.
    Run playback in a separate process. Stock haptics/gamepad vibration can be
    separate producers from wheel FFB; observe requests before suppressing each
    actual delivery path. Keep absent signed force models explicitly unavailable.

## Exact final release

Art 0.4.0 was published and installed from clean source
`30d421b26eccebc564cd50bf0733c1ed6a82d849` after all 20 local gates.
Final replay `replay-20261004-210753` completed all 13,455 driving/finish poses,
zero detected position error, finish/results/menu and normal close. All 58 game
preferences and saved files were restored. Startup reported native 2560x1440
FullScreenWindow without the failed exclusive-mode switch. Both published ZIPs
and checksum files were independently downloaded and matched to tested assets.
The runbook records commands and hashes. This does not resolve the GPU incident
or certify all hardware. iRacing subsequently passed the scoped cold-launch
reference described in the runbook. Woden subsequently passed its Kenya
reference; DRIVE still needs live qualification.

## Earlier evidence that passed

- Art owner session 5, `replay-20261004-173553`: 13,455 driving/finish poses,
  zero detected position application error, automatic finish/results/menu.
- Same immutable recording, RC10 `replay-20261004-204543`: same 13,455 poses,
  zero detected position error, normal close/restoration. Owner: "Yes, this
  looks right", then "everything looks great" and requested release.
- Fresh synthetic roundtrip7: 721 force samples, 1,210 encoded Forza packets,
  two effect requests, three collision entries; 26,665 offline assertions,
  zero force delta/native-magnitude mismatch. Its trajectory replay applied 721
  driving poses with zero position error. This is synthetic source evidence.
- Physical output was muted. No torque, cross-game calibration or broad
  hardware/layout certification follows from these results.
- Woden, DRIVE and iRacing candidate builds/package checks passed; at this
  checkpoint none of their new trajectory adapters had run in-game.

Private recordings/logs remain in the consumer workspace under `results/sessions`.
Public source explanations: Art `docs/reviews/2026-10-04-session-replay-review.md`,
`2026-10-04-playback-validation.md`, and `2026-10-04-graphics-hang.md` on
`codex/session-playback` (or release tag after publication).

## Acceptance before claiming another game works

1. Fingerprint the installed game and exact adapter/core payloads. Back up owner
   settings. Preserve concurrent development work; only one game owns the rig.
2. Select offline/unranked eligibility. Verify actual hook counters, one original
   signal sample per trajectory row and nonempty meaningful force/telemetry data.
3. Capture a labelled real drive, stop/save, verify every seal and required stream.
4. Replay that exact case; check next-tick position/rotation, camera, HUD, scene,
   final frame and result/progression suppression. A menu-only run is not a drive.
5. Exercise Stop/F12, pause, focus loss, scene change, retry and normal exit.
   Verify ownership restoration and continued physical-output suppression.
6. Run a complete reference route; keep short, full-route, rendered and physical
   evidence separate. Record failures as well as passes with exact source hashes.
7. Qualify the exact installed archive. Do not promote a build-only adapter,
   incomplete capture, disabled signal producer, or corrected-input replay as a
   validated reusable system. Leave untested cases explicitly open.
