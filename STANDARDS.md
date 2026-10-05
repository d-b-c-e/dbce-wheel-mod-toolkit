# Standards ledger

Family-wide decisions every game mod should bring in. This is the announcement
channel between agents: whoever sets or changes a standard appends an entry here
**and commits it in the same turn**; each game agent compares this file with its
repo's `TOOLKIT-ADOPTION.md` at session start.

Rules:
- Append only. Never renumber or rewrite an entry; supersede it with a new one
  ("Supersedes STD-005") and mark the old one *Superseded by STD-0xx*.
- Each entry says what to do, who it applies to, where the details are, and a
  reference implementation when one exists.
- Details live in the linked guide; the entry is the one-line contract.
- Adoption is recorded in each game repo, not here. Portfolio rollup:
  `E:\Source\dbce-project-mgmt\tools\Get-StandardsBacklog.ps1`.
- Code and version pins are tracked separately by `tools/Get-ConsumerStatus.ps1`.

Statuses in a game's `TOOLKIT-ADOPTION.md`: `adopted` (with commit/branch),
`partial`, `pending`, `n/a` (with the reason), `unchecked` (nobody has looked yet).

---

## STD-001 — One mod per game
2026-09 · owner · applies to: every game

Wheel input, FFB, telemetry, cameras and triple screens ship as one mod and one
repo per game (`games\dbce-mods-<game>`). No parallel wheel/triple mods.
Reference: art of rally (wheel and triple mods unified 2026-10-04).

## STD-002 — Recording and playback from launch
2026-10-04 · owner · applies to: every game

*Superseded by STD-012 following the accepted Art, iRacing and Woden playbacks.*

Each game needs a recorded session that replays through the game itself, from
launch through the menus to the drive, as the base for automated testing.
Replay is timing-accurate input playback, not pose correction or teleports.
The owner asks for a recorded session; the agent arms the recorder, launches the
game, says how to exit and validates the capture. No in-mod record button.
Details: `docs/SESSION-RECORDING.md`, `docs/RECORDED-PLAYBACK.md` (not yet
committed as of 2026-10-04; Astra/Codex owns this work).

## STD-003 — Normalized FFB strength
2026-10-04 · owner · applies to: every game with FFB

Reuse the shared FFB logic. The strength setting is normalized across games: 50%
should feel about the same in every mod. Default 50%.

## STD-004 — Consistent settings UX
2026-09 · owner · applies to: every game

Same F6 settings panel shape across mods (Simple/Advanced views, same page names
and controls where the game allows), F8 FFB on/off. Details:
`docs/CONSUMER-EXPERIENCE.md`; full guides `docs/CONSUMER-UX.md`,
`docs/CONSUMER-SETTINGS-VIEWS.md` (not yet committed as of 2026-10-04).

## STD-005 — Camera numpad layout 8/2 9/3 4/6 7/1 +/− 5
2026-10-04 · owner · applies to: every game with mounted cameras

Numpad 8/2 forward/back, 9/3 up/down, 4/6 left/right, 7/1 tilt forward/back
(look down/up), +/− FOV, 5 reset. Untouched earlier default sets migrate on load;
customised sets are kept. Details: `docs/CONSUMER-CONTROLS-CAMERAS.md`.
Reference: Woden `CameraTuning.MigratePreviousDefaults` (branch
`claude/triple-screens`, 580a907). Supersedes iRacing's interim layout (fc35102).

## STD-006 — Camera step sizes are settings
2026-10-04 · owner · applies to: every game with mounted cameras

Move, tilt and FOV per press are player settings (default 0.02 m, 1°, 2°) in
Advanced → Cameras with a defaults button. Details:
`docs/CONSUMER-CONTROLS-CAMERAS.md`. Reference: Woden 2e32e46.

## STD-007 — Triple screens in one wide window
2026-10-04 · owner · applies to: every game with a 3D driving view

One window across the three screens (Surround, or a borderless span), split into
three viewports: centre is the game camera with an off-axis projection, sides
are yawed by the rig's panel angle, HUD and menus on the centre screen. `Mode =
Auto` turns it on when the window is three screens wide. Hide the game's own
pillarbox/letterbox when it leaves black bands on a three-wide window.
References: iRacing Arcade `TripleView` (fc35102), Woden `TripleView` +
`MenuBorders` (b89ce73).

## STD-008 — Display changes: let the game apply them once
2026-10-04 · owner (after a GPU reset and hard reboot) · applies to: every game

Never stack runtime resolution/fullscreen changes. If the game's resolution list
lacks the Surround desktop size, add it to the list so the game's own single
apply selects it, and set the saved setting before launch. Monitor profiles only
via the MonitorProfileSwitcher CLI with settle time. Run a display watchdog
(driver-reset events, stalled frames) during display tests. Reference: Woden
`DesktopResolution` (0b0f844), `tools/dev/Display-Watchdog.ps1`.

## STD-009 — Dashboard telemetry matches the HUD
2026-10-04 · owner · applies to: every game with Forza-format telemetry

Forza `Speed` follows the number the player sees on the HUD (physics speed only
as fallback). If the game has only a rev fraction, send RPM on a nominal scale
(stated in code and docs) rather than nothing; always send gear. Keep raw values
in the detailed stream. Reference: Woden `ForzaProjection` (0b0f844).

## STD-010 — Install the latest build for testing
2026-10-04 · owner · applies to: every game

When a build is ready for the owner to test, back up the current install and
install it to where the owner's Stream Deck button launches, without asking.

## STD-011 — Work lands on main
2026-10-04 · owner ("if we don't start merging things into main we're going to lose track") · applies to: every repo, every agent

`main` (or `master`) is the single source of truth. Start each task from it, on one
short-lived branch or worktree, and merge back into it as soon as the build and tests
pass, in the same session. Before ending a session: nothing uncommitted, merged branches
deleted, finished worktrees removed. What is *installed* is recorded in the repo's docs,
not kept on a separate branch. Never leave a standard, fix or experiment only on disk or
only on a side branch. Portfolio check: `E:\Source\dbce-project-mgmt\tools\Get-BranchDrift.ps1`.

## STD-012 — Reproduce the route and preserve original signals
2026-10-05 · owner acceptance and replay implementation findings · applies to: every game

Supersedes STD-002. Keep externally armed recording and unattended cold startup
through native menus. Record and replay are separate, clearly labelled launches.
Use physics-tick trajectory playback for repeatable route/camera reproduction;
retain original controls, physics, force requests and telemetry for analysis.
Verify each applied pose on the next physics tick, including the final pose, and
observe camera/HUD behavior separately. Input replay remains a diagnostic until
determinism is demonstrated; pose corrections cannot turn its divergence into a
passing physics test. Kinematic playback cannot regenerate the original tyre forces.

Mute physical/network delivery while retaining the original requested data and
units. Preserve saved display and owner settings. Keep complete sealed references
outside disposable worktrees and verify them before cleanup. Automated diagnostic
controls must be labelled separately from owner driving; both record real physics.

References: accepted Art 0.4.0 session 5, iRacing's two accepted cold launches and
Woden's accepted Kenya reference. Their acceptance scopes and current reference
availability are in `docs/RECORDING-PLAYBACK-RUNBOOK.md`; implementation failures
and fixes are in `knowledge/RECORDING-PLAYBACK-LESSONS.md`. Shared contract:
`docs/PLAYBACK-CORE.md`. A game adapter still requires its own live qualification.
