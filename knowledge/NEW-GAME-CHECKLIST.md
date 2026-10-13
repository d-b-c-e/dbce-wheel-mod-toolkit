# New game checklist — from "this game has no wheel support" to a release

The order that worked for four games. Skip nothing; every skipped step here
was a day lost somewhere else.

## 0. Decide it is worth doing

- Offline single-player, or an online mode you can gate. No anti-cheat.
- No DRM circumvention. No physics changes that touch leaderboards without an
  independent gate (iRacing Arcade's `FunModeGate`: online check, network
  check, scenario check, plus a results-blocker).
- Does the game already have FFB code that never runs? (art of rally did:
  half-built, never wired.) Read the assemblies before writing anything.

## 1. Fingerprint the engine and pick the template

`dbce-mod-toolkit`'s fingerprinter → Unity Mono / Unity IL2CPP / UE4 / native
/ emulator. Templates: `templates/unity-mono-umm`, `templates/unity-il2cpp-bepinex`,
`templates/native-dinput8-proxy`. Emulators: work in the driver (see `MAME.md`).
Read the matching knowledge page **before** the first hook.

## 2. Get a loader running and a log line out

Prove the load path with one log line and a `status:` heartbeat. Under IL2CPP,
pre-seed the BepInEx config and wait for interop generation. Log by default.

## 3. Find the car and sample it

Hook one per-tick method on the car; identify the local player's instance;
copy speed, steer, steer rate, lateral/longitudinal acceleration, per-wheel
slip and load (or lateral force), surface, gear, rpm, collision flags into a
plain `TelemetryFrame`/`CarState` once per tick. **Count the hook's calls**
before believing it (inlining). Decide left/right wheels by local position if
the arrays do not say.

## 4. Telemetry first

Emit Forza through `Dbce.Wheel.Telemetry` (Horizon 324 unless the dash profile
says otherwise). Verify with `tools/forza/forza_probe.py`, then SimHub.
`IsRaceOn` from the countdown; park between stages; idle keeper if the
launcher can host it. Telemetry proves the sampler before FFB depends on it.

## 5. Force feedback — measure, then model

- Open the wheel through `native/wheelffb` (DirectInput backend). Confirm
  "initialised OK", device list with capabilities, effect axis count.
- **Measure the sign** on the real base with a fixed test level; write the
  measurement in the code next to the sign; ship an Invert toggle anyway.
- Choose the source signal (`FFB-SIGNAL-DESIGN.md`): lateral force × trail if
  the tyre model exposes forces; the game's own motor output if it is an
  arcade board; the `ForceModel` presets otherwise. **Not raw aligning torque.**
- Turn the input validators and the 5 Hz trace on; drive; read the trace for
  sign-by-speed-band and liveness before judging feel.
- Use the shared [steering/effect strength contract](../docs/FFB-STRENGTH-CONTROLS.md)
  and current versioned reference. Steering strength and crash/effect strengths
  are independent. Keep the model's low-speed fade explicit.
- With shared controls, make Follow Steering use the active profile's exact
  wheel, including pending, refusal and stop states. See the
  [selection contract and A/B fixture](SHIM-IDENTITY-AND-NATIVE-ACTIONS-2026-10-11.md#follow-steering-ffb-must-follow-the-active-input-route).

## 6. Input

- Does the game's input library see the wheel? If yes: fix its hidden
  deadzone and binder limitations (`REWIRED.md`, `UNITY-INPUT-SYSTEM.md`). If
  no (Fanatec under Raw Input, shifters everywhere): direct read through the
  toolkit and overwrite the car's inputs in a postfix. **Never** switch the
  library's backend at runtime.
- Shifter: read directly; H-pattern reverse-first; sequential edge-triggered.
- Bind by "Assign, then move it", with settle-then-arm and rest/far calibration.

## 7. Camera

Bonnet and bumper views into the game's own cycle; Cinemachine vcam at high
priority if the game uses Cinemachine; hand back the instant the game directs.
Numpad tuning that writes back to config.

When moving canvases onto a camera for a centered triple-screen HUD, check
[world-space precision and canvas depth](UNITY-CANVAS-PRECISION.md). A stable
camera can still leave static text jittering when a tiny canvas is far from
the origin; qualify the presentation change independently of driving controls.

## 8. Panel, settings, docs

Same section vocabulary as the other mods. Device dropdowns with capabilities.
"Create support file" button. `CLAUDE.md` with the non-negotiables and a
Status table that says what is *verified* versus *built*. `CHANGELOG.md` from
the first release.

## 9. Package and install

`Install.bat` via Steam registry discovery; refuse while the game runs; marker
file for uninstall; config backup. One zip. Post the changelog.

## 10. Before release

- Drive with alt-tab mid-stage; drive to the finish cinematic; pause; quit
  from a stage — the wheel must be zero in every case.
- Support bundle from your own install reads clean.
- Every knowledge item learned goes into this repo with `[repo, date]`.
