# Case file: iRacing Arcade

| | |
|---|---|
| Repo | `E:\Source\iracing-arcade-wheel` (private) |
| Game | iRacing Arcade (Steam 3226450) — the Circuit Superstars codebase (`CircuitSuperstars.*`) |
| Engine | Unity **6000.3.19f1 IL2CPP**, metadata v39; Burst physics; Cinemachine; **Unity Input System**; FishNet networking; PlayFab leaderboards |
| Loader | BepInEx 6 Bleeding Edge #788 (Doorstop 4.5, .NET 6), Il2CppInterop, Harmony |
| State | 0.2.0 (2026-09-03): loader, hook, telemetry, cameras, gearbox, panel and rebinding verified live; **FFB output and pedal shaping unjudged at the base** |

## What was wrong with the game

`InputSystem.settings.supportedDevices` is `Mouse, Keyboard, Gamepad`: every
wheel, shifter and stalk is discovered and ignored — the stock game **cannot**
bind a wheel, whatever reviews say. No FFB anywhere. No telemetry. One follow
camera. Automatic gearbox only.

## What the mod does

- Clears `supportedDevices` (empty = everything); DS-8X shifter and MOZA stalk
  appear; nothing junk does because no layout matches the other 35 HIDs.
- Harmony postfix on `AutomobilePipeline.PostPhysicsUpdate` records the local
  car; `Sampler` copies into `CarState` once per tick.
- FFB from a **slip × load proxy** through the shared native DLL — measured
  ~30 straight / 4,600 hard corner / 10,600 spin; reference 5,000. **Check for
  the Pacejka-style reversal before trusting it.**
- Forza 324 telemetry at 50 Hz (identical encoder to art of rally).
- Chase camera override on the live `EdyCamera` (only an *enabled* rig counts);
  bonnet/bumper as a Cinemachine vcam at priority 1000 parented to the car,
  placed from `LocalBounds`; the game's unused `Driving/NextCamera` action is
  borrowed to cycle.
- **Manual gearbox ("fun mode")**: prefix on `AutomobileWheelsSetup.PushWheelInputs`
  scales torque and divides rpm caps, with the mod's own rev limiter; only for
  the local car (pointer compare); **offline Free Play only**, gated three ways
  and with `UserTimeTrials.TryUpdateResult/TryUpdateBest` forced false. Owner
  decision: never loosen without asking.
- Controls panel on F6 (IMGUI rebuilt from stripped primitives): rebind any
  action via a private capture action, overrides persisted as JSON and
  re-applied every 3 s, pedal/invert/deadzone as processors, custom HID layout
  JSONs, HID-gap diagnostics, F8/F9/F10 mod-owned button bindings.
- Installer with the BepInEx first-launch workaround pre-seeded.

## What was tried and abandoned

`AutomobileMotor.PowerDrive` prefix (inlined, never called). Camera lock onto
the disabled `EdyCamera`. OS `SendInput` for testing (ignored; in-process
`StateEvent` injection instead). In-cabin bonnet placement (single-sided
models).

## Findings that transferred

Injected MonoBehaviours carry only Unity messages; consts not emitted; count
prefix calls; IMGUI stripped; `supportedDevices`; capture-action rebinding;
Simulation-page HID axes dropped; Input System disables keyboard when
unfocused; menu drops a second Down within ~1 s; BepInEx truncates its log.

## Still open

FFB sign/strength at the base; F8/F9/F10 binding at the rig; DS-8X gates;
custom-layout path never exercised on a device that needs it; `[Dev]
InputCommandFile` must be off before others use the install.

## Where to look

`docs/HOOKS.md` (gotchas), `docs/CONTROLS.md`, `docs/LOADER.md`, `docs/GEARBOX.md`,
`docs/CAMERA.md`, `CLAUDE.md`.
