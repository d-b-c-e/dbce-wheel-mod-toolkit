# Case file: art of rally

| | |
|---|---|
| Repo | `E:\Source\art-of-sim-rally` (public, MIT, users on Reddit/Nexus) |
| Game | art of rally 1.5.8b, Steam 550320 |
| Engine | Unity 2019.4.38f1 **Mono**; Edy's Vehicle Physics (`CarDynamics`, `Wheel`, `Drivetrain`); **Rewired 1.1.55** on Raw Input |
| Loader | Unity Mod Manager + Harmony |
| Released | 0.1.0 → 0.2.1 (2026-09-02); 0.2.2 pending (direct wheel input, Fanatec crash fix) |

## What was wrong with the game

The FFB feature is half-built and never wired: `ForceFeedback` MonoBehaviour is
never attached, `Wheel.Mz` is only computed `if (cardynamics.enableForceFeedback)`
which nothing sets, `CarDynamics.forceFeedback` is never assigned, and the native
`UnityForceFeedback.dll` the game declares seven imports from **does not exist in
the shipped build**. Unrecognised wheels get Rewired's hidden 10% deadzone and
gamepad steering smoothing. The controls screen binds `Joysticks[0]` only.
Shifters are invisible to Raw Input. No telemetry. Chase camera only.

## What the mod does

- Supplies the missing DLL (seven game-declared exports plus its own) and sets
  the flag; force is **front-axle lateral force × pneumatic trail**, faded below
  12 km/h — the game's Pacejka `Mz` reversed past ~8° slip and the tyres run
  12–29° in corners ("there is no centre"). `+Fy` centres on a MOZA R12.
- Direct steering path and zero deadzone for unrecognised wheels; bind-any-device
  latch on the remapper; glyph text fallback.
- Shifter read directly through the native aux device (H-pattern + sequential).
- Forza 324 telemetry at physics rate; `IsRaceOn` from the start-line hold.
- Bonnet and bumper views added to the game's rotation; numpad tuning.
- Direct wheel input (`WheelInput`) for devices Rewired cannot read (Fanatec):
  assign-by-moving, rest/far calibration, postfix on `AxisCarController.GetInput`.
- Support bundle, Install.bat, changelog.

## What was tried and abandoned

- **Switching Rewired to DirectInput at runtime** — four attempts; menus dead
  with input flowing at every layer. Settings.xml-only experiment remains.
- Raw `Mz` as the force. Fixed `+X` direction encoding (worked on R12, not R5).
- Cockpit camera (cars have no interiors — settled, not a gap).

## Findings that transferred

`0x80040205` is `NOTEXCLUSIVEACQUIRED` (alt-tab); signed-magnitude encoding
with matched direction length; one DirectInput instance per session (Fanatec
crash chain); Fanatec twins and `0x80040154`; try every FFB candidate;
`IsRaceOn` from countdown; SendInput works here (INPUT struct 40 bytes);
`Stop-Process` from Bash-spawned PowerShell does not; the hook keeps running
through the end-of-stage cinematic (ask the event manager).

## Still open

End-of-stage camera wobble (cosmetic). Direct wheel input drive test on the
owner's rig. Reply to the Fanatec user after 0.2.2.

## Where to look

`docs/FORCE-FEEDBACK.md`, `docs/CONTROLS.md`, `docs/CAMERA.md`, `docs/TELEMETRY.md`,
`CLAUDE.md` ("Findings that must not be re-derived"), `CHANGELOG.md`.
