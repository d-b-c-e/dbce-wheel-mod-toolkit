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
Woden's accepted Kenya reference; DRIVE session-8 adds a separately labelled
diagnostic capture/cold playback/Stop, including sealed presentation values.
Their acceptance scopes and current reference
availability are in `docs/RECORDING-PLAYBACK-RUNBOOK.md`; implementation failures
and fixes are in `knowledge/RECORDING-PLAYBACK-LESSONS.md`. Shared contract:
`docs/PLAYBACK-CORE.md`. A game adapter still requires its own live qualification.

October 5 continuation: iRacing's new diagnostic reference (`802a6fe` runtime)
passed 3,001 poses and original-force analysis with native output muted; it is
separate from its lost owner reference and has no new rendered acceptance.

## STD-013 — The installed build launches plainly
2026-10-05 · owner ("always install the latest to my machine so that I can easily launch from steam or Stream Deck without needing extra parameters if possible") · applies to: every game

Keep the latest testable build installed where the owner launches it (Steam folder
or the Stream Deck button's target, after a backup, per STD-010), and make it work
from a plain Steam or Stream Deck launch: no launch options, dev scripts or env
vars. Features turn themselves on from what the mod can see (for example triples
`Mode = Auto` on a three-wide desktop; Sonic's SonicTriple adds `-emulatestereo`
itself). When a plain launch is not possible, or an install conflicts with a safety
rule (UE4SS left loaded for online play), say so to the owner rather than leave a
manual step undocumented.

## STD-014 — Request reciprocal review when progress stalls
2026-10-05 · owner · applies to: every repo, Codex and Claude

When an agent stops making meaningful forward progress, request a focused review
from the other agent before repeating the same approach. Share the current source,
exact failure, evidence and hypotheses already tried. Ask the reviewer to challenge
the approach and propose a different direction or a test that distinguishes the
remaining explanations. This works both ways: Codex reviews Claude's stalled work
and Claude reviews Codex's. Use the established portfolio inbox directly; do not
require the owner to relay messages. Record whether review was requested or
actually received, and keep useful independent work moving while awaiting it.
The normal rig lease, owner-input and restoration rules still apply.

First request: `dbce-project-mgmt/inbox/2026-10-05-peer-review.md` asks Claude to
review Sonic's stalled native-menu dispatch and the alternative working virtual-pad
startup route. The request is not evidence that the review or gameplay replay passed.

## STD-015 — Triples on Surround and on separate monitors
2026-10-05 · owner ("our goal is to support both surround and separate triple monitors") · applies to: every game with triple screens (extends STD-007)

Support both topologies: one wide Surround (or borderless-span) window per STD-007, and three
separate Windows displays without Surround (the owner's everyday "Sim Racing" layout: three
2560x1440 screens at x = -2560, 0, 2560). A mod picks the separate-display path from what it
can see (three equal side-by-side displays, no wide window), per STD-013, and keeps the same
projection, panel-angle and HUD-on-centre rules. Engine routes differ: Unity
`Display.displays[i].Activate()` with one camera per display (art of rally
`SeparateDisplayRenderDriver`; side views need the centre's colour grading and their windows
must come back after Alt+Tab), SDL borderless windows per display (F-Zero). Verify both
topologies at the rig before calling triples done for a game.

## STD-016 — Telemetry: Forza Horizon layout, on by default
2026-10-05 · owner ("If it's our telemetry we should be using the Forza 5 protocol") · applies to: every game with Forza-format telemetry

Mods send the toolkit default `ForzaPacket.Write` layout, `ForzaLayout.Horizon324` (the 324-byte
Forza Horizon 4/5 "Dash" packet; SimHub game: Forza Horizon 5), to 127.0.0.1:8000, on by
default with an owner switch to turn it off (STD-013). Other layouts (FM7 311-byte) only where
a game's own established receiver needs them, documented in that game's repo. Speed follows
STD-009.

## STD-017 — Hide settings pages that have nothing to offer
2026-10-05 · owner ("if we simply don't offer options in a game - e.g. the game already has in game bonnet and bumper, etc, lets just hide the tab rather than show it and not have any options") · applies to: every mod's F6 panel (refines STD-004)

A page appears only when the mod offers at least one setting or action on it in this game. If
the game already provides something itself (bonnet/bumper cameras, its own bindings) and the
mod adds nothing, hide that tab rather than show an empty or "not available in this game" page.
The remaining core pages keep their names and relative order (Controls, FFB, Cameras,
Telemetry, Help); Help always stays (version, support file). Decide visibility from what the
build supports, not from runtime state such as a disconnected wheel: an FFB page whose wheel
is missing stays visible with its status. A saved page that is hidden opens the first visible
page instead (no index shifting, per the page-migration rule in `docs/CONSUMER-SETTINGS-VIEWS.md`).

## STD-018 — Handling changes never reach online scores
2026-10-05 · owner ("we just don't submit scores online when that part of the mod is on") · applies to: every game with online leaderboards or scored online play

Input mapping (curves, deadzones, raw axes, shifters) is the player's controller and leaves scores
alone. Anything that changes the car's physics or grip (grip dials, brake-grip fixes, assists the
game does not offer) blocks the game's own online score uploads while it is in effect, and for the
whole stage once it has been on, even if undone before the finish. The block is in the game's upload
path (the call returns "not uploaded"), shown in the panel ("Online times: not uploaded (...)") and
logged. If the block cannot be installed, the handling change stays off. Offline play is unaffected.
Reference: DRIVE `Handling` (2bafbd2).

## STD-019 — Menus on the centre screen; side screens only in gameplay
2026-10-05 · owner ("putting menu on center screen and only really rendering the other screens during gameplay makes sense") · applies to: every game with triple screens (refines STD-007, both STD-015 topologies)

On a three-screen layout, front-end menus, loading screens and the mod's own F6 panel sit on the
centre screen at a single-screen size and aspect, with nothing stretched across the width. The side
screens stay black outside gameplay. Triple views (side cameras, off-axis centre) switch on only once
the player's car is in gameplay, and the in-game HUD and pause menu stay on the centre screen (STD-007).
Leaving gameplay returns to the centre-only menu. A game whose menus already behave like this needs
no change; record per game whether menus, loading and the mod panel are centred.

## STD-020 — Standard feature checklist per game
2026-10-05 · owner ("make a check list of features that should be standard for each game ... adding more detailed columns to the grid") · applies to: every game

Every game is tracked against the same feature list, as columns of the portfolio grid
(`dbce-project-mgmt/PROJECTS.md`, "Feature checklist") and in its `TOOLKIT-ADOPTION.md`. A
feature the game already does well natively counts as done (🎮); one that cannot exist in that
game is "—". Codes:

| Code | Feature | Done means |
|---|---|---|
| STR | Direct steering | The wheel axis reaches the car without the game's gamepad deadzone, curve or smoothing |
| PED | Analog pedals | Throttle and brake follow the pedal (no on/off thresholds or squaring the player cannot turn off); braking does not cost grip a light touch would not |
| HB | Analog handbrake | Lever position sets handbrake strength |
| DEV | Separate devices | Pedals, handbrake and shifter can each be on their own USB device, bound in the mod |
| H | H-pattern shifter | Gates select gears; out of gear is neutral; reverse works |
| SEQ | Sequential shifter | Up/down on any device |
| GLY | Wheel button prompts | In-game prompts show the wheel's buttons, not gamepad/keyboard glyphs |
| FFB | Mod FFB for any wheel | Forces on any DirectInput FFB wheel (not only the game's brand), F8 on/off (STD-004) |
| NRM | Normalized FFB | Strength 50% feels the same as in every other mod (STD-003) |
| TEL | Telemetry | Forza Horizon layout, on by default, HUD-matched, including the start countdown (STD-009, STD-016) |
| SUR | Surround triples | Angle-correct triples on one Surround window (STD-007) |
| SPN | Span triples | On separate monitors without Surround, the mod creates the borderless span on a plain launch (STD-015) |
| DSP | Per-display triples | One view per separate display, where the engine allows (STD-015) |
| MNU | Centred menus | Menus, loading and the mod panel on the centre screen, sides black outside gameplay, HUD centred (STD-019) |
| CAM | Camera keys | Numpad camera layout and step settings (STD-005, STD-006) |
| F6 | Settings panel | F6 panel with the shared pages, empty tabs hidden (STD-004, STD-017) |
| PLN | Plain launch | Works from Steam/Stream Deck with no parameters (STD-013) |
| ONL | Online-safe | Handling changes never reach online scores (STD-018); "—" when the game has no online scoring |
| REC | Recording | A recorded session per game (STD-012) |
| PLY | Playback | Recorded route plays back for automated tests (STD-012) |
| INS | Installer | Player installer with backups, settings kept, clean uninstall |

Status marks follow the grid legend (✅ owner-accepted, 🧪 built and installed waiting for the rig,
🔧 partial, 🎮 native, ❌ missing, — not applicable, ❔ not yet checked). A game is release-ready
when every applicable column is ✅ or 🎮, or the release notes name the gap.

## STD-021 — art of rally is the FFB reference
2026-10-06 · owner ("I want to treat Art of Rally as our gold standard for FFB values. So a crash or other FFB values in other games should approximately line up with Art of Rally if both games are at 50%") · applies to: every game with FFB

Refines STD-003. With both games at Strength 50, each game's forces should come out at about
the level art of rally's do for the same kind of event: steering load in comparable cornering,
crash cues, landings and surface texture. The reference is art of rally 0.4.1 at its defaults
(Strength 50; crash cue = 120 ms push plus 25 Hz rattle at half the crash strength). art of
rally is not retuned to meet other games. Compare per event type and speed band with the
toolkit analyzer (`docs/FFB-NORMALIZATION.md`), not whole-drive averages; an effect a game
does not have is not invented to match. The owner's feel at the rig is the acceptance; offline
matches are evidence. Normalization work is Codex's.

## STD-022 — One triple-screen selector: Off · Surround · Separate monitors
2026-10-06 · owner ("it should just be a simple off / surround / separate like the other projects") · applies to: every game with triples

The settings panel offers triple screens as one three-way choice, in this order: **Off** (one
view, and no span), **Surround** (three views when the game window spans a Surround desktop),
**Separate monitors** (the mod opens one borderless window across three side-by-side monitors,
STD-015, then the same three views). Show the current status under it, and say when a choice
applies at the next game start. Rig measurements, HUD centring and FOV options sit below the
selector; no separate Auto/On/Off radio or extra span toggle. Mods without an in-game panel use
the same three names in their config. Reference: iRacing Arcade `TripleLayoutChoice`
(`SpanPolicy.cs`, repack.117); art of rally's "Off / Single wide display / Three separate
displays" is the same choice with older labels.
