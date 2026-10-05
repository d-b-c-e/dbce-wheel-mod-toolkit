# Slipstream — Windows mod feasibility

Assessment date: **2026-09-07**. Provenance: **[dbce-wheel-mod-toolkit, 2026-09-07]**.

**Recommendation:** a worthwhile candidate for analog wheel controls, useful dashboard telemetry and deliberately synthesized arcade FFB. A forward-facing bonnet-style view is plausible, but a geometrically faithful bonnet camera is a different and substantially larger rendering project. The main initial risk is loading code into the shipped, trimmed .NET bundle; the car-state seam itself is unusually accessible.

This assessment used read-only inspection of installed files, static extraction/decompilation into the user's temporary directory, repository knowledge and current public sources. No game/editor was launched, no wheel was enumerated or acquired, no force was applied, and no installed game, configuration or save was changed. Nothing here establishes tested Slipstream compatibility with the owner's equipment.

## Feature ratings

Scores rate the proposed feature's feasibility on the inspected build: 5 = straightforward extension; 3 = credible with a material unresolved integration problem; 1 = major redesign. Effort is estimated engineering time, not a delivery commitment.

| Requested feature | Existing support / evidence | Proposed result | Rating and confidence |
|---|---|---|---|
| Wheel input | Keyboard/gamepad rebinding and rumble exist. Gamepad axes become digital button flags before driving logic. No dedicated wheel calibration or multi-device driving-role system found. | Continuous steering and separate analog throttle/brake through a game-specific input adapter. Shifter/stalk buttons can control actions; genuine H-pattern driving needs a new transmission model. | **3/5**, medium implementation confidence; high confidence in the static input diagnosis. |
| Telemetry | No native dashboard/UDP car telemetry export identified. The editor's `--port` argument is not telemetry. Public car-state fields and a fixed-step update loop are present. | Speed, inputs, race/lap information, crash/drift/off-road cues; optional explicitly synthetic RPM/gear. | **4/5**, medium-high once a loader is proven. Physical per-wheel data is unavailable. |
| FFB | Gamepad low/high-frequency vibration exists. No directional wheel-force implementation found in the game's driving/output path. | Speed-dependent centring/damping, drift lightening, off-road texture and bounded collision events using the toolkit. | **3/5**, medium. Good arcade cues are plausible; tyre aligning torque is not available. |
| Bonnet/hood camera | Third-person sprite presentation; camera offsets, FOV and tilt are internal rendering parameters. No player-selectable bonnet mode found in inspected settings/gameplay paths. | Reprojected forward view with the player sprite omitted and optional original hood artwork. | **3/5 for a stylized bonnet view**, medium-low; **1/5 for a faithful free 3D bonnet camera**, high confidence that substantial reconstruction would be needed. |

The score does not imply any of these modifications has run. Static stock findings are detailed below; public support claims are confined to the cited sources. [dbce-wheel-mod-toolkit, 2026-09-07]

## Exact distribution and runtime inspected

| Item | Local evidence |
|---|---|
| Distribution | Steam Windows installation, app **732810**, `D:\Program Files (x86)\Steam\steamapps\common\Slipstream`. |
| Build | `appmanifest_732810.acf`: **15704123**, installed depot **732811**, manifest **1083573391354303750**. No beta branch name is established by that manifest. |
| In-game version identifier | Embedded assembly's `Slipstream.Core.Globals.VERSION_STRING`: **v1.3.3-R6**. Assembly version `0.0.0.0` and dependency-project version `1.0.0` are not the game release version. |
| Executable | `slipstream.exe`, **16,414,515 bytes**, PE machine **0x8664 / AMD64**. SHA-256: `6ACF5EED35FE84CA8C4E9B9498302056ADACF7703CD5C750C5E7A3CB8362A5DF`. |
| Runtime | Native single-file host containing managed assemblies and an embedded `slipstream.runtimeconfig.json`: **net8.0**, included **Microsoft.NETCore.App 8.0.8**; dependency target `win-x64`. |
| Game assembly | Embedded `slipstream.dll`, file offset **10,039,296**, size **867,328**. SHA-256: `C20ACAE0F5F7CD77C0E49FAF63847F9A95DF4D4228D829F08F54E3FB429E9CD5`. Target framework `.NETCoreApp,Version=v8.0`; readable IL and names. CLI flags `0x1` (IL-only), managed native header zero: the extracted game assembly is not a NativeAOT or ReadyToRun-only binary. |
| Framework / native libraries | Loose `FNA.dll` file/product version **24.9.0.0**; `FNA3D.dll`, `FAudio.dll`, `SDL2.dll`, `libtheorafile.dll`, `Steamworks.NET.dll`, `steam_api64.dll`. The dependency manifest names Steamworks.NET **20.2.0.0**. |
| Renderer evidence | `assets/shader/pseudo3d.fxb`, road projection code, car texture atlases. A pre-existing `SlipstreamLog.txt` reports **FNA3D D3D11**, RTX 5080 and two Xbox-labelled gamepads. That is historical log evidence, not a test performed for this assessment; it does not identify either gamepad as the owner's wheel. |
| Content actually inspected | Only **vanilla** under `assets/content`. Blue Hour and arbitrary Workshop tracks were not locally inspected. Their extra rendering cases remain a coverage gap. |

The custom C#/FNA engine is corroborated by the developer's [own project history](https://www.ansdor.com/) and [Steam update history](https://steamcommunity.com/app/732810/allnews/): version 1.2 replaced libGDX with FNA. Old Java/libGDX instructions do not describe this installation. The [Steam store](https://store.steampowered.com/app/732810/Slipstream/) explicitly describes a pseudo-3D engine using 2D graphics, and advertises Workshop, a level editor, local multiplayer and leaderboards. Sources accessed 2026-09-07. [dbce-wheel-mod-toolkit, 2026-09-07]

## Loader and hook feasibility

**Confirmed restriction:** the embedded runtime configuration sets `System.StartupHookProvider.IsSupported` to **false**. It also disables debugger support, metadata updating, built-in COM interop and consuming managed code through native hosting. The package includes ILLink dependencies and trimmed framework assemblies. Therefore, setting `DOTNET_STARTUP_HOOKS` is not an established drop-in loader for this build. A nearby external runtimeconfig file must not be assumed to override the embedded configuration. Microsoft's [single-file documentation](https://learn.microsoft.com/en-us/dotnet/core/deploying/single-file/overview) explains embedded configurations and assembly-location limitations; its [environment-variable documentation](https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-environment-variables#dotnet_startup_hooks) describes the generic startup-hook mechanism, not its availability in this particular game. Accessed 2026-09-07.

**Best first candidate, untested:** an x64, untrimmed .NET 8 managed launcher in a future Slipstream-specific repository. It would verify the original executable hash, read the user's own embedded game assembly, resolve the installed FNA/Steamworks assemblies and native libraries, install narrowly scoped Harmony patches, then invoke the original STA entry point. Ship the adapter/loader, not copied game code. The existing IL is sufficient for this to be a concrete experiment rather than a native offset hunt. [Harmony's original project](https://github.com/pardeike/Harmony) supports runtime method decoration; this is a candidate dependency, not proof that this game's load arrangement works. Accessed 2026-09-07.

Loader details that must be settled by that experiment:

- `Slipstream.Program.Main` parses editor arguments and creates `MainGame`; preserve its STA and ordinary startup lifecycle.
- Storage derives asset paths using `AppDomain.CurrentDomain.BaseDirectory`. Merely changing the working directory is insufficient; the launcher needs an explicit, tested original-install path strategy.
- Preserve native DLL resolution and the legitimate Steam application context. `SteamServices.Initialize` calls `SteamAPI.Init`; Workshop, achievements and leaderboard behavior under another executable are unverified.
- Use the launcher's complete framework where the stock bundle trimmed away optional APIs. Test reflection, dynamic patch generation and exact dependency identity before device work.
- Patch before first relevant JIT compilation and count hook calls. Extraction/decompilation success does not prove method interception.

A game-specific native bootstrap or local bundle patch could be fallback work if the launcher fails, with additional maintenance cost. An arbitrary `dinput8.dll` proxy is not a proven managed-code bootstrap: game input goes through FNA/SDL and no qualifying DLL load path was demonstrated here. Unity Mod Manager and Unity IL2CPP BepInEx assumptions do not apply. The embedded CLR settings do not prevent the toolkit's native C ABI in principle; DirectInput COM calls occur inside its C++ DLL. [dbce-wheel-mod-toolkit, 2026-09-07]

Useful static hook candidates are `MainGame.UpdateFrame`, `Slipstream.Screens.Gameplay.WorldUpdate`, `Slipstream.Elements.Car.Update`, `Car.UpdatePedals`, `Car.ApplyPhysics`, `Road.Draw`, `Road.DrawCar`, and `MainGame.BeforeQuit`. Their responsibilities, signatures and call order were inspected; none was hooked dynamically.

## Input and the owner's hardware

`Controls.RawGamePadState` reads sticks/triggers, applies the configured stick deadzone, then uses a **0.25 threshold** to produce flags. `PollGamepad` maps flags to actions. `Car.UpdatePedals` ramps accelerator, brake and `SteeringWheel` from those flags; it also updates `Direction` and participates in auto-drift behavior. The presence of float fields and an analog gamepad API therefore does **not** establish continuous analog driving support. The current update loop advances in **1/60-second** steps. [dbce-wheel-mod-toolkit, 2026-09-07]

The adapter should read physical devices independently and normalize each role, then replace the digital accumulation at the car-input seam while retaining the game's drive-state restrictions and direction/drift calculations. Overwriting fields after the entire car update risks stale or inconsistent physics, animation and replay state. Preserve thresholded action bits where the brake/accelerator sequence is used to initiate a drift, and define how analog travel interacts with that sequence. This is an input semantic change; it requires testing beyond checking that an axis value changes.

| Hardware / role | Attainable integration and remaining limit |
|---|---|
| **MOZA R12** steering | Native toolkit direct reads and x64 FFB output are credible. Implement rotation range, centre, inversion, deadzone and saturation in the per-game adapter. Its dedicated wheel view/calibration is new work. No Slipstream/R12 test was performed. |
| **Separate pedals** | Read a second device by saved identity; calibrate throttle and brake independently, including polarity and rest/far endpoints. Do not assume combined axes or require the pedals to appear as an Xbox controller. Exact pedal model and connection layout were not established in this task. |
| **SIMAGIC DS-8X** | Toolkit knowledge records its DirectInput button presentation. Buttons can invoke existing menu/rewind/other actions. **No current gear selector, gear-ratio state or clutch model was found**: `Gearbox` is an acceleration-related car upgrade attribute, not current gear. H-pattern or sequential transmission behavior would be newly designed per-game gameplay work. |
| **MOZA stalk** | Candidate button-to-action binding via direct reads, subject to its actual exposed device/axis/button layout. No native lights, indicators or wipers were found to control; stalk handling should not promise those functions. Persistent switch positions require explicit edge/level behavior. |

The original community [Wheel to Keyboard inputs guide](https://steamcommunity.com/sharedfiles/filedetails/?id=2460164236), published 2021-04-17 and updated 2024-09-28, describes AntiMicroX mappings and reports several Logitech/Fanatec/separate-pedal setups. That is evidence for a digital workaround reported by its author, not analog support, directional FFB or MOZA compatibility. An [older wheel discussion](https://steamcommunity.com/app/732810/discussions/0/1693795812312354715/) likewise concerns mappings and predates this build. Accessed 2026-09-07.

Ghost recordings are a significant compatibility issue: `Car.Update` records only a byte of driving action bits and playback feeds those bits back into the simulation. Continuous input cannot be faithfully reproduced in that format. Disable or separately version analog ghost recording, and establish a per-game leaderboard gate before distributing a controls mode that changes driving semantics. The store and static Steam leaderboard code establish that online scores exist; no anti-cheat integration was identified in the inspected file set. [dbce-wheel-mod-toolkit, 2026-09-07]

## Telemetry: available signals and honest limits

Public `Car.States[]`, `CurrentStateIndex` and `PrevStateIndex` form a ring of simulation/rewind state. Sample the selected local player's completed state, not an arbitrary car in the traffic list. The game also has multiple local-player viewports, time manipulation and separate countdown/finish states.

| Signal | Static evidence | Export treatment |
|---|---|---|
| Speed | `Car.Speed` reads `CarState.Velocity.Y`; HUD converts by 3.6 for km/h and 2.2369 for mph. | Strong candidate for direct m/s export. Validate against HUD. |
| Steering and pedals | `SteeringWheel`, `Accelerator`, `Brake`, `LastInput`, `Direction`. | Separate raw physical input from effective game input. `Direction` is an arcade control/state quantity, not proven world yaw or tyre slip. |
| Longitudinal acceleration | Changes in speed over simulation time; game computes a scalar acceleration from engine/brake/drag terms. | Derived signal with reset handling and explicit unit validation. Game-speed scaling and rewind must not create spurious impulses. |
| Lateral movement | `Velocity.X`, `X`, track curve, collision vectors. | Useful arcade cues; road-relative movement is not automatically vehicle-frame acceleration. Avoid presenting camera-relative movement as physical G force. |
| Drift / slipstream / off-road / collisions | `DriftConfirmed`, `DriftStep`, flags, `SlipstreamActive`, `SlipstreamLevel`, `CollisionForces`, `CrashDirection`, `Crashing`, `Road.IsOnTrack`, named `crash`/`bump` signals. | Export validated events/flags through diagnostics; use bounded synthetic effects. Flag meaning and transitions still need runtime validation. |
| Race and lap | Gameplay state, player identity, clocks, route/fork and race-position handling. | Candidate lap time, position and `IsRaceOn`; prove countdown, pause, finish, rewind and stage transitions. |
| Position | Integer track-relative `X` and longitudinal `Z`; `ApplyVelocity` scales velocities by 225 for these coordinates. | A path-distance estimate is possible after calibration. These are not a globally consistent XYZ track map. |
| RPM / current gear | `EngineSoundPitch` exists; no engine revolutions, discrete transmission state or ratios found. | Optional explicitly synthesized dashboard RPM/gear. Sound pitch is an authored cue, not measured crankshaft speed. |
| Tyre / suspension physics | No per-wheel normal loads, suspension travel, contact force, tyre slip or aligning torque found in `CarState` / inspected physics. | Leave unsupported; never invent physical telemetry to fill every packet field. |

Route: per-game sampler → toolkit **`TelemetryFrame`** → **`Dbce.Wheel.Telemetry`** Forza encoder → configurable loopback UDP destination. Choose the existing receiver's 324-byte Horizon or 311-byte Dash layout; validate exact packet length and values with `tools/forza/forza_probe.py`, then the intended SimHub dashboard. This reuses a transport/encoder, not an existing native Slipstream integration. No Slipstream telemetry mod or official SDK was located in the scoped searches. The editor's pipe/port facility serves track editing, not continuous racing telemetry. [dbce-wheel-mod-toolkit, 2026-09-07]

Maintain monotonic packet timestamps and explicitly gate/reset on rewind, reset, pause, fast-forward and lost player state. Emit an inactive keeper frame between races with coherent RPM normalization. Keep unavailable/synthesized-field metadata in the adapter and support report: Forza lacks a universal validity mask, and byte fields cannot carry a generic `-1` sentinel. Verify the intended receiver's fallback behavior. MOZA Pit House/SimHub port contention is already documented in [the toolkit telemetry notes](../../../knowledge/TELEMETRY-FORZA.md); that hardware plumbing still needs a Slipstream-specific receiver check.

## FFB signal quality

This is **synthesized arcade FFB driven by real game state**, not reconstructed physical tyre load. The inspected model computes forward speed and sideways response using arcade scalar formulas and track curvature. Even `Driver`'s AI quantity named `LateralForce` reuses a direction/speed expression; its name is not evidence of Newtons at the steering rack. Collision vectors are game response values, not calibrated impact forces. [dbce-wheel-mod-toolkit, 2026-09-07]

A sensible first profile uses physical wheel angle/rate for centring and damping, game speed for weight/fade, validated drift state for lightening, off-track status for mild periodic texture, and crash/bump transitions plus pre-impact speed for bounded events. Preserve the source and confidence of each cue. Gamepad `LowFreqVibration`/`HighFreqVibration` can help identify event timing but carry neither wheel-force sign nor rack torque. Do not convert the two rumble channels directly into opposing constant-force directions.

Send the resulting force through toolkit shaping, output diagnostics and the native DirectInput backend. Start with logging only; later attended tests must establish sign, low-strength stability, pause/focus/rewind gating, watchdog behavior and zero before the window closes. Derivatives must reset across time discontinuities. SDL is already present in the game, but the toolkit SDL backend is **not implemented**, and sharing/exclusivity between FNA's input path and the physical base remains untested. Apply the existing [device rules](../../../knowledge/DIRECTINPUT-FFB.md) and [signal guidance](../../../knowledge/FFB-SIGNAL-DESIGN.md), with per-game normalizers and an explicit synthetic profile. [dbce-wheel-mod-toolkit, 2026-09-07]

## Bonnet view: projection and visibility constraints

The developer's [modding guide](https://slipstre.am/modding/) (published 2024-02-21, revised 2024-09-04; accessed 2026-09-07) explains car sprites, camera-facing scenery, special depth/floor props and the non-Euclidean track representation. It states that slopes affect appearance/visibility rather than physics. Car creation uses Blender to render sprite frames; a Blender source asset does not mean a 3D car mesh is loaded during play. The guide's raw text is linked by the page at [modding.md](https://slipstre.am/modding/modding.md).

Local code independently confirms the practical constraints:

- `Gameplay.PlayerData` has interpolated X/Y/Z camera offsets. `Road.Draw` derives a chase viewpoint from the car, a default height of **90** rendering units and a rearward offset involving baseline FOV and viewport width. FOV defaults to **60**. These are pseudo-3D rendering parameters, not a Unity/Unreal camera transform.
- Road sections are projected into screen geometry using accumulated segment `DeltaX`/`DeltaY`; `InfoBuffer.Clip` controls hill occlusion and the back-to-front drawing of road, props and cars. Advancing/lowering the viewpoint changes the near segments and clipping behavior.
- `Road.DrawCar` computes sprite position/scale from segment depth. Hiding the local player's draw is feasible in principle, but the same car object also supplies route, fork, FOV, draw distance and visibility decisions. Remove only its drawing, not its presence in the simulation or required render context.
- Background, road and particles have separate draw calls and camera arguments. They must use a coordinated viewpoint. Simply changing one offset can leave smoke, background parallax, fog or track alignment detached from the player.
- Cars are atlased normal/crash/spin views. There is no installed hood/interior mesh established by this inspection. A new bonnet overlay needs original artwork and an explicit stylistic choice. Inferring a complete hood from the rear sprite is not an existing rendering feature.
- `Direction` controls arcade behavior and sprite selection. No continuous global car orientation/camera yaw system was established. A forward road-aligned view is much easier than a camera rigidly attached to a freely rotating 3D car.

**Attainable first version:** a road-aligned forward view, lower and farther forward than chase, with the local car sprite suppressed and optional original static/animated hood art. Keep HUD/radar available. Test near-road continuity, crests/dips, sharp curves, off-road visibility, traffic overlap, fork transitions, reverse tracks, crashes/spins and rewind. Disable the new view outside its proven gameplay states.

**Not established:** convincing close traffic at every angle, arbitrary look-around, faithful yaw during spins, hidden sides/interiors of cars, or a coherent 3D world behind the current view. Blue Hour's depth/floor props add further near-view culling/ordering cases and were not installed here. A true geometric bonnet experience would require substantial scene/asset reconstruction; it should not be promised as a quick camera-position patch. [dbce-wheel-mod-toolkit, 2026-09-07]

## Reuse and repository ownership

The developer's [Slipstream Toolkit](https://ansdor.itch.io/slipstream-toolkit) provides atlas packing, a track-editor GUI and a car-sprite Blender template. Original [bride](https://github.com/ansdor/bride) and [atlas](https://github.com/ansdor/atlas) repositories are useful for understanding content/editor workflows, not a discovered gameplay plugin ABI. The official content system handles music, cars and tracks; it does not establish executable wheel/telemetry hooks. Accessed 2026-09-07.

Reuse this repository's **source/package 0.12.0** and **native component 0.5.0 (38 exports)** separately. Available pieces include exact-module managed binding, native device readers and DirectInput FFB, force model/shaper/profile data, telemetry codecs and diagnostics. The shared input kit, calibration UI and SDL backend are **future work**. The native dinput-proxy scaffold is implemented/tested but does not solve the managed bootstrap here. `docs/STATE.md` takes precedence over older checklist/template descriptions.

Create any implementation in a future Slipstream-specific repository: loader, hooks, input semantics, local-player selection, camera changes, online/replay gating and UI belong there. Keep reusable device/force/telemetry plumbing here. Persist each physical role's identity and expose selection/calculation/delivery status as described in [consumer UX guidance](../../CONSUMER-EXPERIENCE.md). Do not present a calculated or accepted force as tested physical feel. [dbce-wheel-mod-toolkit, 2026-09-07]

## Effort and decisive experiments

Assumptions: one experienced C#/.NET mod developer; this exact Windows x64 build; initial single-player/vanilla scope; access to the owner's hardware for a later attended phase; no new transmission, full 3D reconstruction or public release support in the base estimate. One day means roughly 6–8 engineering hours. Ranges include investigation and modest iteration and overlap, so should not be mechanically summed.

| Work | Rough effort | Smallest experiment / pass criterion |
|---|---|---|
| Loader and startup seam | **2–5 days**; another **5–10** if a different bootstrap is needed | Under future execution authorization, an isolated managed launcher loads the user's original assembly, resolves assets/FNA/Steamworks, reaches the menu and logs counted `UpdateFrame` calls. No device initialization. Stop and reassess if the full-runtime route cannot preserve startup. |
| Analog steering + independent pedals | **3–7 days after loader** | Inject deterministic 25/50/75% values at the input seam with hardware output disabled; confirm distinct effective controls/car response and working drift transitions. Then a no-force role/calibration check on the actual devices. |
| Useful telemetry | **1–3 days after loader**, **2–4** more for lifecycle/receiver polish | Log one selected player's speed and controls through a short drive; speed must match HUD. Emit exact-size packets with sensible countdown/pause/rewind/finish handling and inspect them in the probe/receiver. |
| Synthesized FFB | **2–5 days after sampler/input**, plus attended tuning | A trace-only lap proves cue liveness, signs and reset behavior. Only then test limited force with an attendant and verify focus, pause, quit and time-discontinuity shutdown behavior. |
| Forward/bonnet rendering | **1–3 days for viability**, **5–12 more** for an acceptable vanilla implementation | Remove only the local car draw and adjust road/particle/background views together. Drive a curve, crest, traffic encounter and fork. Any unfixable missing near-road/prop visibility is a camera scope stop condition. |
| True 3D bonnet reconstruction | **4–8+ weeks**, low-confidence and potentially open-ended | First demonstrate a coherent replacement road/prop projection plus enough car geometry for required viewpoints. This is beyond the practical first mod. |

An input/telemetry/synthetic-FFB prototype is approximately **2–4 working weeks**, conditional on the launcher succeeding. A robust release adds setup UI, packaging, restart/disconnect handling, mode coverage and hardware testing. Authentic H-pattern/clutch behavior is a separate physics/gameplay project with no responsible estimate from this static inspection.

**The next experiment should be the no-device managed launcher, followed immediately by a read-only car-state trace and a sprite-suppressed camera proof.** This resolves the actual loading risk and renderer viability before investing in wheel tuning or decorative bonnet artwork. These experiments are proposals; none was run during this research-only task. [dbce-wheel-mod-toolkit, 2026-09-07]

## Evidence audit

- **Verified locally:** Steam build/depot/manifest; executable and embedded-assembly hashes; .NET/FNA versions; IL-only game assembly; disabled startup hooks; analog-to-digital conversion; public state and fixed-step update; rumble output; road/sprite draw paths; only vanilla content installed.
- **Developer statements:** C#/FNA rewrite, pseudo-3D/2D presentation, editor/Workshop content scope and sprite workflow. Sources are linked next to the relevant claims and were accessed 2026-09-07.
- **Community reports:** digital wheel-to-keyboard mappings only; no report has been promoted to tested owner-hardware support.
- **Inference:** proposed launcher, hook strategy, telemetry/force mappings, forward camera adaptation and effort ranges.
- **Unknown until tested:** launcher/Steam compatibility, live patch success, multi-device coexistence, calibrated units beyond HUD speed, analog drift/ghost behavior, physical force sign/feel and all close-view rendering cases.

Static scratch files reside outside the repository at `C:\Users\antho\AppData\Local\Temp\dbce-slipstream-research-20260907`. This report contains observations and identifiers, not copied proprietary implementation or assets. Reproduce inspection against the hashes above; scratch decompiler line numbers are intentionally not treated as a stable hook contract. [dbce-wheel-mod-toolkit, 2026-09-07]
