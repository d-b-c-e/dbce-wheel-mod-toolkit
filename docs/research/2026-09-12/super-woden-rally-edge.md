# Super Woden: Rally Edge — Windows mod feasibility

Assessment date: **2026-09-12**. Provenance for local observations and engineering
judgments: **[dbce-wheel-mod-toolkit, 2026-09-12]**. Web sources accessed on that
date unless stated otherwise.

**Recommendation: put this near the top of the new-mod shortlist, alongside
Horizon Chase Turbo.** The installed build has a familiar Unity 6 IL2CPP runtime,
identifiable analog input fields and methods, a Rigidbody vehicle with explicit
front/rear WheelColliders, and a configurable 3D camera controller. That is a
better starting point for physics-informed FFB and a bonnet view than the purely
projected/sprite approach in Slipstream. A small runtime proof is still needed
before ranking it conclusively ahead of Turbo.

This is a completed **initial feasibility assessment**, using web research and
read-only inspection of the installed executable, IL2CPP metadata and native
registration tables. No game was launched, game DLL loaded as executable code,
device acquired, force applied, mod deployed or installed configuration changed.
No loader, input override, telemetry stream or camera view has run in this study.

## Four requested features

Ratings describe engineering feasibility, not working support. High means a
concrete static route exists; medium means important implementation questions
remain. Confidence in declarations is higher than confidence in runtime behavior.

| Feature | Existing support / evidence | Mod feasibility | Main unresolved question |
|---|---|---|---|
| **Wheel input** | Gamepad-oriented input system. Logitech reader/wrapper components exist, but their activation and hardware coverage are unverified. | **High; medium-high confidence.** Separate float steering/throttle/brake fields and a `Steer(float)` method are present. | Input update order, deadzones/filtering, effect of steering assists, and real mixed-device behavior. |
| **Telemetry** | No documented public exporter or current SimHub integration was found. | **High for basic dashboard data; medium for richer wheel data.** Car, Rigidbody, gear/RPM, race state and WheelHit references are identified. | Actual units, sample timing, live field population, correct player, and reset/pause semantics. |
| **FFB** | Logitech-specific code and spring configuration are present; functioning directional wheel FFB was not established. Controller rumble is a separate path. | **Medium-high for physics-informed arcade FFB.** Wheel contact/slip data offer more than speed-only synthetic cues. | Available force semantics and physical feel; contact-force magnitude is not steering-rack torque. |
| **Bonnet camera** | Native chase/isometric/photo/replay camera machinery exists. A hidden behind-car mode is documented by players. No bonnet preset was verified. | **Medium-high for a proof; medium for a polished feature.** Typed camera presets include height, distance, angle and car attachment options. | Low-angle hood/road visibility, culling, scenery, clip planes and camera takeover during transitions. |

## Exact installed build and method

| Item | Verified local evidence |
|---|---|
| Distribution | Windows Steam app **3218630**, retail manifest; `D:\Program Files (x86)\Steam\steamapps\common\Super Woden Rally Edge` |
| Manifest | `appmanifest_3218630.acf`, installed state 4, build **21802346**; content manifest **223460804701382137** |
| Unity | `UnityPlayer.dll` product version **6000.3.6f1 (bbb010bdb8a3)**; file version **6000.3.6.12300304** |
| Architecture | Game executable, UnityPlayer and GameAssembly each have PE machine **0x8664 / AMD64** |
| Scripting | `GameAssembly.dll` plus `Super Woden Rally Edge_Data\il2cpp_data\Metadata\global-metadata.dat`; IL2CPP metadata magic **0xFAB11BAF**, version **39** |
| Input/graphics context | Surviving native code modules include `Unity.InputSystem.dll`; `GamePadSystem.Controllers` is `UnityEngine.InputSystem.Gamepad[]`. D3D12 files are present, but the active rendering API was not tested. |
| Static analysis | Existing iRacing Arcade BepInEx #788 `LibCpp2IL.dll` successfully read the metadata and native registration tables: **9,751 types**, **118,512 methods**, including **820 types** in the two game assemblies. |
| Symbols | Partial name obfuscation: many methods have scrambled names, while key Unity callbacks, vehicle types and serialized fields remain readable. Native addresses/types resolved offline; no hook execution was tested. |

SHA-256 fingerprints:

- `Super Woden Rally Edge.exe` (667,648 bytes): `fa0da16cdc44a0ed6e6cfb2eaa3c87d6698aeee10f46c16a475c9db97165b9a5`
- `UnityPlayer.dll` (35,718,568 bytes): `9feedb4527a30fae4762bff45d204f8e85b90851b2c8ae8d24f436f7d474667b`
- `GameAssembly.dll` (56,622,592 bytes): `f422894d8d2b0df4edb7e5259e5e60cb8c4f8dea2e85ebdfc09dd6766349250c`
- `global-metadata.dat` (11,062,704 bytes): `3b5d7167aea83febdc625f1635fc7cb414ff13ca609d78cc32158133c3877815`

The installed root contains no BepInEx/MelonLoader deployment. Assembly names in
`ScriptingAssemblies.json` describe source assemblies; they do not imply readable
Mono DLLs exist. The inspected build is IL2CPP. Its Unity version and runtime were
established from local files, not from storefront tags or another Woden game.

The [developer-authored Steam page](https://store.steampowered.com/app/3218630/Super_Woden_Rally_Edge/)
identifies ViJuDa, a 2026-01-15 release, split-screen for up to four players, photo
mode and online records. The [developer news feed](https://steamcommunity.com/app/3218630/allnews/)
documents a 2026-02-05 custom-livery update. Livery customization is not a code
plugin API; no official code-mod SDK was established.

## Loader and toolkit fit

**Proposed loader: BepInEx 6 IL2CPP, Windows x64, with metadata-v39 support.**
The [official IL2CPP installation guide](https://docs.bepinex.dev/master/articles/user_guide/installation/unity_il2cpp.html)
specifies the separate IL2CPP build family and architecture choice. Repository
experience in [Unity IL2CPP/BepInEx](../../../knowledge/UNITY-IL2CPP-BEPINEX.md)
provides a close starting point: Unity 6, metadata 39, BepInEx #788 and CoreCLR.
The successful offline use of that package's parser reduces a compatibility
unknown; it does not prove chainloader startup, interop generation or Harmony
patching in this game.

Reuse the toolkit's netstandard2.0 telemetry/FFB assemblies from the consumer's
CoreCLR plugin, with the exact-module native binding and x64 device DLL. Unlike
Turbo, a legacy net35 bridge is not indicated. Re-check current BepInEx package
requirements at implementation time rather than assuming a historical build is
the only option. Unity Mod Manager's Mono assembly-patch route is inappropriate.

Count callbacks before trusting hooks. IL2CPP can inline or share methods, and
the obfuscated helper names are fragile across updates. Initial single-player
targets include `MainCar.FixedUpdate`, `Controls.FixedUpdate` and
`Car_Cam.LateUpdate`. Fixed-update sampling phase must be measured; a postfix
can still run before Unity's next physics solve. Do not assume it is already
a coherent post-physics frame.

The following **static RVAs** were resolved against the fingerprinted
`GameAssembly.dll` (preferred image base `0x180000000`). They are reproducibility
anchors, not tested detour addresses or a stable interface:

| Method | RVA | Verified signature |
|---|---|---|
| `MainCar.FixedUpdate` | `0x4FD330` | `void ()` |
| `MainCar.Steer` | `0x500910` | `void (float)` |
| `MainCar.Brake` | `0x4FCA00` | `void (bool, float)`; parameter meanings not resolved |
| `MainCar.Shift` | `0x4FFBE0` | `void (int)`; index/direction semantics not resolved |
| `Controls.FixedUpdate` | `0x9A41F0` | `void ()` |
| `Car_Cam.LateUpdate` | `0x8557E0` | `void ()` |
| `Logitech.SteeringWheelReader.FixedUpdate` | `0x86F480` | `void ()` |

## Wheel input and the owner's devices

**Static evidence:** `Controls` has `Steering_float`, `Pedal_Acc` and `Pedal_Bra`
as `System.Single`, plus gear-up/down, handbrake, headlights, horn, camera,
rear-view, restore and pause actions. `MainCar` has separate float
`Steering_Wheel_Pos`, `Pedal_Acc_Value` and `Pedal_Brake_Value`, a `MyControls`
reference, `IsPlayer` and `PlayerIndex`. `GamePadSystem` stores Unity Input System
gamepads, deadzone/sensitivity values and controller rumble settings.

This establishes real analog storage and a float steering method, but not the
exact transfer function. `SteerSys` includes speed/gas-dependent helper curves,
autocenter speed, torque helpers and a maximum steering angle. `DriveAssists`
contains countersteer, skid-control and other assistance settings. Initial
wheel work should feed the intended input boundary and retain those settings;
silently changing assists would be a separate gameplay change.

**Proposed route:** directly read selected physical devices with the toolkit,
then override only the chosen local player's controls at the demonstrated point
before the car consumes them. A `Controls.FixedUpdate` postfix is a candidate,
not yet proven to precede `MainCar` consumption. Do not simply call `Steer` an
extra time per frame or write fields after the physics work and assume it is
equivalent. Test distinct 25/50/75% values and simultaneous throttle/brake.

The game ships Logitech native plugins and typed `Logitech.SteeringWheelReader`
and `Logitech.LogitechSteeringWheel` components. The reader references `Player1Car`,
normalized steering, accelerator/brake, wheel index, detection status and spring
parameters. Its enable/fixed-update/quit callbacks resolve to native code.
**Whether it is instantiated/enabled, which devices it admits, and what effects
it actually sends remain unknown.** This is more than a stray DLL, but still
not proof of a functioning hidden wheel feature or MOZA support.

Firsthand January reports in the [wheel discussion](https://steamcommunity.com/app/3218630/discussions/0/682991471067102029/)
describe Logitech/Fanatec failures. A [T300 user on page 2](https://steamcommunity.com/app/3218630/discussions/0/682991471067102029/?ctp=2)
reports working input through x360ce and explicitly reports no FFB. These posts
precede the inspected February build and do not identify it. They are useful
historical observations, not a current support matrix. A remembered developer
tweet in that thread is not a verified promise of future support.

| Hardware role | Assessment |
|---|---|
| MOZA R12 | DirectInput reader/output through the toolkit is credible. Calibration, rotation mapping, strict identity and actual force behavior need testing. Logitech-specific activation is not an R12 solution by itself. |
| Separate pedals | Independent analog destinations exist. Provide separate GUID-backed readers and per-axis travel/inversion calibration; the exact pedal model/polarity was not established. |
| SIMAGIC DS-8X | Sequential actions are identified. `Shift(int)`, gearbox arrays and `MyGear` make further investigation worthwhile, but the integer may mean an increment rather than an absolute gear. H-pattern, neutral/reverse and clutch behavior are unverified. |
| MOZA stalk | Headlight, horn and camera actions provide useful possible bindings. No turn-indicator or wiper gameplay action was established. |

A finished shared calibration/input kit is not shipped by this toolkit. Input
role UI, calibration and game-specific binding semantics are consumer work.

## Telemetry and force signals

No public UDP/shared-memory format, enabling option or code-backed game-specific
telemetry mod was verified. The game is absent from the inspected
[SimHub supported-games list](https://www.simhubdash.com/supported-games/).
Searches of the readable game type/method names also found no telemetry/Forza/UDP
exporter names, but obfuscation makes that a weak negative test. Do not invent a
native port or protocol. No network capture was performed.

| Candidate data | Concrete static evidence | Validation required |
|---|---|---|
| Speed and pose | `MainCar.Rb` is a `Rigidbody`; `TrueSpeed` is an integer; `SpeedMomentum` is float | Compare body velocity, display speed and stage movement; verify units and effects of speed multipliers. |
| Steering and pedals | Typed float fields in `Controls` and `MainCar` | Distinguish requested input from final filtered/applied input. |
| RPM and gear | `Rpm` float, `MyGear`/`Gears` integers, transmission arrays | Determine normalized versus actual RPM and indexing/reverse semantics; validate against HUD. |
| Wheel contact/slip | `WheelData_` contains `WheelCollider[]`, explicit front/rear arrays and `WheelHit[]` | Establish contact freshness, wheel ordering and whether cached hits are populated. Query surviving `GetGroundHit` if needed. |
| Wheel rotation and suspension | WheelCollider and transform references | Confirm accessible surviving APIs and derive travel only from validated geometry/state. |
| Race/lap/state | `CarStatus` has WARMING/RACE/END/DESTROYED; `Clock`, `Laps`, `CheckPoint`, `Replay`, `Respawning`; `Pause.Paused` exists | Correct local player; countdown, finish/autopilot, pause/photo, replay and respawn transitions. |
| Fuel/damage/temperature | `StatusParameters` has Fuel, Life, Oil, Temperature and G_Force | Their units and whether they represent physical quantities or gameplay meters remain unknown. |

A new sampler could feed `Dbce.Wheel.Telemetry` and its Forza-compatible packet
encoder. Start with speed, controls, gear/RPM and active-driving state; add
per-wheel channels only after verification. Export to a configurable receiver
address/port, validate packet length and units with the toolkit probe, and park
output between events. Do not label derived acceleration or inferred wheel
loads as game-provided measurements.

For **FFB**, the promising source is validated front-wheel slip/contact state
and vehicle motion, with explicit synthesized centering, damping, surface and
impact cues where appropriate. Unity's [WheelHit documentation](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/WheelHit.html)
defines directional slip and **contact-force magnitude**, not signed lateral
tyre force or steering-rack torque. A slip/contact-based force model therefore
remains a modeled approximation. Neither WheelCollider presence nor an
`ActualPower` field proves physical self-aligning torque is available.

Existing spring parameters do not prove rich native FFB. First inspect whether
the Logitech component actually outputs anything useful. If it does, choose one
force owner and suppress conflicting output before opening the toolkit device.
Keep gamepad rumble distinct from wheel torque. Record signal liveness,
rejection, computed output and API delivery separately; apply the toolkit's
freshness, zero-on-pause/exit, ramp and recovery rules. No feel/sign assessment
was made on the R12.

## Bonnet camera and rendering

**A hidden native chase view is already documented**, rather than requiring a
new third-person camera mod. The [original Steam guide](https://steamcommunity.com/sharedfiles/filedetails/?id=3651982159)
(posted 2026-01-22, updated 2026-01-30) describes an `Unknown` option in the hidden
BIOS menu that adds a behind-car view. The [camera discussion](https://steamcommunity.com/app/3218630/discussions/0/682991471066942396/)
contains firsthand February confirmations. The guide's claims about unlocking
conditions and leaderboard/achievement effects were not tested here.

Local evidence corroborates the camera architecture: `Car_Cam.Cam_Mode` has
Chase, IsoMetric, Photo and Replay; `Bios_Access` exists. `Car_Cam` references
the player car/transform and `Chase_Presets`. A `CamPreset` has typed height,
distance, angle, rotation offset, azimuth scale and `UseCam_Link`; `MainCar`
has a `Cam_Link` transform. `Chase_M_Values` contains FOV, speed-FOV curve,
clipping distance and height adjustments. `ChangeCamera`, `LateUpdate` and
camera setup methods were identified.

No preset named bonnet/hood/bumper was verified, and the runtime preset values
were not read. A bonnet mode might be another preset or a late camera override
using the car attachment; the actual sign/reference frame must be established.
Prefer integrating with the native camera cycle, retaining split-screen player
ownership and yielding to photo/replay/finish scenes.

Unlike Slipstream's sprite cars, this game's vehicle declarations reference
`MeshRenderer` objects, 3D transforms and physics. Its custom-livery templates
and shipped chase camera provide additional evidence that a 3D view is credible.
**Actual mesh topology, scenery culling and bonnet visibility were not inspected
or rendered.** A low view must still be tested for:

- Hood/front-body completeness, backfaces, near clipping, damage/mesh changes
  and car-specific proportions; no cockpit interior is assumed.
- Crests, hairpins, trackside foliage, billboards, fog and distant scenery that
  may have been designed for a higher view.
- Camera-relative culling/LOD and the meaning of the car's `CullSphere` object;
  its name does not establish its behavior.
- Shake, dynamic FOV, vehicle yaw/roll, camera smoothing, respawn jumps and
  changes between driving, replay, menus and photo mode.

Reported camera modifications elsewhere sometimes refer to **Super Woden GP 2**
or to this native hidden-mode toggle. No original-source Rally Edge wheel,
telemetry or bonnet mod was verified for build 21802346. Treat those reports as
leads rather than an existing codebase ready to reuse.

## Smallest decisive experiment and effort

**Next step: one game-specific diagnostic plugin, with wheel output disabled.**
Prove the BepInEx load/interop route, count the three key callbacks, identify a
single local `MainCar`, and log controls, Rigidbody speed, gear/RPM, wheel contacts
and state transitions. Check update order before any input override. Add a
reversible one-car bonnet toggle and inspect a straight, crest and hairpin.
This directly addresses the main uncertainties without building a full product.

Provisional effort for one developer familiar with Unity IL2CPP and this toolkit,
one Windows build and initial single-player scope:

| Work | Rough engineering time, conditional on successful preceding discovery |
|---|---|
| Loader, interop and read-only state proof | **1-3 days**; more if obfuscation/interop breaks useful types |
| Analog wheel + separate pedals proof | **2-5 days** after loader; **5-10 days** with role selection/calibration/reconnect behavior |
| Basic telemetry | **1-3 days** for a proof; **3-7 days** with units, state lifecycle and receiver validation |
| Physics-informed arcade FFB | **3-7 days** for a signal/delivery prototype, then attended tuning and lifecycle testing |
| Bonnet view | **1-3 days** to settle viability; **4-10 days** across representative cars, courses and transitions if geometry cooperates |
| Combined first supported build | Approximately **3-6 person-weeks**, sharing the work above; not a delivery commitment |

These estimates overlap and should not be summed. H-pattern/clutch additions,
physics changes, reconstructed interiors, multiple store versions, broad hardware
support and complete split-screen coverage are excluded. An obfuscated call
chain, dead/inlined hooks or missing low-view content can change the scope.

The main reason to prioritize this experiment is the combination of familiar
runtime tooling, concrete analog input signatures, real wheel-contact API types
and native camera presets. It has more direct local evidence than Horizon Chase
2 or Origins, while avoiding Turbo's legacy managed-runtime bridge.

## Repository boundary and reproducibility

Reviewed toolkit [STATE](../../STATE.md), [consumer guidance](../../CONSUMER-EXPERIENCE.md),
[Unity knowledge](../../../knowledge/UNITY-MODDING.md), IL2CPP notes and force-signal
guidance. Toolkit source/package **0.12.0** and native component **0.5.0** remain
separate versions. The shared input/calibration kit and SDL backend remain future
work. Future game hooks, samplers, camera code and online gates belong in a
per-game repository, reusing the toolkit's device/FFB/telemetry layers.

The store documents online records. Preserve the game's physics/assists and
establish recording/session gates if future development changes competitive
behavior. The community hidden-menu guide is not authorization or evidence for
leaderboard-safe mod behavior.

Scratch analysis is at
`C:\Users\antho\AppData\Local\Temp\dbce-woden-research-20260912`.
`metadata/MetadataAudit.csproj` uses the existing
`E:\Source\iracing-arcade-wheel\lib\core\LibCpp2IL.dll` and its dependencies.
It reads files as byte arrays with `LibCpp2IlMain.LoadFromFile`; it never executes
the game's native DLL. `game-types.json` contains the static type/signature
inventory. No proprietary method bodies or game assets are copied into the
repository. Browser-source recovery used a public-page curl fetch after the
Steam pagination URL failed in the web tool; no account was accessed.

Research is complete at the static-feasibility level. Loader acceptance, live
signal quality, telemetry on the wire, bonnet rendering and the owner's physical
wheel experience remain unverified.
