# Rush Rally 3 and Rush Rally Origins — Windows mod feasibility

Assessment date: **2026-09-07**. Provenance for this assessment, its static observations, inferences and effort estimates: **[dbce-wheel-mod-toolkit, 2026-09-07]**. External sources have publication dates where available and were accessed on 2026-09-07. This is an initial feasibility assessment, not a tested compatibility report.

**Recommendation:** investigate a Rush Rally 3 modern-device compatibility patch first. It already has native wheel FFB, multiple-device binding and a dedicated bonnet camera. A telemetry exporter is the substantial missing capability identified by this research. Origins also received official wheel support; its bonnet-camera proposal is substantially riskier, and its current PC binary must be inspected before committing to an implementation.

## Scope and evidence standard

- **Official:** developer statements, Steam product descriptions and developer release notes. These establish intended/shipped features, not compatibility with this particular rig.
- **Static:** local file metadata, PE imports, C++ RTTI names, readable resource metadata and configuration names. Names are discovery leads, not proof that a particular function runs or that a value has known units.
- **Reported:** first-hand user or integration-author reports, with their platform/date limitations retained.
- **Inference / proposal:** engineering conclusions and future experiments. No game hook was executed.

Read first: [STATE](../../STATE.md), [consumer experience](../../CONSUMER-EXPERIENCE.md), and AGENTS.md. Relevant knowledge reviewed: [new-game checklist](../../../knowledge/NEW-GAME-CHECKLIST.md), [DirectInput proxy](../../../knowledge/DINPUT8-PROXY.md), [device quirks](../../../knowledge/DEVICE-QUIRKS.md), [FFB signal design](../../../knowledge/FFB-SIGNAL-DESIGN.md), and [Forza telemetry](../../../knowledge/TELEMETRY-FORZA.md). Current state takes precedence over older checklist references to unfinished templates.

Toolkit **source/package 0.12.0** and native device component **0.5.0, 38 exports**, are separate version identities. The x86/x64 `native/dinput-proxy/dinput_proxy.h` scaffold exists. Native read-device primitives and the FFB/telemetry plumbing exist; a shared input/calibration kit and SDL backend do **not**. Existing offline and zero-output hardware checks do not prove gameplay feel or adoption by these games.

No games were launched, devices enumerated/acquired, force applied, mods deployed, installed settings changed, or messages sent to anyone. Only the assigned report is added to tracked documentation. Scratch scripts, extracted metadata and public-page captures are under `%TEMP%/codex-rush-rally-research-20260907`; no proprietary code or assets are included in this report. No commit or push was performed.

## Four-feature decision table

“Confidence” refers to the assessment, not to tested hardware compatibility. Effort details and assumptions follow each game.

| Game / feature | Existing support | Feasibility and priority | Confidence |
|---|---|---|---|
| RR3 — wheel/input | Official DirectInput 8; separate peripherals added after launch; sequential mapping evident statically | **Good candidate for a focused compatibility repair.** Diagnose enumeration, bindings and steering transfer before replacing input | High on native support; medium on a repair for this rig |
| RR3 — FFB | Official native FFB; concrete DirectInput FFB class in installed binary | **Preserve the native signal first.** Trace delivery and repair device handling/encoding if required | High that FFB exists; medium on repair effort; unknown physical quality on R12 |
| RR3 — telemetry | No documented public export or supported packet format found | **Plausible native sampler; meaningful new work.** Vehicle metadata is promising, function/field access is unproved | Medium |
| RR3 — bonnet camera | Dedicated bonnet option, class, configuration and car attachment points | **Existing feature: validate/select it first.** Mod work only for a demonstrated adjustment need | High static confidence; no visual check on this build |
| Origins — wheel/input | Official wheel-support release in February 2023; peripheral gaps reported | **Promising follow-on**, conditional on current PC binary and device-path inspection | High on release history; medium-low on the exact implementation |
| Origins — FFB | Wheel support is official; Origins-specific FFB transport/effects not established here | **Investigate native FFB before adding it.** Shared RR3 implementation is plausible, not verified | Low-medium |
| Origins — telemetry | No documented public export or packet format found; no local binary | **Plausible shared sampler design**, but its hook and signals require discovery | Low-medium |
| Origins — bonnet camera | Developer describes intentional overhead views; no official bonnet mode established | **Experimental / highest risk.** Prove forward road rendering and visibility before promising a camera mod | Low-medium |

## Rush Rally 3

### Exact distribution and static evidence

Local **Steam Windows retail app 2020860**, at `D:/Program Files (x86)/Steam/steamapps/common/Rush Rally 3`. The installed `appmanifest_2020860.acf` reports **build 21554208**, depot **2020861**, content manifest **3414488631727382793**. This does not identify the free demo, and historical demo reports must not be treated as tests of this retail build.

| Inspection | Result and significance |
|---|---|
| Executable | `RushRally3.exe`, **12,480,584 bytes**, SHA-256 `e31e8fd627d1021ff5de15e8ffb31be8792c70d7516d02b52e9c419e50b4081a` |
| Architecture/runtime | PE machine **0x8664 / AMD64**, PE32+ **0x20B**, CLR directory zero; MSVC C++ runtime imports and MSVC RTTI. Native x64 C++, not a Unity Mono/IL2CPP finding |
| Engine | Numerous `RushEngine/…` labels and `RuCar…` / `RuCamera…` classes corroborate the developer's explicit **Rush Engine**, its own cross-platform engine, identification. [Developer portfolio, undated][portfolio] |
| Graphics/input | Imports `d3d11.dll!D3D11CreateDeviceAndSwapChain`, `DINPUT8.dll!DirectInput8Create`, and XInput 9.1.0 state/rumble APIs. No separate SDL DLL was found in the inspected install |
| Binary limitations | `.text` entropy **7.9999705 bits/byte**, high-entropy `.bind`, entry RVA **0xBD7310** inside `.bind`. Consistent with a packed/encrypted executable; the exact protection was not established. Ordinary static instruction tracing was not assumed possible, and nothing was unpacked or bypassed |
| Build identity caveat | COFF timestamp is **2026-01-19 12:23:46 UTC**; embedded PDB path ends in `Win.Final.x64/RushRally3.pdb`. No fixed file-version resource or game exports found. Timestamp/PDB label is not a marketed game version; the Steam build and hash are the reproducible identity |
| Resources/settings | Custom paired `.cpu` / `.gpu` resources, including `data/database/database.cpu`; installed `globaldata.dat` is a **600-byte binary file**, not a user-editable telemetry INI. Its first bytes and printable strings were inspected; saved-game contents were not decoded |

These facts establish a reasonable x64 DLL-proxy entry route, but neither an accepted loader nor a usable car-state hook has been demonstrated.

### Existing wheel support and modern-device gaps

The Steam store explicitly advertises wheel FFB. The developer describes DirectInput 8 in the pinned wheel thread, and the November 2022 launch post discusses testing with a Logitech G923 and driving compatible wheels' RPM lamps. Those lamps do **not** establish a public telemetry protocol. [RR3 store][rr3-store]; [developer wheel thread, 2022-11-24][rr3-wheels]; [major update, 2022-11-15][rr3-major]

Do not repeat the launch limitation as current design: the **2022-12-16 v1.130** notes explicitly add mixed wheels, pedals and handbrakes. The **2023-02-15 “More wheel updates”** release note also allows keyboard and other-device remapping in the wheel section. [v1.130 developer notes][rr3-130]; [RR3 official news][rr3-news]

Static configuration names independently support per-action device selection: steering, acceleration, brake, handbrake, gear up/down and camera each have wheel-axis settings; corresponding `e_U32_WHEEL_GUID_…` names occur. Wheel deadzone/calibration and force-strength settings also exist. This is stronger evidence than assuming a single gamepad device, but does not prove stable identities, good calibration, all USB device types or 128-button handling.

First-hand reports remain mixed: an R21 user reported missing separate peripherals in April 2024; R9/R5 users reported force locking in December 2024/July 2025; March–May 2026 posts report standalone pedals/handbrakes not detected, G923 force retained after a race, and an R5 not detected. No commenter identifies build 21554208, so these are diagnostic leads, not reproduced defects. [Wheel thread pp. 5–7][rr3-wheels5] [pp. 6][rr3-wheels6] [pp. 7][rr3-wheels7]

**Input feasibility (inference):** analog steering and independent accelerator/brake channels already have a native path. First trace device class/capability filters, per-action GUID association, state format, ranges and inversion. The toolkit's known SIXDOF-versus-DRIVING issue is a candidate explanation for some modern bases, **not a diagnosed RR3 bug**. Repair it only if the trace actually shows exclusion. Then measure steering input against in-game road-wheel response at different speeds; static deadzone names do not rule out gamepad-style smoothing or speed assistance.

For the DS-8X, sequential gear-up/down bindings are directly supported by the discovered action vocabulary. No direct per-gear wheel-binding or wheel-clutch action was identified in that vocabulary; a keyboard-clutch setting and real clutch/gearbox classes exist. **H-pattern gear selection and analog clutch remain unverified**, and implementing them may require a gearbox/input-layer hook rather than button remapping. Gear count, reverse/neutral rules and clutch behavior need explicit validation. A button mapper can offer sequential/menu actions; it cannot make digital keystrokes into independent analog pedals or prove H-pattern behavior.

### Loader and hook approach

**Preferred entry:** an x64 `dinput8.dll` proxy, using the existing toolkit scaffold and forwarding the imported API to the genuine System32 DLL. Native game-specific hooks remain in a future per-game repository. No Unity loader, Harmony patch or BepInEx interop should be budgeted for this binary.

Start at documented DirectInput interfaces: enumeration, device creation/capabilities, data format, range properties, state reads and effect creation/update/lifecycle. This can settle many wheel/FFB problems without discovering game function addresses. Log both the game's chosen input device and its FFB target. A proxy import is strong feasibility evidence, not proof that Steam startup or all device paths will accept the proxy unchanged.

A telemetry sampler or direct H-pattern input needs a second seam. RTTI includes `RuCar`, `RuCarInputLayer`, `RuCarEngine`, `RuCarGearbox`, `RuCarWheel`, `RuCarSuspension` and driveline components. Use these as identification anchors in a future normally running, authorized offline session; identify the local player and lifecycle before sampling or writing. The packed/encrypted code, absent PDB and lack of an official mod API make version-specific discovery and maintenance the main cost. No offsets, calling conventions or update functions are verified here.

The install was not shown to have an anti-cheat component, but the absence of a named file is not a compatibility guarantee. Both games have multiplayer/leaderboards. Keep initial instrumentation in offline play; input changes must preserve the game's physics and assist rules. Online policy/gating is a per-game implementation decision, not authorization inferred from this report.

### Telemetry and FFB signal quality

No official UDP switch, public shared-memory interface, SDK header, documented SimHub/SimTools integration or packet definition was located. Neither game appears on the inspected **SimHub supported-games page**. Searches of developer sources, original integration repositories and SimTools material did not produce a Rush Rally protocol. SimTools' site could not be fully retrieved (HTTP 406; the reader fallback required authentication), so its complete plugin catalogue was **not** audited. The correct finding is **no verified integration found**, not proof that no private or unindexed plugin exists. [SimHub supported games, current inspection][simhub]

**Actual native format:** unestablished. **Port, packet size, field offsets, units, update frequency and enablement procedure: all unknown.** No packet was captured. Winsock `sendto`/`recvfrom` imports also serve networking; they do not prove telemetry export. The developer's “real telemetry data” wording describes physics development, not a Data Out feature. [Developer portfolio][portfolio]

| Candidate car-state data | Evidence / expected route | Quality limit |
|---|---|---|
| Speed, engine RPM, gear, player inputs | Engine/gearbox/input RTTI and HUD RPM/speed identifiers; sample the player's simulation/HUD source | Addresses, units and update order unverified; HUD text can be rounded |
| Position, orientation, velocity and acceleration | Vehicle/camera physics relationship makes a pose/velocity seam plausible | Derivatives would be calculated signals, sensitive to timestep, resets and filtering |
| Wheel loads, slip, suspension, tyre forces | Wheel/suspension classes; lateral-force, load/force and aligning-moment labels | Labels are not accessible live fields; tyre order, sign and units need measurement |
| Surface/impact/state | Surface-dependent game design, vibration effects and vehicle/game-state metadata | Magnitudes and active-driving/menu semantics require validation |

**Proposed export:** snapshot one coherent player frame after simulation update, then use `native/forza/forza_packet.h` to send a **new, mod-owned** Forza-compatible stream (311 or 324 bytes selected explicitly for the receiver). A configurable local destination such as `127.0.0.1:8000` is a proposed bridge setting, **not an RR3 port**. Verify packet lengths/units, pause/restart/replay behavior and countdown/menu flags; coordinate SimHub/Pit House port ownership. Unknown signals must remain explicitly unsupported rather than invented measurements. A dashboard-only first version needs much less data than credible suspension shakers or motion.

Native FFB should be investigated before constructing a replacement force model. `RuForceFeedbackDirectInput_Platform` and `RuForceFeedback` are present, alongside vibration-effect metadata. The original **MMVJ** project's author reports RR3 using constant-force effects under its Linux mapper. That is useful first-hand integration evidence; it is not a Windows/R12 trace or proof of tyre-derived steering torque. Its published license says **all rights reserved**, so it is a reference rather than code to vendor. [MMVJ README/license, current inspection][mmvj]

RR3 also contains a real car/tyre model **and** assisted/fake side-force configuration labels. It is therefore unjustified to describe the outgoing FFB as pure steering-rack physics based on the class names alone. Trace the native requested force and, if possible, correlate it with front lateral force/load/slip. If only output encoding, axis count or lifecycle is defective, preserve the game's force calculation and repair delivery. Only if native cues are inadequate should a per-game sampler feed the toolkit force model: measured front-tyre forces are preferable to a spring plus lateral-acceleration approximation. Label synthesized centering, road buzz and gear pulses as synthesized. Captured FFB alone cannot reconstruct independent RPM, gear or six-axis motion telemetry.

If toolkit output takes ownership, suppress/relinquish native output first; two competing force owners are not a viable default. Follow the one-DirectInput-instance, signed-magnitude/axis-count, effect recovery, stale-frame zero, pause/focus and pre-window-close shutdown rules. None of that establishes physical feel without later attended testing.

### Bonnet camera: already a distinct native path

Static evidence is unusually strong:

- The executable has **separate** bonnet, bumper and cockpit option names, a `RuCameraBonnet` class, and the `ru_transform_bonnetcamera` attachment identifier.
- `database.cpu` contains separate **Bumper / Bonnet / Cockpit** camera configurations. The bonnet block has FOV **68**, near **0.5** and far **3000**; bumper and cockpit blocks differ. These are stored metadata values, not measured runtime projection/units or a guarantee of actual draw distance.
- Two sampled car resources, `fanducompanionii.cpu` and `autobrazorgl.cpu`, contain bonnet-camera attachment metadata and separate cockpit/noncockpit model groups. The whole car roster was not inspected.

This establishes far more than a generic cockpit/bumper label. The first experiment should be to select the **native bonnet option**, confirm the bonnet is actually visible on several cars, and inspect its FOV and road visibility. The game already has forward-facing scene rendering, cockpit content, mirrors and extended-distance work in its PC release history. [Developer PC graphics update, 2022-10-14][rr3-4k]

Any optional camera adjustment must still use the engine camera before visibility determination, preserve bonnet/body visibility selection, and verify near clipping, headlight/wiper effects, damaged panels, road crests, horizon/fog, trees/billboards, shadows and wet surfaces. Altering only a final D3D view matrix can leave culling and LOD tied to the original camera. No rendered scene was inspected here; native bonnet existence is not a claim that every car's hood geometry is flawless.

### RR3 effort and smallest proof of concept

Estimates are **engineer working days**, assuming one experienced Windows C++ modder, an authorized offline runtime phase, the existing toolkit build environment, one pinned Steam build and no protection circumvention. Validation/hardening includes pause, restart, focus, shutdown and device reconnection; broad public hardware coverage adds time. Features share discovery work, so these are not additive quotes.

| Feature | Smallest useful experiment | Provisional implementation effort |
|---|---|---|
| Wheel/input | 1–3 days: logging proxy with physical FFB suppressed; record enumeration/state formats, simultaneous steering/brake/throttle and sequential buttons | 3–8 days for a localized filter/binding fix; 2–4 weeks for a full independent input/calibration UI or H-pattern/analog-clutch hooks |
| FFB | 1–3 days: trace native effect type, axis count, magnitude/direction, gain, starts/stops and HRESULTs with torque suppressed | 2–7 days for an isolated transport/lifecycle correction; 2–4 weeks if new physics sampling/tuning is required |
| Telemetry | 3–10 days: identify the player and log speed/RPM/gear plus pose across restart; prove units and liveness | 1–3 weeks for a useful basic exporter; 3–6 weeks for well-validated tyre/suspension/motion signals |
| Bonnet | About 0.5 day: select native view and inspect several cars/stages | No feature-addition work if native behavior satisfies the request; 2–5 days for a localized adjustment after a usable camera hook is found |

**Decisive first POC:** a forwarding/logging x64 proxy that prevents all force-producing effect creation/start/actuator paths from reaching the hardware, while reporting what the game requested. Compare a known working wheel path with the R12 and each separate input role. This settles loader use, device rejection versus binding rejection, and native force encoding without writing a replacement physics model. In a later runtime phase, the game itself would still access input devices; none of this was run in this research task.

## Rush Rally Origins

### Distribution and engine evidence

Target is **Steam Windows app 1728660**, released **2022-04-13**. **No Origins executable, installed manifest, build ID, hash or local configuration was inspected.** The supplied installation survey found no Origins Steam manifest. No demo or depot was downloaded for this task. Do not substitute RR3 build 21554208 or the Android version for the missing Origins build. [Origins Steam page][origins-store]

The store says Origins uses RR3 graphics/physics; Brownmonster describes transferring engine improvements from Origins back into RR3. This is credible primary evidence of a **shared Rush Engine family**. Native C++/x64/D3D11 similar to RR3 is a strong working hypothesis, supported by the advertised 64-bit OS/CPU and DirectX 11 requirements, but **Origins PE architecture, compiler, actual renderer imports, packing and input implementation remain unverified**. Minimum OS requirements alone are not a PE inspection. [Origins store][origins-store]; [engine integration post, 2021-09-10][shared-engine]

### Native inputs, FFB and the owner's devices

Official Steam notes dated **2023-02-09** explicitly add wheel support and caution that not every wheel works. A developer confirmed on **2023-03-31** that the demo and retail game had the same wheel/peripheral support at that time. That makes a current demo a useful future low-cost probe, after recording its own build; it does not prove perpetual demo/retail parity. A G923 success and separate-USB-pedal failures were reported in the same original support thread. [Origins official news][origins-news]; [developer/user thread][origins-wheels]

**Source trap:** Origins' news feed includes a November 2022 cross-promotion titled **“Rush Rally 3 Available NOW!”** containing “full force feedback wheel support.” That passage describes **RR3**. It must not be used as an Origins FFB specification. The actual Origins wheel announcement does not name effect types, a DirectInput version, FFB signal source or device list. Treat native Origins FFB as unresolved until its binary or a direct effect trace establishes it. [Origins news][origins-news]

Origins' earliest no-handbrake/no-manual-gears descriptions are also stale: the developer restored both in August 2021, and the Windows May 2022 notes mention improved manual shifting. This supports manual/sequential gameplay, not independent H-pattern gates or analog clutch simulation. [Developer update, 2021-08-24][origins-manual]; [Windows notes][origins-news]

Analog steering is a reasonable expectation of the wheel path; actual transfer curve, pedal simultaneity, independent USB bindings, clutch exposure, direct gear selection and menu handling remain unknown. Do not transfer RR3's per-action GUID names to Origins as an observed fact. Its drift/automatic-handbrake assistance is intentionally different, so an input mod should feed the intended analog control layer without silently importing RR3 handling rules. [Origins developer FAQ, historical design description][origins-faq]

| Owner hardware / role | RR3 assessment | Origins assessment |
|---|---|---|
| MOZA R12 | Correct general API family; no test of this rig/build. MOZA-family detection/force reports justify tracing device filtering and effect encoding | No R12-specific verified result found. Shared-family compatibility problems are a hypothesis |
| Separate pedals | Native mixed-device support exists, but the exact pedal model/USB arrangement is unspecified and reports show exceptions. Check both axes simultaneously, rest/travel and polarity | Official wheel support does not settle separate-pedal support; original reports identify this gap |
| SIMAGIC DS-8X | Known toolkit-side 11-button, no-axis device; sequential action vocabulary exists. H-pattern, neutral/reverse and clutch require more work/evidence | Button capture and sequential mapping must be checked; H-pattern/analog clutch unestablished |
| MOZA stalk | Known toolkit-side separate 32-button device; map only actual game actions such as camera/menu/look where accepted. Indicator/wiper gameplay functions not established | Device capture/action availability unknown; do not promise functions absent from the game |

The device shapes above come from this repository's [device knowledge](../../../knowledge/DEVICE-QUIRKS.md), **not tests in either Rush Rally title**. Separate input roles should persist by identity and retain disconnected bindings. The future input kit cannot be claimed as an already implemented solution.

### Loader, signals and telemetry feasibility

**Conditional loader plan:** inspect the current Windows executable/imports first. If it imports DirectInput 8 as RR3 does, reuse the x64 proxy approach after confirming its architecture. If not, discover the actual input/graphics loading route; neither a managed loader nor a dinput proxy is automatically valid. Any `RuCar…` RTTI or shared component names must be located again, not presumed identical at RR3 offsets.

Expected useful signals are player speed, RPM/gear, throttle/brake/steer, position/velocity, yaw, per-wheel suspension/slip/load, surface and collision events. These are **inferred from the documented shared vehicle dynamics**, not found Origins fields. Start with its displayed speed/gear and the player car object, then determine whether full physics state is available. A stable basic sampler could feed the same mod-owned Forza encoder as RR3. Native UDP protocol/port/format remains unknown; the SimHub and SimTools research limits stated above apply equally here.

If Origins already generates meaningful force, port only the required compatibility repair. Otherwise the best potential FFB source is its own front-tyre load/lateral force, if found. A velocity/steering/acceleration model with slip-lightening and surface events is a fallback with **synthesized cues**, not proof of real aligning torque. Origins' altered drift model makes copying RR3 force gains or raw aligning-moment assumptions especially weak. No statement about actual R12 feel is supportable yet.

### Bonnet-camera rendering constraints

The developer explicitly describes Origins as an intentional overhead racer. No official bonnet/cockpit/bumper option was established; an April 2022 developer reply likewise explains the deliberate camera choice. [Developer FAQ][origins-faq]; [developer reply on its app listing, 2022-04-18][origins-camera-reply]

There is genuine 3D terrain evidence, independent of screenshots: Brownmonster explains that the series generates tracks **at load time**, uses terrain triangles, and generates water meshes from them. Therefore “top-down” does not imply a flat sprite road. It also does not prove the forward road/horizon needed for a bonnet view is complete. [Procedural terrain/water explanation, 2021-08-12][terrain]

Before committing to a bonnet feature, a camera POC must establish:

1. **Road projection and geometry:** select perspective projection and a car-relative forward camera before visibility work. Inspect crests, dips, hairpins, bridges if present and track edges. Determine whether terrain ahead is generated and loaded far enough for a low camera; stored track files cannot simply be assumed available.
2. **Culling and LOD:** validate frusta, fog, far distance, shadows, occlusion and any generation/streaming bounds. A final-view-matrix override cannot bring back terrain/objects already culled or not generated.
3. **Sprites/billboards:** inspect trees, crowds, particles, fences and distant scenery for single-sided or overhead-specific representations. Terrain meshes do not establish suitable low-angle vegetation or horizons.
4. **Car content:** check whether a bonnet attachment exists, the body/hood survives the selected visibility mode, and the low view exposes missing inner/back faces. RR3's cockpit/bonnet parts are not permission or evidence that Origins ships equivalent content. Do not copy RR3 assets into a mod.
5. **Driving visibility:** test multiple countries, weather/daylight, narrow roads, dust/water and other cars. Bonnet visibility, usable road sightline and stable near clipping are separate acceptance checks. A bumper proof can test road rendering first, but does not satisfy a request for a visible bonnet.

No local Origins geometry, projection or culling was inspected. Shared renderer ancestry makes the experiment worthwhile; camera-only success is not yet established. Missing terrain/hood content could expand the work beyond a sensible mod.

### Origins effort and smallest proof of concept

Use the same engineer/day assumptions as RR3, **plus access to a current Windows binary**. The lower estimates assume sufficiently similar native components and that no new assets are needed.

| Feature | Smallest useful experiment | Provisional implementation effort |
|---|---|---|
| Wheel/input | 0.5–1 day binary/import audit, then 1–3 days of native input tracing | 3–8 days for a transferable device fix; 2–4 weeks for new input/calibration/shifter integration |
| FFB | 1–3 days to determine whether effects exist and what native force is requested, with physical force suppressed | 2–7 days for a shared adapter fix; 2–4 weeks for sampled/synthesized FFB, plus attended tuning |
| Telemetry | 3–10 days to locate/validate basic state after identifying the runtime | 1–3 weeks for basic export; 3–6 weeks for robust richer signals; shared code may reduce the upper range |
| Bonnet | 2–5 days after a viable camera hook, testing perspective road visibility before cosmetic polish | 2–6 weeks if geometry/culling permits; potentially unsuitable if substantial missing content must be created |

**Decisive first POC:** record the executable hash/imports and compare RTTI/resource vocabulary to RR3, then trace its own wheel/effect path. For camera feasibility, conduct a separate gamepad-driven, no-force forward-camera experiment over representative terrain. Passing the loader or steering test does not settle the camera question.

## Credible shared-engine opportunities and boundaries

The developer's explicit cross-porting of engine work is stronger evidence than similar appearance. Share **discovery techniques and adapter interfaces first**: PE/build identification, DirectInput traces, device-role diagnostics, car-frame semantics, telemetry encoding, lifecycle handling and a camera QA procedure. If matching RTTI/component layouts are confirmed later, a common Rush Engine adapter module may be justified. Maintain separate build signatures, player-selection logic, force tuning and camera content rules. [Shared engine development][shared-engine]

RR3 is the better seed because it is installed, has readable native metadata, and supplies native wheel/FFB/bonnet reference behavior. Its telemetry sampler could establish units and component conventions useful for Origins, but addresses/layouts and assist differences cannot be copied blindly.

Existing integrations worth retaining are the native wheel paths and RR3's native camera system. RR3's official **Steam Workshop is for liveries**; it does not supply a code-plugin ABI or vehicle telemetry SDK. No maintained open-source Rush-specific Windows wheel/telemetry/camera mod was identified in the searches performed. MMVJ supplies a limited effect-type observation, not reusable Windows game hooks. [Workshop developer post, 2023-01-09][workshop]; [MMVJ][mmvj]

Keep game hooks, samplers, input writes, camera controls and online gates in future per-game repositories. This toolkit should receive only justified reusable device/FFB/telemetry plumbing, diagnostics and dated knowledge. The immediate deliverable is this assessment; all runtime POCs above remain future work.

## Dated source register

All sources below were accessed **2026-09-07**. Unspecified publication dates are recorded as undated, not assumed current releases. Steam news was also read through Valve's public `ISteamNews/GetNewsForApp/v2` API to distinguish individual announcements from cross-promotions.

| Source | Published / evidence used |
|---|---|
| [RR3 Steam store][rr3-store] | Current product listing; Windows release 2022-11-24; official FFB claim |
| [Brownmonster portfolio][portfolio] | Undated; explicitly identifies own Rush Engine; physics-development description |
| [RR3 major update][rr3-major] | 2022-11-15; G923 wheel support and compatible RPM lamps |
| [RR3 wheel information][rr3-wheels] | Developer post 2022-11-24; DirectInput 8; historical launch limitations |
| [RR3 v1.130 changes][rr3-130] | 2022-12-16; mixed pedals/handbrakes/wheels |
| [RR3 official news][rr3-news] | “More wheel updates” 2023-02-15; “Minor Update” 2023-02-14; also Workshop/release history |
| [RR3 wheel thread page 5][rr3-wheels5], [page 6][rr3-wheels6], [page 7][rr3-wheels7] | First-hand reports spanning 2023–2026; not build-pinned. Page 7 public HTML recovered by curl; timestamps confirm 2026 dates |
| [RR3 PC graphics update][rr3-4k] | 2022-10-14; cockpit, mirrors, particles and draw-distance work |
| [Origins Steam store][origins-store] | Current product listing; Windows release 2022-04-13; shared graphics/physics and 64-bit/DX11 requirements |
| [Origins official news][origins-news] | Wheel support 2023-02-09; manual shifting improvements 2022-05-26; RR3 cross-promotion 2022-11-24 |
| [Origins wheel thread][origins-wheels] | Developer 2023-01-05/2023-03-31; G923/separate-pedal first-hand reports 2023–2024 |
| [Origins manual-controls update][origins-manual] | 2021-08-24; restored handbrake and manual gears |
| [Origins developer FAQ][origins-faq] | Undated/historical; intentional overhead design and altered physics; future-v1.8 wording superseded by release evidence |
| [Origins developer camera reply][origins-camera-reply] | Reply 2022-04-18 on Android listing; design intent only, not current Windows build evidence |
| [Procedural terrain/water post][terrain] | 2021-08-12; explicit geometry-generation explanation and cross-product reuse |
| [Back to RR3][shared-engine] | 2021-09-10; engine improvements transferred from Origins |
| [SimHub supported games][simhub] | Current list inspected; no Rush Rally entry found; does not rule out external/private plugins |
| [MMVJ README and license][mmvj] | Undated/current inspection; author's Linux constant-force observation; all-rights-reserved license statement |
| [RR3 Steam Workshop][workshop] | 2023-01-09; livery sharing scope |

[rr3-store]: https://store.steampowered.com/app/2020860/Rush_Rally_3/
[portfolio]: https://www.brownmonster.co.uk/portfolio/index.html
[rr3-major]: https://www.rushrally.co.uk/blog/2022/014_rr3_major/index.html
[rr3-wheels]: https://steamcommunity.com/app/2020860/discussions/0/3594464430433399956/
[rr3-130]: https://www.rushrally.co.uk/blog/2022/017_rr3_update_changes/index.html
[rr3-news]: https://steamcommunity.com/app/2020860/allnews/
[rr3-wheels5]: https://steamcommunity.com/app/2020860/discussions/0/3594464430433399956/?ctp=5&l=english
[rr3-wheels6]: https://steamcommunity.com/app/2020860/discussions/0/3594464430433399956/?ctp=6&l=english
[rr3-wheels7]: https://steamcommunity.com/app/2020860/discussions/0/3594464430433399956/?ctp=7&l=english
[rr3-4k]: https://rushrally.co.uk/blog/2022/013_rr3_4k/index.html
[origins-store]: https://store.steampowered.com/app/1728660/Rush_Rally_Origins/
[origins-news]: https://steamcommunity.com/app/1728660/allnews/
[origins-wheels]: https://steamcommunity.com/app/1728660/discussions/0/3722820884758473159/
[origins-manual]: https://www.rushrally.co.uk/blog/2021/007_week/index.html
[origins-faq]: https://brownmonster.wixsite.com/home/faq-1
[origins-camera-reply]: https://play.google.com/store/apps/details?id=brownmonster.app.game.rushrallyremastered&hl=bg
[terrain]: https://www.rushrally.co.uk/blog/2021/006_safari/index.html
[shared-engine]: https://www.rushrally.co.uk/blog/2021/009_back_to_3/index.html
[simhub]: https://www.simhubdash.com/supported-games/
[mmvj]: https://github.com/leosat/MMVJ
[workshop]: https://www.rushrally.co.uk/blog/2023/001_steam_workshop/index.html
