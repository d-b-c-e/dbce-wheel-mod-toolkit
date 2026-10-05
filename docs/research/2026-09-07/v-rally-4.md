# V-Rally 4 — Windows mod feasibility

Research date: **2026-09-07**. Provenance for this assessment and all local observations: **[dbce-wheel-mod-toolkit, 2026-09-07]**. All external sources below were accessed on that date; publication dates are given where established. This is an initial feasibility assessment, not gameplay or hardware validation.

**Recommendation:** start by checking the existing controls and FFB on the owner's rig. The game already implements analog wheel input, FFB and a bonnet camera. A modern-device compatibility fix may be small, or unnecessary. **Telemetry is the main missing feature worth a dedicated proof of concept.** No public telemetry route was verified for this exact build, and the WRC telemetry patch is useful prior art, not a confirmed V-Rally 4 integration.

## Scope and feature ratings

Read first: [STATE](../../STATE.md), [consumer experience guidance](../../CONSUMER-EXPERIENCE.md), AGENTS.md, and the knowledge pages on new games, DirectInput proxies, device quirks, FFB signals, Forza packets and FMOD telemetry. Toolkit source/package **0.12.0** and native device component **0.5.0** are separate versions. The shared input kit and SDL backend are **not implemented**; the native DirectInput proxy header scaffold is implemented and tested offline.

No game was launched, no device acquired, no force applied, no mod deployed, and no installed game or user setting changed. Work consisted of filesystem reads, PE metadata/import/export/string inspection, archive-index inspection, in-memory decompression of two camera configuration records, and current web/original-source research. Scratch scripts, downloaded public source and a temporary LZ4 library stayed outside tracked documentation. No proprietary code or assets are included here.

Ratings: **A** = already supplied, verify configuration; **B** = favourable bounded compatibility work; **C** = plausible research project with an unresolved signal/hook; **D** = major missing rendering/data capability. Confidence refers to the assessment, not tested hardware compatibility.

| Requested feature | Existing support | Rating and confidence | Practical decision |
|---|---|---|---|
| Wheel/input | Native DirectInput, configurable analog axes, independent-device presets and direct gear actions | **A/B; high** that the infrastructure exists, **medium** for this mixed rig | Try native binding first. Diagnose any failure before changing enumeration or device identity. |
| FFB | Native SAT, spring/centering, damping and event/vibration configuration | **A/B; high** for implementation, **medium** for R12 delivery and feel | Preserve the native signal. Configuration or effect-encoding repair is preferable to inventing replacement steering forces. |
| Telemetry | D-BOX motion integration; no verified public UDP/shared-memory export for this build | **C; medium** for a basic extraction route, **low–medium** for rich tyre data | Prove local-car speed/RPM/gear and lifecycle first through an FMOD or native sampler. |
| Bonnet/hood camera | Native bonnet configuration explicitly present, including dedicated car rendering | **A; high** for existence | No new camera is required to satisfy “if absent.” Inspect native view in a later attended session; adjustment remains optional. |

## Exact installation and engine evidence

The inspected distribution is **Steam Windows, app 658700, manifest build 3570632**, under `D:\Program Files (x86)\Steam\steamapps\common\VR4`. Its installed content depot is **658701**, manifest **5714683614672803886**. This establishes the manifest/build inspected; Steam content verification was not run, and the installation is not asserted to be pristine. Other stores and console builds were not inspected.

| Local fingerprint | Observation |
|---|---|
| Executable | `VRally4.exe`, **23,709,392 bytes** |
| SHA-256 | `dbc8951800d9bb6727657413665373a7ef279a2d76b6b562acd033a395762622` |
| PE architecture/runtime | Machine **0x8664 (AMD64)**; CLR directory is zero; MSVC native imports including `MSVCP140.dll`/`VCRUNTIME140.dll` |
| PE timestamp | **2019-02-15 14:46:36 UTC**, header metadata rather than a verified release date |
| Graphics/input | Imports `D3D11CreateDeviceAndSwapChain`, DXGI and `DINPUT8.dll!DirectInput8Create`; HID and SetupAPI also imported |
| Physics/audio | `PhysX64_s.dll`, `PhysXCommon64_s.dll`, `PhysXCooking64_s.dll`, `PxFoundation64_s.dll`, `fmod64.dll`, `fmodstudio64.dll` |
| Symbols and strings | **18,872 named PE exports**, largely PlayAll/KTU engine facilities; names such as `VRPhysicWheel`, `VRCameraModeMgr` and `VRCarBaseDynaValues` in strings; embedded x64 PDB path ending `VRally464_s.pdb` |
| Assets | 33 `PPKG` version-2 archives under `WIN32\PKG`; 30,995 index entries across base/patch archives, including duplicates; loose input configuration under `Common\Settings` |

The publisher's [Steam product description](https://store.steampowered.com/app/658700/VRally_4/) explicitly identifies **KT Engine** and KT Racing/Kylotonn, and specifies 64-bit Windows and DirectX 11. The binary evidence independently supports native x64 with proprietary PlayAll/KTU components. **No exact internal KT Engine release number was established.** The folder named `WIN32` does not make this an x86 game, and bundled PhysX does not make it Unreal. Unity/BepInEx, Harmony, UE object scanning and WRC-branded assumptions are inappropriate without separate evidence.

Static code-reading limitation: `.text` has approximately **8.00 bits/byte entropy**, the entry point lies in `.bind`, and sample exported function bodies did not decode as ordinary callable prologues from the on-disk image. This is consistent with a protected/transformed executable, but the precise protection was not identified. Export names/RVAs are discovery leads, **not verified function semantics or ready hook addresses**. No protection bypass was attempted. [dbce-wheel-mod-toolkit, 2026-09-07]

## Native controls and the owner's mixed rig

The [steering guide](https://steamcommunity.com/sharedfiles/filedetails/?id=1524655078), published **2018-10-02**, is credited to **KT Racing and Nacon** and describes development intentions starting with patch 1.02. Nacon is also explicitly marked **developer** on the pinned [supported-peripheral announcement](https://steamcommunity.com/app/658700/discussions/0/1735465524721552780/) of **2018-09-25**. Thus its authorship has stronger support than its title alone. The retrieved guide also contains Steam removed/incompatible warning banners; its current listing status should not be assumed. Its configuration paths and named features are corroborated by the installed files and official patch notes.

The guide documents independent steering, throttle, brake and clutch adjustment, including deadzone, saturation, sensitivity, inversion and combined-axis handling. The developer's launch list includes Logitech/Thrustmaster/Fanatec wheels, USB pedal products, H-pattern shifters, handbrakes and the AccuForce Pro V2, and describes the list as tested examples rather than an exhaustive restriction. Neither establishes MOZA/SIMAGIC support for this rig.

Local files provide more specific evidence:

- `Common\Settings\InputDeviceConfig.lua` maps product GUID numbers to DirectInput axes and filters. It contains separate Fanatec USB pedal and Heusinkveld pedal configurations, plus the AccuForce wheel and Thrustmaster TSS. **No MOZA or SIMAGIC name, or decimal product-GUID entry with MOZA VID 0x346E, was found in this file or `InputPresets.lua`.** Missing presets do not prove rejection of an unknown wheel.
- `InputBindings.lua` contains analog steering/clutch actions, brake/throttle/handbrake bindings, shift-up/down, direct reverse and gears **1–6**, rear/left/right views, wipers and headlights. Per-binding `ControllerGUID` fields in presets support an intended multi-device design. Actual simultaneous mixed-vendor binding remains untested.
- Export names include `InputDriverPC::EnumControllerCallback`, `EnumDirectInputSteeringWheel`, `FindInputDeviceConfigForGUIDProduct`, `CreateUserDefinedInputDeviceConfig`, and `GetFFBDevice`. Strings identify loading a saved `UserInputDeviceConfig.lua`. These are good diagnostic targets; successful user overrides and exact device-class filtering were not dynamically established.

| Rig component | Assessment and remaining check |
|---|---|
| **MOZA R12** | Native support is plausible without a mod. A [Steam player's firsthand R12 report](https://steamcommunity.com/profiles/76561199811965944/recommended/658700) says binding and driving worked; its build, firmware and full rig are unspecified, so this is community evidence only. Toolkit history records an 8-axis/128-button R12 and device-class/FFB-encoding quirks. Determine whether this game enumerates it, classifies it as a wheel, and selects it for FFB before applying a SIXDOF/DRIVING shim. |
| **Separate pedals** | The native USB-pedal configurations are strong evidence that pedals need not be connected through the wheel. The owner's exact pedal model and axis polarity were not supplied or measured. Verify each axis independently, simultaneous throttle/brake, full travel and clutch; avoid copying another brand's inversion/combined-axis assumptions. |
| **SIMAGIC DS-8X** | Toolkit history identifies a separate 11-button device. Native sequential and direct gear actions are promising. Test held H-pattern positions, reverse, neutral on release and clutch semantics. No direct 7th/8th gear action was found in the loose default bindings; do not promise all DS-8X positions. |
| **MOZA stalk** | Toolkit history identifies a separate 32-button device. Wiper/headlight and view actions exist, making useful native binding plausible. No evidence establishes turn-indicator actions, maintained-switch semantics, or enumeration of this exact button-only device. |

MOZA's [Xbox support table](https://support.mozaracing.com/en/support/solutions/articles/70000663997-xbox-games-support-list) includes V-Rally 4, but **Xbox compatibility is not evidence for this Windows R12/mixed-USB setup**. No device enumeration was performed here. The official [1.03 patch notes](https://steamcommunity.com/app/658700/allnews/), dated **2018-10-15**, also record controller and Fanatec-filter fixes; launch-era failures may not describe the installed build. [dbce-wheel-mod-toolkit, 2026-09-07]

## Loader and compatibility-hook feasibility

**Best initial loader candidate: an x64 DirectInput proxy.** The game imports the ordinary DLL name, and no root `dinput8.dll` was present in the inspected installation. The toolkit's `native/dinput-proxy/dinput_proxy.h` supplies reusable forwarding infrastructure, not a completed V-Rally mod. App-local loading, initialization timing and interaction with the protected executable still need a one-log-line runtime proof.

First instrumentation should observe the game's own enumeration callbacks, capability results, requested data formats, per-device axis values and selected FFB device. It can separate “missing from enumeration,” “seen but wrong family,” “wrong axis mapping,” “binding stored against the wrong device,” and “force commands rejected.” That prevents an indiscriminate VID/PID spoof or global device reorder from masking the actual issue.

If a device-type filter is demonstrated, adapt the known enumeration/capabilities/device-info strategy only for the selected physical device. If enumeration works, prefer a saved native configuration or a small binding fix. Reading DS-8X/stalk directly and writing native game actions is a fallback with additional calibration, identity and menu-state work; the shared input kit will not provide that automatically.

The toolkit/native device library must not blindly open a competing FFB session while the game drives the same base. A repair to the game's original effect calls may need no separate toolkit force device at all. A replacement output architecture must first establish one owner and a coherent DirectInput lifecycle, then gate output on driving state and zero it through pause, focus changes and orderly exit. Existing native C++ method names do not supply safe class layouts or calling contracts.

`PhysXCooking64_s.dll` is another demonstrated *import opportunity*, because the established WRC patch uses that filename; it is not proof its extraction logic recognizes this game. No official mod SDK or V-Rally-specific working loader was verified. Online compatibility/anti-cheat guarantees were not established; initial implementation should target local driving. [dbce-wheel-mod-toolkit, 2026-09-07]

## Telemetry: available integrations and useful signals

**No verified public UDP protocol, configurable UDP endpoint, or native shared-memory contract was found for this build.** The [SIM Dashboard author lists V-Rally 4 as lacking a telemetry interface](https://stryder-it.de/simdashboard/help/en/For_PC_Gamers/Game_Configuration/Unsupported_Games). The current [SimHub supported-games list](https://www.simhubdash.com/supported-games/) does not list it. No original V-Rally-specific SimTools plugin/download with a verified acquisition method was located. This is a scoped search result, not proof that no private or future integration exists.

**D-BOX is different:** official [patch 1.02 notes](https://steamcommunity.com/app/658700/allnews/) introduced simulator support on **2018-10-02**; [D-BOX's own catalogue](https://www.d-box.com/en/home) includes V-Rally 4. Locally, `dbxLive64.dll` exists, exports `GetEventHandler`, and the executable contains matching loader/event-handler strings. That is credible evidence of a game-to-motion path. Its arguments, timing, hardware activation conditions and whether it carries raw dynamics or processed motion commands are **unknown**. It must not be described as an open telemetry SDK.

The existing [motion4all WRC Telemetry Patch](https://www.overtake.gg/downloads/wrc-telemetry-patch.38991/) is a stronger research lead than generic WRC folklore. Its original author documents a PhysX DLL replacement and a **mod-created shared-memory** structure with velocity, acceleration, gear, RPM and suspension channels; its support list includes WRC 7/8/9/10/Generations and EA SPORTS WRC, **not V-Rally 4**. The patch's coordinate/wheel ordering and gear encoding also differ from a Forza packet. [SIM Dashboard's integration instructions](https://stryder-it.de/simdashboard/help/en/For_PC_Gamers/Game_Configuration/WRC) confirm that those WRC games require a mod. A shared engine or DLL basename is insufficient to transplant offsets.

Original [SpaceMonkey receiver source](https://github.com/PHARTGAMES/SpaceMonkey/blob/709c2711ac293cf0922a7c1fadf40348124ec115/GenericTelemetryProvider/WRCAPI.cs) and its [WRC payload readme](https://github.com/PHARTGAMES/SpaceMonkey/blob/709c2711ac293cf0922a7c1fadf40348124ec115/WRCInjection/Readme.txt) were inspected. They provide useful receiving/translation prior art, not a V-Rally sampler. The project's [WRC guidance](https://github.com/PHARTGAMES/SpaceMonkey/blob/709c2711ac293cf0922a7c1fadf40348124ec115/README.md) explicitly warns that its WRC motion subset is insufficient for AccuForce Foundation FFB. Do not promote motion acceleration into measured steering rack torque.

A conflicting search hit deserves explicit rejection as verified evidence: [OpenRacing's V-Rally adapter](https://github.com/EffortlessMetrics/OpenRacing/blob/51c8ab363cffea087d7559bc4ef81968fe6b86e4/crates/telemetry-adapters/src/v_rally_4.rs) asserts a Kylotonn UDP stream at port 64000. The inspected file implements a receiver/parser with constructed packet tests; it does not establish an emitter, setup procedure or capture from build 3570632. Its wider documentation also describes V-Rally using a different protocol label. **A parser accepting test bytes does not verify game telemetry.** Treat this claim as uncorroborated until an actual packet capture establishes the sender and fields.

| Signal | Best lead in this build | Quality and unknowns |
|---|---|---|
| Speed, engine RPM/load, shifts | Ordinary FMOD Studio import `EventInstance::setParameterValue`; `WIN32\Sounds\GUIDs.txt` identifies separate `Full` and `AI` car sound families, including engine and shift events | Strong hook opportunity; actual parameter names, scales, player identity and update rates unmeasured. A `Full` event is not by itself proof of local-player ownership. RPM/load used for sound may be normalized or smoothed. |
| Gear and actual driver commands | HUD/car state; original input actions; possibly native peripheral display path | Gear/input infrastructure exists. Object addresses and actual selected gear versus a shift-button event are unresolved. |
| Velocity, orientation, acceleration | PlayAll physics actor getters, car state, or D-BOX producer calls | Native physics data plausibly exists; exported generic actor getters do not identify the player's car. Acceleration derived from velocity needs timing/frame conversion and should be labelled derived. |
| Suspension, slip, contact/surface, collision | `VRPhysicWheel`/car strings, native FFB feature configuration and FMOD parameters | FFB configuration implies these situations are distinguished internally. Per-wheel units, sign, front/rear ordering, tyre forces and access paths remain unknown. |
| Race/paused/menu/replay state | Camera/race/HUD lifecycle | Essential to prevent stale dashboard/shaker output. No sampler should ship without distinguishing player driving, replays, AI and menus. |

The smallest useful route is an FMOD parameter census or a narrow car/HUD sampler that validates speed/RPM/gear against the display, then exports a native `TelemetryFrame` equivalent through `native/forza/forza_packet.h`. The existing 232/311/324-byte encoders can be reused; choose the receiver's required format explicitly. Target roughly 50–60 Hz only if the source updates that quickly, preserve units/unknown-field semantics, and park output between stages. Full tyre/motion telemetry is a separate milestone. An alternative receiver-facing option is SimHub's [external simulation integration](https://manual.simhubdash.com/external-sim-integration); it still requires a real extractor and is not implemented in this toolkit release. [dbce-wheel-mod-toolkit, 2026-09-07]

## FFB signal quality and whether a mod improves it

The installed `InputFFBConfig.cfg` defines a generic hardware block as well as manufacturer-specific blocks, constant/spring/damper scaling, SAT configuration, engine effects and surface/slip/suspension/collision settings. The [developer guide](https://steamcommunity.com/sharedfiles/filedetails/?id=1524655078) explains the distinction between steering force, damping and vibration, and documents a user Documents override. This is a substantial native implementation, not controller rumble alone.

**Real-physics versus synthesized:** native SAT is the most promising steering signal because the game already computes it. Its exact derivation from tyre forces, trail, slip or a simplified model was not verified. Configured surface/engine/collision vibration and centering are designed cues; they are not raw steering-rack torque. The existence of PhysX or a slider named SAT cannot settle physical fidelity.

If the R12 receives no force or the wrong sign, first log the original effect types, axis counts, magnitude/direction encoding, gains, start/update calls and results. Toolkit knowledge about MOZA sign handling is relevant, but **does not prove a V-Rally bug**. A surgical effect-encoding fix could preserve the game's tuning and information. If replacing output becomes necessary, explicitly suppress native delivery and route the validated native signal through the toolkit's shaping/diagnostics. Only fall back to the shared synthetic `ForceModel` after stating which measured channels are missing; lateral acceleration plus a virtual spring is a lower-confidence substitute, not recovered SAT. [dbce-wheel-mod-toolkit, 2026-09-07]

## Bonnet camera and rendering constraints

**Bonnet is present in the inspected PC assets.** The base archive contains `COMMON\TUNING\CARS\BASE\GAMEVIEWCAMERAS.CFG`, plus car-specific camera files. The base configuration and the SMG Dakar buggy override were decompressed in memory. The base includes named bonnet/cockpit configurations and follow views; the buggy override also contains a bonnet configuration. This corroborates the executable's `Bonnet`/`Cockpit` labels without inferring an engine or camera from screenshots.

The bonnet configuration is a fixed camera referenced to a car point named `CenterWiper`. It disables cockpit rendering but enables the front pass and car rendering in that pass. World near clipping is **1.0**, while the front pass has near/far distances of approximately **0.15/3.5** in the configuration's units. Consequently, moving only the world camera or changing only its near plane could clip the bonnet or misalign the separately drawn car. Preserve both passes, their FOV relationship and per-car offsets.

This is a native 3D scene/camera arrangement with model resources, rather than evidence of a sprite-car or projected pseudo-3D road. Nevertheless, road visibility, close terrain geometry, occlusion/LOD, streaming at crests and roadside views, shadows and car deformation have **not been visually verified**. The existing bonnet configuration lowers the risk but does not prove every car or stage renders correctly. Hidden cockpit parts, the bonnet's underside, wipers and damaged/detached bodywork need checking if the view is moved. Exposure is also view-specific: the binary contains separate bonnet/cockpit exposure settings. Photo mode/Ansel presence does not prove free driving-camera control.

For the requested missing-feature assessment, no camera implementation is justified. If the stock view is unsuitable, first establish whether a user/loose-file camera override is actually loaded; the documented FFB override is not evidence that arbitrary camera overrides work. A later adjustment should remain inside the native camera cycle and hand control back for replays, finish scenes, photo mode and menus. [dbce-wheel-mod-toolkit, 2026-09-07]

## Archive/mod techniques that actually apply

The original [zbirow V-Rally-4-Tools source and format notes](https://github.com/zbirow/V-Rally-4-Tools/tree/1d134ab469ee6ff1a19eefe761c60aab6c3939da) are specific to this game. Its PPKG index layout matched **all 33 local archives**: 30,995 entries parsed with bounded offsets; sampled first-three data markers in every archive were `PKGB`. This confirms index access, not successful asset editing or repacking.

There is a concrete caveat in the published extractor: for the tested base camera record, the index size **3970** is the compressed payload length, excluding the leading `PKGB` and trailing `PKGE` markers. The script instead reads size minus four after skipping `PKGB`, which truncates this LZ4 frame. Reading the full payload decoded **29,831 bytes**, SHA-256 `2f8f866f8a60bc8bb00cfa7e1ce884d185e6bc2c177fd703a25c615fec53bbd7`, with exactly 3970 input bytes consumed and the `PKGE` trailer immediately following. The SMG Dakar camera override also decoded. The framing corresponds to the [official LZ4 frame format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md).

Thus this source is useful prior art with a verified local correction, **not a validated drag-and-drop unpack/repack pipeline**. Compression, patch precedence, loader override rules and repacking still need implementation/verification. Do not assume WRC 7 QuickBMS scripts, WRC camera offsets, later Kylotonn tools or EA WRC Unreal techniques transfer unchanged. The toolkit should retain reusable knowledge; per-game hooks, configuration adapters and cameras belong in a future V-Rally repository. [dbce-wheel-mod-toolkit, 2026-09-07]

## Effort, blockers and smallest next experiments

Estimates are engineering time for one developer familiar with native Windows hooks and this toolkit, with access to later attended testing on this exact build. They exclude release/support expansion, multiple store versions and broad hardware testing. Alternative paths overlap; do not add every estimate together.

| Work | Rough effort | Smallest experiment and success criterion |
|---|---|---|
| Native input/FFB/camera check | **0.5–1 day**, much of it configuration and recording | In a separately authorized attended session, first verify no-force analog input for each device and native bonnet selection. If all bindings work, no input mod is needed. Physical FFB evaluation is a later explicit, low-force step. |
| Targeted wheel classification or saved-binding fix | **2–5 days** after reproduction; **1–3 weeks** if native action injection is required | A log-only x64 proxy observes actual enumeration and data formats. A confirmed single mismatch becomes one narrowly scoped fix; reconnect/restart must preserve all device roles. |
| Native FFB delivery repair | **3–7 days** once the fault is known; **2–4 weeks** for an output replacement | Record native effect parameters/results without adding another force owner. Prove encoding/lifecycle behaviour before judging feel. If native output works, tune it instead. |
| Basic telemetry | **3–7 days for discovery/PoC**, **1–3 weeks** for a stable basic bridge if useful channels appear | Hook the imported FMOD parameter setter and record names/ranges, event identity and cadence. Validate player speed/RPM/gear through start, driving, pause and exit. Emit a short captured trace through the existing Forza encoder and inspect the receiver. |
| Rich physics/tyre telemetry | **4–8+ weeks**, uncertain | Locate the local car/physics object and prove one signed velocity plus one per-wheel channel across resets. D-BOX call inspection is an alternative discovery probe, with no promise of tyre data. Stop expanding the packet until each field has validated semantics. |
| Optional bonnet adjustment | **1–3 days** if an override is loaded; **1–2 weeks** if hooks/render integration are needed | Change one view parameter reversibly in a future mod workspace, prove it affects the native bonnet, then inspect both car/world passes on a closed-body rally car and a buggy. Existing camera satisfies the base request already. |

Principal blockers are untested mixed-device selection, unknown native FFB behaviour on this R12, protected on-disk function bodies, unlocated player state, unverified camera override loading, and no confirmed V-Rally telemetry emitter. The exact pedal model is another input-specific unknown. No engine SDK or publicly verified build-matched telemetry offsets were found.

**First decision experiment:** an attended native setup check, followed only if needed by a zero-output diagnostic proxy. **First new-feature experiment:** an FMOD parameter census identifying the local player's engine/speed channels and menu transitions. This initial research is complete; neither experiment was performed under the present static-inspection scope. [dbce-wheel-mod-toolkit, 2026-09-07]
