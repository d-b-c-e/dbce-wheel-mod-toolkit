# Horizon Chase Turbo and Horizon Chase 2: Windows mod feasibility

Assessment date: **2026-09-07**. Provenance: **[dbce-wheel-mod-toolkit, 2026-09-07]**.

This is an initial feasibility assessment completed through read-only local inspection and current web research. No game was launched, device acquired, force applied, mod deployed, installed setting changed, or person contacted. Nothing here is a gameplay or hardware compatibility test. The sequel's correct title is **Horizon Chase 2**, not “Horizon Chase Turbo 2.”

Evidence labels: **Static** means the installed Turbo files establish the code or data relationship, without proving runtime behavior. **Primary** means developer, store, support or original tool documentation. **Community** means a report not reproduced here. **Inference** means an engineering proposal based on that evidence. **Unknown** means the necessary evidence was unavailable. All web citations were accessed 2026-09-07; historical publication dates are supplied where available.

## Decision and separate feature ratings

**Start with Turbo for input and telemetry.** Its readable Mono code exposes suitable seams and useful arcade state. Its old managed runtime adds a bridge task. **Treat the sequel as a separate Unreal investigation**, with more promising world geometry for a bonnet view but substantially less local evidence.

Ratings describe feasibility of a mod, not current support: **High** = concrete accessible seam; **Medium** = credible route with an important experiment outstanding; **Low** = missing essential signals or a fundamental mismatch.

| Game | Wheel input | Telemetry | FFB | Bonnet/hood camera |
|---|---|---|---|---|
| Turbo, inspected Steam build | **High**, high static confidence: float steering, throttle and brake getters are patchable; independent devices need a new adapter/setup UI | **High** for speed, controls, race state and arcade dashboard; **Low** for genuine tire/suspension telemetry | **High** for synthesized arcade cues, medium confidence in eventual feel; **Low** for physical self-aligning torque | **Medium**, medium static confidence: meshes and camera offsets exist, but the road is a moving sampled/deformed presentation; Senna has a special first-person path |
| Horizon Chase 2, no PC build inspected | **Medium**, low-to-medium confidence: Unreal/controller route is credible; actual input contract unknown | **Medium** for a basic dashboard, low confidence until player state is located; rich physics **Unknown** | **Medium** for synthesized cues, low confidence; physical tire forces **Unknown**, not promised | **Medium**, medium confidence: developer documents a real 3D map, but hood geometry, culling and low-view road visibility are untested |

Neither game's standard controller support establishes MOZA R12 support, separate pedal support, wheel torque, or an H-pattern gearbox. A rumble motor API is not a steering torque API.

## Toolkit baseline and integration boundary

Read first: [STATE](../../STATE.md), [consumer experience](../../CONSUMER-EXPERIENCE.md), and AGENTS.md. Relevant knowledge: [new-game checklist](../../../knowledge/NEW-GAME-CHECKLIST.md), [Unity](../../../knowledge/UNITY-MODDING.md), [UE4](../../../knowledge/UE4.md), [force signals](../../../knowledge/FFB-SIGNAL-DESIGN.md), [Forza](../../../knowledge/TELEMETRY-FORZA.md), and [device quirks](../../../knowledge/DEVICE-QUIRKS.md).

The source/package is **0.12.0**; the native device component is **0.5.0**, with **38 exports**. Native input readers already exist: `EnumerateAllDevices`, `GetAnyDeviceGuid`, `OpenReadDevice`, and `ReadDeviceState`. A finished shared input/calibration kit and an SDL backend **do not exist**. The native proxy header scaffold does exist. Prior R12 zero-output smoke and offline conformance do not establish in-game behavior for either Horizon title. [Static; dbce-wheel-mod-toolkit, 2026-09-07]

Future per-game repositories should own loaders, hooks, local-player selection, car-state sampling, camera rigs, control bindings, gameplay gates and UI. Reuse the toolkit for device output/readers, model/shaper/profile data, packet encoding and diagnostics. No game hook, camera or physics modification belongs in this toolkit research change.

## Horizon Chase Turbo

### Exact inspected distribution and runtime

Local evidence **T1**, inspected 2026-09-07:

| Item | Evidence |
|---|---|
| Distribution | Windows Steam app **389140**, installed directory `D:\Program Files (x86)\Steam\steamapps\common\Horizon Chase` |
| Manifest | `D:\Program Files (x86)\Steam\steamapps\appmanifest_389140.acf`: build and target build **20246174**, installed state |
| Main depot | **389141**, manifest **5175572295771994008** |
| Other installed depot | **863521**, manifest **4907089088019941174**, associated DLC app **863520**; soundtrack files are present. This does not prove Senna Forever ownership |
| Engine | `globalgamemanagers` begins with **2018.4.27f1**; executable and UnityPlayer file version **2018.4.27.5122107** |
| Architecture | Both `HorizonChase.exe` and `UnityPlayer.dll` have PE machine **0x014C / x86**; the OS being 64-bit does not change this |
| Scripting | Readable `HorizonChase_Data\Managed\Assembly-CSharp.dll`; `Mono\EmbedRuntime\mono.dll`; no IL2CPP route needed |
| Managed compatibility | Game assembly decompiles as **net35**; bundled mscorlib assembly **2.0.0.0**, file **2.0.50727.1433**; no `netstandard.dll` in the managed inventory |
| Input | `InControl.dll`, `InControlNative.dll`, `XInputInterface32.dll` |
| Game assembly SHA-256 | `73D699E91D5213A7ACB87763F6F330DFF0558B259769274381FC6AFEDFFB8FF2` |
| UnityPlayer SHA-256 | `EB96EBCE63D86FC3BD53AC0F3C2B117E3B2AD39FC59015F413B5D53AB8F55980` |

The Windows Epic build, Linux/macOS builds, console builds and historical Steam builds were **not** inspected. Installed code is readable, but loader execution is unproved.

**Availability:** the current Steam page explicitly marks Turbo unavailable from its store. Valve's retirement documentation says prior owners retain library download/install/launch access. That makes this an existing-owner project, not a recommendation to buy it on Steam. June 1, 2026 is the delisting date reported by PC Gamer from the official announcement; current unavailability is independently verified at Steam. No account entitlement or redownload was tested. [Steam product notice](https://store.steampowered.com/app/389140/Horizon_Chase_Turbo/), [Valve retirement policy](https://partner.steamgames.com/doc/store/retire_app?l=english), [dated announcement coverage, 2026-03-25](https://www.pcgamer.com/games/racing/horizon-chase-turbo-a-great-arcade-racer-with-no-online-functionality-or-licensed-vehicles-is-being-delisted-in-june/).

### Existing support: the wheel code is misleading unless traced

**Static T2:** `Aquiris.HorizonChase.Input.Platform.Pc.PcInputUser.SteeringWheel` returns null. The platform-independent `ISteeringWheelDevice` interface exposes direction, accelerator and brake, and the controls UI has steering/FFB sensitivity names, but that does not make the PC path operational. `LogiWheel` declares native force-effect imports; the installed Plugins folder contains only the Steam and InControl/XInput plugins listed above, with no matching `logiWheel` implementation. Do not describe this as a hidden Windows Logitech FFB feature waiting for a toggle.

The active PC haptic path sends two motor amplitudes to InControl's vibration API. `PlayerCarInputActions` includes analog steering and accelerator/brake actions, nitro, pause, honk and first-person selection. It has no manual shift or clutch actions. No Forza/UDP driving exporter was found in the inspected game code. `ProfileTelemetryData` records session timing/counts: its name denotes analytics, not sim telemetry. A targeted search across the 1,058 decompiled game source files found no `WheelCollider`, `Rigidbody`, `UdpClient` or `Socket` references; this is a scoped negative result, not proof about every engine library.

**Camera support is partly present:** Senna Forever advertises a cockpit view, and the local `FirstPersonCameraController` initializes only for Ayrton career/championship modes with special F1 model components. Standard cars should not be credited with that cockpit or a stock bonnet view. Installed code/assets do not grant DLC entitlement. [Senna Brands feature announcement](https://www.senna.com/en/horizon-chase-presta-homenagem-ayrton-senna-com-expansao-senna-sempre-para-game-em-pc-e-consoles/) [Primary; 2021].

### Loader and analog input feasibility

**Inference:** use **BepInEx 5 x86**, a **net35** consumer plugin, and Harmony-compatible patches. BepInEx's own target-selection guidance agrees with the old mscorlib evidence. Prove a startup log and hook counter first; do not select an IL2CPP loader or assume this behaves like Unity 2019's newer Mono profile. [BepInEx target selection](https://docs.bepinex.dev/articles/dev_guide/plugin_tutorial/2_plugin_start.html).

The toolkit's current managed FFB and telemetry projects target `netstandard2.0`; unchanged assemblies are **not a viable baseline for this legacy game runtime**. A practical initial route is a small net35 sampling/input shim calling a **new consumer-owned x86 C ABI bridge**. That bridge can use the existing native `WheelFfb.dll` and compile the toolkit's C++ force-model/profile and Forza headers. The packet/model headers are not already exposed as DLL exports: their wrapper must be built. An alternative is a net35 shim sending simple state to a modern companion process for telemetry; avoid splitting wheel acquisition between processes. Do not replace the installed Mono runtime just to fit the toolkit.

**Static T3:** `InputHandler.GetSteeringAxis(playerId)`, `GetAcceleration(playerId)` and `GetBraking(playerId)` delegate to `PlayerInputHandler`. Its getters return floats and merge optional wheel input with gamepad actions. `PlayerCar.HandleInputs` consumes those floats; `CarEngine.AnalogAcceleration` and `AnalogBraking` apply response curves. Thus proportional steering and two independent proportional pedals are real code paths, not just button-emulation guesses.

Patch those getter results for the explicitly selected local player, or supply an adapter for the dormant wheel interface. A direct getter override makes role selection clearer than the stock “largest magnitude wins” merge. Preserve menu/pause/autodrive gates. Steering passes through `ProcessAxisInput`, a time-window filter affected by weather, then becomes lateral displacement along the route; it is not a physical road-wheel angle. Initially retain that behavior and measure response. Reducing its filtering is a separate handling decision requiring per-game evaluation and leaderboard considerations.

Independent pedals and accessory buttons can be read through distinct toolkit reader slots with GUID-backed roles. Build per-axis travel, inversion, deadzone and lost-device handling in the consumer; there is no finished shared calibration UI to import.

**Shifter limitation:** `GearBox` computes gear and normalized RPM from normalized speed and an animation curve. There is no ordinary manual gearbox to bind. DS-8X buttons can control existing actions; a sequential/H-pattern drivetrain would be a new gameplay feature with separate effort and online gates. A dashboard gear display alone must not be mistaken for manual transmission support.

### Car-state and telemetry route

**Static T4:** promising sampling point is a postfix on `PlayerCar.Update`, filtered by `PlayerId`, with a later-stage check if the final speed changes again. `PlayerCar` is a regular class with an explicit update pipeline, not automatically a Unity physics component. `LateUpdate` adjusts rubberbanding, so sampling phase must be measured rather than called “60 Hz physics” by assumption.

| Candidate signal | Quality and caveat |
|---|---|
| `CurrentSpeed`, `CurrentSpeedKPH`, `CurrentSpeedKPHMasked` | Actual simulation/display values, but not interchangeable: masked speed and rubberband additions require comparison to HUD and travelled distance |
| Input getter values | Real normalized player controls; record actual injected value and final filtered value separately |
| `GearBox.CurrentGear`, `NormalizedRPM` | Game-authored **synthetic** gearbox/RPM; inspect indexing before Forza conversion, since Forza gear zero means reverse |
| `Evaluator.TravelledDistance`, travelled percent, lap/race position | Good route-progress candidates; check wrap, finish and race restart semantics |
| `LateralOffset`, `LateralOffsetDelta`, `LastCurvature` | Arcade lane/spline quantities; derivatives can support cues but are not measured tire load or physical yaw rate |
| `IsDrifting`, dirt/curb state, collision callbacks, nitro, fuel | Useful gameplay/events; log transition timing and intensity, not an invented four-wheel model |
| Velocity, acceleration, world pose | Need conversion from the logical curve/lateral state; differentiating the rendered car transform gives misleading forward motion because the road moves around its presentation |
| Tire slip/load, suspension, tire temperature, torque | Not found in the inspected car model; keep unavailable in diagnostics and use documented protocol defaults, not plausible invented measurements |

**Read-only trap:** do **not** call `PlayerCar.FilteredRPM` just to sample it. It calls `RPMHelper.GetRPM`, which updates state and can invoke `CarEngine.ApplyRPMBoost`. Observe the existing normalized gear/RPM state or cache the result of the game's own invocation. Extra getter calls could change gameplay. [Static T4]

Export a versioned plain frame to the native bridge and use `native/forza/forza_packet.h` for a receiver-selected 324- or 311-byte packet. A Forza-compatible dashboard is a good target; physical motion telemetry is not yet justified. Verify packet length, units, race/menu transitions and fresh values with the existing probe before SimHub. Set race telemetry active during countdown as appropriate, but enable wheel forces only while the player controls the car. Idle frames must park the receiver. [Inference based on T4 and toolkit knowledge]

### FFB signal quality

**Inference, strong static basis:** useful arcade FFB is achievable from speed, steering/rate, curve resistance, surface, drift and collision events. Reuse toolkit spring/damper/lateral cue shaping and periodic textures. The game's `HandleCurveForce` applies a curvature/speed/weather-dependent **lateral offset increment**. Its `m_lastCurveLateralForce` includes delta time and is not Newtons or aligning torque; normalize/calibrate before using it, and never feed it to a tire-force preset as if it were `Fy`.

This could produce centering weight, corner load, drift lightening and impact/curb feedback. It cannot recover a tire self-aligning torque model that the inspected simulation does not provide. Physical sign, strength, oscillation, focus-loss recovery and feel remain untested. First FFB development should log computed values with output unopened; use the toolkit's freshness watchdog, zero/exit lifecycle and delivery diagnostics in the eventual consumer.

### Bonnet geometry and rendering feasibility

**Static T5:** this is not merely a flat sprite racing screen. Car views operate on `MeshRenderer` models; the road uses `SkinnedMeshRenderer` and track nodes. However, it is also not an ordinary fixed world/car Rigidbody setup. `CurveSampler` transforms sampled spline points into a moving reference basis; `CurveSamplerToTrack.MainUpdate` sends the relative samples and start/end percentages to `TrackManager.SetTrack`. The visible road and props follow that window, and `TrackView` scrolls material offsets with travelled distance. `PlayerCarTrackView` stays attached near a reference track node plus lateral/vertical displacement.

**Inference:** a modest forward/low offset in the existing player camera frame is credible. A camera attached to an assumed world-space vehicle trajectory is the wrong first implementation. Relevant seams include `CarBasedCamera.ApplyOffset`, the `camera_prefab/camera` child, `CameraMovementController`, and the existing Senna controller. Preserve the correct player's layer mask and camera stack.

The outstanding rendering work is concrete:

- Check the near road beneath a lowered camera, finite sampled horizon and prop spawning through crests, dips and sharp bends; extending the window may be necessary.
- Inspect each ordinary car's hood mesh, winding/backfaces and near clipping. `MeshRenderer` presence does not prove a complete hood viewed from above/inside, and does not prove interiors exist.
- Check background/skydome placement, illustrated horizons, shaders and any camera-facing details. The inspected code only explicitly identified UI billboards; the scene assets were not exhaustively classified as mesh versus billboard/sprite.
- Preserve mirror-mode projection/culling behavior, split-screen layer masks, start/finish cinematics, nitro effects and rain. The Senna implementation explicitly swaps cameras, model parts, HUD/rain and collectible scale; copying its position alone misses those behaviors.
- Test road and pickup visibility at normal racing speed. A mathematically valid low camera may hide the next curve behind hills and make the game unpleasant to drive.

Senna's shipped cockpit demonstrates an intentionally engineered low viewpoint for its own content. It does not prove a general bonnet camera for every standard car/track. A bumper view is an easier fallback if the hood is incomplete; a cockpit/interior reconstruction is outside this estimate.

### Existing integrations and smallest decisive experiment

A player reports T150 input through [XOutput in this Steam discussion](https://steamcommunity.com/app/389140/discussions/0/1696045708646596837/). This is a **community** fallback for controller emulation, not evidence of direct FFB or R12 compatibility. The [original XOutput project](https://github.com/csutorasa/XOutput) was archived on 2024-12-03 and identifies its emulation dependencies as deprecated; it is a source/reference candidate, not a maintained new-install recommendation. The owner's adjacent xoutput-redux may supply existing setup experience, but was not inspected in this task.

Astien's Turbo VR mod is reported in a [2025-06-09 gameplay/community post](https://www.reddit.com/r/virtualreality/comments/1l7ajxa). The post specifically limits first-person play to Senna content. Original mod source, license, supported build and a stable direct download were not verified, so it is a compatibility/research lead, not code ready to vendor. The public [NextOS Horizon Chase loader](https://github.com/NextOs-Ports/horizonchase-nextos) concerns **Android Horizon Chase version 2.6.9**, not Horizon Chase 2 or this Windows Turbo build. Its input-boundary work is conceptual evidence only; its loader/offsets are not portable here.

**Next experiment, proposed only:** in a later authorized per-game task, load a net35 x86 diagnostic plugin with all device APIs disabled. Count `PlayerCar.Update` and the three input-getter calls; log selected-player speed, normalized controls, normalized RPM and race state for one ordinary race. Add a reversible low-camera toggle using the existing camera frame and record one straight, a crest and a sharp bend with the ordinary car visible. Pass requires live values with correct units/player identity, stable menu/finish behavior, and a continuous readable road with a visible hood. This one experiment settles the loader/runtime bridge and the largest camera uncertainty. After that, add no-force device calibration and packet verification before any attended force test.

## Horizon Chase 2

### Distribution, availability and engine evidence

**No Windows binary was inspected.** The coordinator found no Steam/Epic install; this task additionally found no matching Epic `.item` manifest, no sequel folder among the Steam common-directory Horizon matches, and no `%LOCALAPPDATA%\HorizonChase2` directory. This limited search is not proof the game is absent from every disk or library.

The verified PC distribution is the **Epic Games Store Windows product**, whose current page offers purchase and gives PC release date **2023-09-14**. It lists Windows 10, 15 GB storage and an Epic ID requirement. Turbo's delisting does not imply the sequel is delisted. No purchase, ownership check, download, current build number, executable hash or redownload test was performed. [Current Epic product page](https://store.epicgames.com/p/horizon-chase-2?lang=en-US) [Primary]. Do not assign this game Turbo's Steam app ID or invent a Steam build.

**Primary engine evidence:** engineer Liam Dunstan identifies his Horizon Chase 2 console-port work as **C++ / Unreal 3D**. This independently distinguishes the sequel from Turbo's Unity implementation, but does not establish the exact Windows engine patch version. [Developer portfolio](https://liamdunstan.com/games/) [work dated 2023-08 to 2023-11].

**Inference:** a native Unreal Windows build, probably Win64, is the planning baseline. **Unknown:** exact engine major/minor/patch, PE architecture, shipping executable name, reflection layout, archive format/encryption, DRM/anti-cheat and local mod-loader compatibility. Community UE4/WindowsNoEditor references are leads, not a substitute for inspecting a PC binary. Never infer UE4 version from the 3D art or Epic ownership.

### Existing feature support and owner rig implications

Epic explicitly says Windows-compatible controllers can be used. That broad answer does not specify multi-device DirectInput wheels, torque output, per-axis calibration or H-pattern shifting. No primary wheel/FFB support matrix or public driving-telemetry API was found in the checked store/support material. Standard chase-view presentation is documented; no primary source reviewed advertises a bonnet/cockpit mode. Treat the absence of those advertised features as **unverified absence**, not a tested claim about the complete settings menu. [Epic controller FAQ](https://www.epicgames.com/help/c-202300000001631/c-202300000001705/kan-jag-spela-horizon-chase-2-med-en-handkontroll-pa-windows-a202300000020288) [Primary].

Proportional gamepad steering is plausible, but the exact steering-to-movement relationship and analog versus thresholded throttle/brake are unknown. Separate pedals and DS-8X/stalk inputs need independent readers or a virtual-controller route; manual transmission/clutch mechanics are unverified. Do not promise H-pattern driving merely because the physical shifter can enumerate.

### Loader, telemetry and FFB proposal

**Inference:** after confirming PE/runtime evidence, try UE4SS for reflected object discovery and hooks. Its original project supports Lua/C++ plugins, SDK generation and property inspection, and explicitly cautions that games can need custom signatures. A native proxy is a fallback only after checking which DLL the shipping executable actually loads. Do not presume `dinput8.dll` is a valid injection point. [Original UE4SS project](https://github.com/UE4SS-RE/RE-UE4SS).

Locate the local player's controller/pawn and its movement component; inspect mapped input functions and the HUD speed source. Search for velocity/route progress, lateral displacement or curvature, acceleration/braking requests, nitro, collision events, surface/drift state and race phase. These are **candidate signal classes**, not discovered game member names. Unreal does not imply Chaos/PhysX wheel simulation: the game could use a custom arcade or spline controller.

If the car has meaningful world velocity, derive local longitudinal/lateral components and filtered acceleration after confirming units and axes. If its velocity is presentation-only, follow the gameplay speed/HUD source. RPM/gear can be omitted or explicitly synthesized; tire loads/slips must remain unavailable until actual fields and semantics are demonstrated. Use a native consumer sampler plus the toolkit C++ Forza encoder, then verify a basic dashboard on a mirror port. No existing SimHub/telemetry integration was verified.

FFB has two distinct branches: actual tire/lateral forces, **if found and validated**, can feed a physical proxy model; otherwise use synthetic centering/damping plus measured speed/turn/drift/surface/impact cues. The latter is plausible and potentially enjoyable, but engine choice and 3D geometry provide no evidence of tire-force quality. FFB acceptance and R12 feel need separate tests.

The store advertises online multiplayer and leaderboards. Hook discovery should begin in solo play with output disabled. Determine reliable race/session gating and the installed protection stack before deploying an input or camera mod. Do not alter multiplayer state or assume solo races cannot submit records. [Epic product page](https://store.epicgames.com/p/horizon-chase-2?lang=en-US).

### Bonnet camera: more credible geometry, still a real test

The game's concept artist explains that the sequel uses a **3D map with cars moving through it**, explicitly contrasting it with Turbo, while combining 3D assets with illustrated backgrounds. This is direct development evidence, not an inference from screenshots. The 2022-12-12 article was available in the search index during research, but its current URL redirected to the home page during a direct fetch; a reader fallback required authentication, so no fresh archived body was obtained. [Danilo Freitas, The Art of Horizon Chase 2](https://www.horizonchase.com/blog/the-art-of-horizon-chase-2). The current [Epic description](https://store.epicgames.com/p/horizon-chase-2?lang=en-US) also describes a 3D world.

Lead level designer Caio Prates explicitly describes camera angles and verticality as tools for revealing/hiding the next part of a course. That is a specific warning against assuming bonnet visibility will match chase visibility, particularly on Japan's hills. [The Making of Japan, 2022-12-20](https://www.horizonchase.com/blog/the-making-of-japan) [Primary].

**Inference:** a car-relative camera or player-camera-manager override is technically credible once the actual rig is discovered. Check whether the camera follows the vehicle forward vector, route tangent or a blend. Avoid inheriting purely decorative body yaw/roll that would point the bonnet camera away from the road. A spring arm, if present, may push the camera outside the intended position.

Required observations: full hood/front-body mesh and LODs; backfaces and near clip; any owner-hidden parts or first-person-only mesh flags; actor/distance/occlusion culling after the camera moves; track visibility over hills and forks; spawn/streaming ranges; illustrated horizon parallax and billboards; HUD, speed streaks, weather, boost FOV and cinematic resets. No local mesh topology or render capture was available. A 3D map makes the road projection risk lower than Turbo's custom presentation; it does not prove cockpit interiors, unrestricted geometry or good racing visibility.

[Praydog's original UEVR project](https://github.com/praydog/UEVR) provides Unreal camera offsets, user-authored first-person conversion and a plugin system. It is a useful independent camera experiment/reuse candidate, not proof of compatibility with this uninspected build. Community UEVR lists mention the sequel, but no versioned original per-game profile/source was validated here. A later flat-screen camera mod need not require a VR runtime.

### Blockers and smallest decisive experiment

The immediate blocker is an authorized installed PC build, not a conclusion that modding is impossible. First record launcher manifest/version, main executable SHA-256 and PE machine, engine/build strings, archive layout and protection dependencies. Then, in a later authorized solo session, prove one UE4SS/native hook heartbeat and identify the local player's actual steering/throttle/brake and speed source. With wheel APIs disabled, move the existing camera to a reversible hood position for a straight, sharp corner and major crest. Log whether velocity is physical, whether analog pedals survive fractional inputs, and whether all needed geometry remains visible. That turns four medium/unknown ratings into evidence quickly; no trainer, entitlement bypass or gameplay modification is needed for discovery.

## Owner hardware and shared work

| Hardware | Turbo | Horizon Chase 2 |
|---|---|---|
| MOZA R12 | Native toolkit x86 reader/FFB route is credible; stock PC wheel interface is null. No title-specific test | Native toolkit route conditional on architecture/hook discovery; no stock or mod compatibility test |
| Separate pedals | Concrete float accelerator/brake seam; calibrate each device/axis independently | Feasible reader strategy; actual proportional pedal consumption still unknown |
| SIMAGIC DS-8X | Separate button device can map existing actions; no true manual transmission in inspected pipeline | Separate button reader plausible; gearbox/action semantics not established |
| MOZA stalk | Button mapping to honk, camera or menus where appropriate; real indicator/wiper mechanics not established | Button mapping possible after action discovery; real vehicle functions not established |

Repository hardware knowledge records DS-8X as an 11-button device and the stalk as a separate 32-button device, with R12-specific force encoding behavior. Those are prior hardware observations, not measurements repeated here. Persist GUIDs per role, permit non-FFB devices for pedals/accessories, retain missing bindings, and avoid selecting virtual pads for force output. Separate pedal model, connection mode and travel are unspecified; do not guess polarity from brand. [Device knowledge](../../../knowledge/DEVICE-QUIRKS.md) [Static repository evidence].

Shared opportunities are the native device library, force-model/profile data, packet encoder, no-force calibration design, identity persistence, input/output diagnostics and packaging conventions. Turbo may justify a reusable legacy-net35/native bridge in future, but it is **not already shipped**. The games' loaders, signal normalization, player selection and camera code should remain distinct. Any common feel profile needs separate tuning and sign verification. Do not copy Turbo's curve/gear assumptions into the sequel.

## Effort ranges and assumptions

Estimates are **engineering judgment**, not quotes or measured delivery times. One engineer familiar with Harmony/Unreal and this toolkit; 6–8 focused hours per person-day; legitimate game access; one local player and one PC build first; no DRM workarounds, new physics, reconstructed interiors, VR product, or all-wheel/all-store support. Early proofs may fail and change the estimate.

| Work | Turbo | Horizon Chase 2 |
|---|---|---|
| Build/runtime fingerprint + loader heartbeat | 0.5–2 days, plus **1–3 days** for the legacy/native bridge | 1–4 days after install access; more if reflection/signatures differ |
| Steering + independent throttle/brake proof | 1–2 days after loader; 4–8 days with setup/reconnect UI | 2–5 days after loader; 6–12 days with setup, assuming a usable float input seam |
| Basic telemetry proof | 1–2 days after bridge; 3–6 days with state/unit/receiver validation | 2–5 days after discovery; 5–10 days with validation, assuming speed/race state are exposed |
| Arcade FFB proof and attended tuning | 2–4 days after telemetry; 5–10 days including lifecycle and feel | 3–7 days after telemetry; 7–15 days including signal discovery and feel |
| Bonnet-camera proof and initial polish | 1–3 days for one car/track; 5–12 days for ordinary content and transitions; larger if road-window artifacts need changes | 1–3 days after rig discovery; 4–10 days across representative courses/cars; larger if hidden geometry/streaming intervenes |
| Combined first supported single-build release | Roughly **3–6 person-weeks**, sharing milestones above | Roughly **4–8 person-weeks after access**, with wider uncertainty |

Rows share loader/sampler work and must not simply be summed. True manual transmission/clutch, physical tire telemetry absent from the game, and a full cockpit are not included. The next investment should be the small diagnostic/camera experiments above, before promising a complete wheel mod.

## Reproducibility and research limits

Only this Markdown report is a tracked output. Static decompilation and fetch scratch are outside tracked docs at `C:\Users\antho\AppData\Local\Temp\dbce-horizon-research-20260907`; proprietary code/assets are not reproduced in this report. T2–T5 can be reproduced by inspecting the named types in the T1 assembly with `ilspycmd`, without executing game code. Exact bytes/hashes and type-level evidence take precedence over general knowledge-page statements about modern Unity Mono compatibility.

No game-specific wheel/FFB/telemetry mod source was verified for either current Windows build. Community VR/controller observations were retained as leads and not used to establish tested compatibility. Current Epic purchasing availability and current Steam unavailability are primary-source observations; future store policies, server operation, ownership and all-platform delisting behavior are outside what this inspection proved. [dbce-wheel-mod-toolkit, 2026-09-07]
