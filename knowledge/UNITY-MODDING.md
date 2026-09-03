# Modding Unity racing games — the parts that repeat

Two Unity games are on the toolkit: art of rally (Unity 2019.4 **Mono**, Unity
Mod Manager) and iRacing Arcade (Unity 6 **IL2CPP**, BepInEx 6). The next
Unity title will be one or the other. Provenance in brackets.

## First: Mono or IL2CPP?

`<Game>_Data\Managed\Assembly-CSharp.dll` present → **Mono**: readable C#,
Harmony patches anything, decompile with `ilspycmd`. `GameAssembly.dll` +
`il2cpp_data\Metadata\global-metadata.dat` → **IL2CPP**: native code with
metadata; you patch through Il2CppInterop-generated proxies and live with
inlining. `dbce-mod-toolkit`'s engine fingerprinter reports this.

| | Mono | IL2CPP |
|---|---|---|
| Loader | Unity Mod Manager (UMM) or BepInEx 5 | BepInEx 6 Bleeding Edge (≥ #786 for metadata v39) |
| Patching | Harmony, everything reachable | Harmony via Il2CppInterop; small methods may be **inlined** and unpatchable |
| Reading game code | `ilspycmd -t Type Assembly-CSharp.dll` (method bodies) | Cpp2IL / interop assemblies: signatures and fields, **no bodies** |
| Managed runtime | Unity's Mono | .NET 6 CoreCLR (BepInEx 6) |
| Shared libs | `netstandard2.0`, no Unity refs | same assembly loads unchanged |
| Native DLL loading | `LoadLibraryW(path)` then `DllImport` by name | `NativeLibrary.SetDllImportResolver` (or the same preload) |
| IMGUI | full | mostly **stripped**; rebuild widgets from `GUI.DoControl`, `GetRect`, `DrawTexture` |

Details: `UNITY-IL2CPP-BEPINEX.md`.

## Loader notes

- **UMM** (art of rally): install method "Assembly" patches `Assembly-CSharp`;
  `Load(ModEntry)` runs after the first scene's Awake — Rewired etc. are already
  initialised. Settings are a plain-field `ModSettings` class serialised to
  `Settings.xml` beside the mod (unknown XML elements ignored, so new fields are
  safe). Panel is `OnGUI` IMGUI; `Ctrl+F10`. Log at
  `<game>_Data\Managed\UnityModManager\Log.txt`. `[art-of-sim-rally]`
- **BepInEx 6 IL2CPP**: pre-seed `BepInEx.cfg` with `[IL2CPP]
  UnityBaseLibrariesSource =` (blank) **before first launch** or the chainloader
  dies unstripping `UnityEngine.CoreModule` (BepInEx#1350). First launch
  generates interop assemblies (~70 s); copy `BepInEx\core` + `interop` into a
  git-ignored `lib/` for building. Config is `ConfigEntry<T>` in
  `BepInEx\config\<plugin>.cfg`; F-key reload is your own. `[iracing-arcade-wheel docs/LOADER.md]`
- Do **not** hand-write `InitializeComponent`-style generated code (WPF) — the
  generator then skips its own; analogous trap in Unity: do not add
  non-Unity-message methods to an injected MonoBehaviour under IL2CPP.

## Harmony patterns that carried between games

- **Prefix to enable, postfix to read.** art of rally: prefix on
  `CarDynamics.FixedUpdate` sets `enableForceFeedback` so the tyre model
  computes `Mz` this step; postfix reads. Setting the flag in a postfix
  computes one frame late.
- **`out` parameters are `ref` in the patch** (`AxisCarController.GetInput(out
  float throttle…)` → `ref float throttleInput`). Postfix and overwrite only the
  channels you own; leave the rest.
- **Block a method with `__result = false`** as a prefix returning `false`
  (iRacing's time-trial recording gate).
- **Patch a getter**: `[HarmonyPatch(typeof(ControlsRemapper), "get_joystick")]`
  postfix with `ref Joystick __result` (bind-any-device).
- **Count your calls.** A patched method that IL2CPP inlined is never called;
  log `prefix calls / 2 s` while the feature should be live.
- Keep a static `Enabled` gate the loader toggles, and a `Reset()` between
  stages so filter state does not carry over.

## Finding the car

- Mono: grep the decompiled assembly for `Rigidbody`, `WheelCollider`, `slip`,
  `rpm`, `steer`; art of rally is Edy's Vehicle Physics (`CarDynamics`, `Wheel`
  with `Fx/Fy/Mz/slipAngle/idealSlipAngle`, `Drivetrain`, `AxisCarController`).
- IL2CPP: dump enabled actions and a few types with `ilspycmd -t` on the
  interop assemblies; hook a per-tick method on the car pipeline and record
  which instance `IsLocalPlayer()` (iRacing: `AutomobilePipeline.PostPhysicsUpdate`).
  Distinguish the local car by `Pointer` identity, not by reference equality.
- Left/right within an axle is often **not** in the arrays; decide by
  `InverseTransformPoint(wheel.position).x`.
- Sample into a plain `TelemetryFrame`/`CarState` struct once per tick; every
  consumer (FFB, telemetry, gearbox) reads that, not the game objects.

## Driving state

Hooks on the physics step keep running through **end-of-stage cinematics**
while the game drives the car. Ask the game's own event manager whether the
player is in control (`UNDERWAY`) before applying forces; the wheel was
otherwise dragged to full lock after the finish line. Telemetry `IsRaceOn`
wants the wider state (from the countdown). `[art-of-sim-rally GameState]`

## Cameras

- **Mono, custom rig** (art of rally `CarCameras`): postfix `LateUpdate`,
  position the camera at a per-view offset on the car; add bonnet/bumper
  angles to the rotation list so the game's own cycle includes them; hand the
  camera back the moment the game directs (end-of-stage), or it ends up
  underground. Known cosmetic wobble when the cinematic takes over.
- **Cinemachine** (iRacing): a `CinemachineVirtualCamera` with no Follow/LookAt,
  parented to the car at a local offset, `m_Priority = 1000` while selected,
  GameObject disabled otherwise. The brain shows the highest-priority enabled
  vcam; nothing of the game's is patched. Place from `LocalBounds` (bonnet at
  `max.y − 0.22`, bumper at `min.y + 0.35`, `NearClipPlane 0.05` inside the
  collision volume). Low-poly cars are single-sided: a camera inside the body
  sees through it. **Only an enabled rig counts** — the first override locked
  onto a disabled `EdyCamera`. `[iracing-arcade-wheel docs/CAMERA.md]`
- Bonnet, not cockpit: the cars have no interiors. Settled.
- Numpad live tuning (8/2, 7/9, 4/6, 1/3, +/−, 0) writing metres back to the
  config is worth building once; both games have it.

## Input libraries you will meet

- **Rewired** → `REWIRED.md`. Never switch its backend at runtime.
- **Unity Input System** → `UNITY-INPUT-SYSTEM.md`. `supportedDevices` and
  interactive rebinding are the two traps.
- **Legacy Input Manager**: `Input.GetAxis("Horizontal")`; wheels appear as
  joysticks 1–N with fixed axis numbering; no runtime binding UI unless you
  write one.
- When the library cannot read the device at all, **read it yourself** through
  the toolkit's native layer and overwrite the car's inputs in a postfix.

## Panels and settings

- Same section vocabulary in every mod: Controls / Steering, Wheel and pedals,
  Force feedback, Gearbox, Camera, Telemetry, Devices and troubleshooting.
- One dial per sensation (Strength; the reference stays internal).
- Device dropdowns show axis/button counts so same-named devices differ.
- "Create support file on Desktop" button; see `SUPPORT-BUNDLES.md`.
- Under IL2CPP save config every 2 s while the panel is open, not per drag.

## Packaging and install

- Package script stages loader + plugin + native DLL + README, emits a zip;
  `Install.bat` finds the game via the Steam registry and `libraryfolders.vdf`
  (Steam is frequently not under Program Files), refuses if the game is
  running, leaves a marker so uninstall only removes what it installed, and
  backs up config first. Keep the user's side simple: every manual step is a
  support burden. `[art-of-sim-rally, iracing-arcade-wheel]`
- Publish the changelog with every release; users read it.

## Reading the game's assemblies

`ilspycmd` (dotnet tool): `-t <Type> Assembly-CSharp.dll` for one type,
`-p -o <dir>` for everything. Type/field/method names and the P/Invoke table
are also readable from metadata alone with `System.Reflection.Metadata` — that
is how the missing `UnityForceFeedback.dll` was found in art of rally.
