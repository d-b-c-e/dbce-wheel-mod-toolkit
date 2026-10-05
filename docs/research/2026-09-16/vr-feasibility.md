# Separate VR mods: four current Unity racing projects

Assessment: **2026-09-16**. Primary interests: **art of rally, Super Woden:
Rally Edge, #DRIVE Rally and iRacing Arcade**. Target use is Windows PC VR
streamed to **Quest 3 through Virtual Desktop**, with **Steam Frame** as a
subsequent headset target. Seated wheel use is the proposed design.

This is source/document research, not a tested VR installation. No game was
launched, headset tested, loader installed, game file changed or wheel acquired.
No car mesh was visually inspected. Local facts and engineering judgments carry
**[dbce-wheel-mod-toolkit, 2026-09-16]** provenance; sibling sources are identified
below. Public sources were accessed on this date. Existing working-tree changes
in the projects mean the listed commits are context, not exact snapshots of
every inspected file.

## Recommendation

**All four have credible routes worth a bounded experiment.** #DRIVE Rally has
the quickest off-the-shelf VR lead; art of rally is the best first candidate for
a separate VR companion to our established wheel mod. Woden and iRacing Arcade
are reasonable subsequent Unity 6 IL2CPP experiments, with less direct VR
evidence. Engine similarity alone does not establish compatibility.

A cockpit is a separate content feature. A bonnet/bumper camera can provide
stereoscopic depth, natural head rotation and positional tracking without an
interior. Third-person VR is also possible. Merely displaying a flat game on a
large virtual screen does not establish any of those capabilities.

| Game | Local technical evidence | Proposed VR route | Feasibility judgment | Interior evidence |
|---|---|---|---|---|
| art of rally | Unity 2019.4.38f1 Mono; UMM wheel mod; `CarCameras` and bonnet/bumper hooks | Evaluate SuiMachine's existing game-specific VR work and legacy UUVR; adapt camera/UI integration | **Strong precedent**, but current build + wheel mod combination untested | Local camera documentation says no modeled cockpit; an interior would be new content |
| #DRIVE Rally | Unity 6000.0.73f1 Mono; installed build 24837351; readable camera code with Inside, Steering and Hood modes | Modern Mono UUVR, then game-specific menu/camera fixes | **Strong historical UUVR evidence**; exact installed build still requires validation | Actual Inside/Steering camera paths and attachment transforms exist; quality/completeness of the visible interior is unverified |
| Super Woden: Rally Edge | Unity 6000.3.6f1 IL2CPP, metadata 39; existing BepInEx wheel plugin and `Car_Cam` mounted-view hook | Modern IL2CPP UUVR plus a Woden camera adapter | **Plausible, moderate confidence**; no exact-build working VR evidence established | No usable cockpit established by this study; inspect meshes before specifying the required artwork |
| iRacing Arcade | Unity 6000.3.23f1 IL2CPP, metadata 39; BepInEx plugin; `EdyCamera`/Cinemachine and mounted cameras | Modern IL2CPP UUVR plus an Arcade camera adapter | **Plausible, moderate confidence**; no exact-build working VR evidence established | Local camera documentation says no modeled interiors; a cockpit would be new content |

These are research rankings, not support declarations. iRacing Arcade is a
different game from the main iRacing simulator; the latter's VR support is not
evidence for Arcade. [dbce-wheel-mod-toolkit, 2026-09-16]

## What the external VR work establishes

### art of rally

[SuiMachine's VR Hack](https://github.com/SuiMachine/Art-Of-Rally-VR-Hack)
provides a concrete, inspectable precedent. Its instructions use a Unity patcher,
BepInEx and OpenVR. It documents multipass rendering, disabled depth of field
and volumetric lighting, and remaining intro/results defects. This is useful
starting code, not evidence of a polished current release.

The source's [VR rig](https://github.com/SuiMachine/Art-Of-Rally-VR-Hack/blob/main/Source/Hacks/VR_Recenter.cs)
adds a rotation-and-position pose driver updated before rendering. Its plugin
also disables Unity's automatic positional tracking, so an isolated reading of
that flag would be misleading: it has its own tracking setup. Verify actual
head translation and scale in the headset.

Our UMM camera patch and this BepInEx mod both affect camera behavior. Loader
coexistence and pose ownership need explicit tests; do not install two generic
loader bundles over one another and assume compatibility. An adapted companion
may be cleaner than running two independent camera implementations.
[art-of-sim-rally + upstream VR source, 2026-09-16]

### #DRIVE Rally

In the [game's VR discussion](https://steamcommunity.com/app/2494780/discussions/0/4844274293904065844/),
players describe UUVR working in 2024 and 2025, with some menus remaining on the
desktop and a performance cost. The developer also directs readers to a UUVR
preview. These are firsthand historical reports and developer acknowledgment,
not official VR support or a test of installed build 24837351.

Local inspection strengthens the camera route: `CameraCarFollower` actually
implements `UpdateInsideCamera` and `UpdateSteeringCamera`, using interior/head
attachment transforms. This is more than an unused enum name. It still does
not prove that every car has a complete interior when the player leans or looks
backwards. The existing wheel project remains at an offline-built research
stage; its loader and wheel behavior must not be called gameplay-validated.
[drive-rally-wheel, 2026-09-16]

### Shared Unity route

[UUVR](https://github.com/Raicuparta/uuvr), installed through
[Rai Pal](https://raicuparta.com/rai-pal/), has separate legacy/modern and
Mono/IL2CPP configurations. Its [project file](https://github.com/Raicuparta/uuvr/blob/main/Uuvr/Uuvr.csproj)
includes OpenVR and OpenXR integration for modern builds. This makes it a
credible foundation across the four games; it is not a compatibility matrix.
The inspected IL2CPP project references an older BepInEx build than the #788
baseline used by our Unity 6 mods. Verify loader/interop compatibility instead
of replacing working consumer dependencies with a generic package.

## The important work is camera and rendering integration

The proposed relationship is:

`car pose -> chosen seat/bonnet mount -> recentered headset pose -> left/right eyes`

The car mount and headset must compose their transforms once. The headset
runtime supplies the eye projections. Existing flat-screen FOV, speed zoom,
shake, look-behind flips and camera smoothing need VR-specific treatment.
Smooth the vehicle reference if needed, not the user's head tracking. Provide
recenter and optional horizon stabilization, with deliberate transitions at
respawn, replay and results. [dbce-wheel-mod-toolkit, 2026-09-16]

Specific local hazards:

- **Art:** `BonnetCamera` takes over in `LateUpdate` and writes the lens FOV.
  The VR rig must retain control of tracked pose and eye projection while
  receiving the selected vehicle mount.
- **Woden:** `MountedCamera.Prefix` detects foreign position/rotation or parent
  changes. The handoff can make the player camera unavailable and call
  `Runtime.Force.Suspend`. Headset motion could therefore cause repeated
  handoffs and lost FFB. Introduce explicit VR ownership, retaining existing
  game-state/force shutdown protections.
- **Arcade:** the existing mounted view is a Cinemachine virtual camera with
  its own lens and handoff rules. Apply tracking to the correct final output
  camera, preserve player selection, and review velocity-follow/look-behind
  behavior for VR.
- **DRIVE:** existing interior cameras lerp position and rotation toward car
  attachments. Keep the seat attachment separate from tracked head motion;
  UI projection is already a reported problem.

[art-of-sim-rally, woden-rally-edge-wheel, iracing-arcade-wheel,
drive-rally-wheel, 2026-09-16]

For each game, test both eyes for roads, foliage, shadows, transparent windows,
particles and post-processing. Check culling when looking sideways and leaning;
content authored for a distant chase camera may expose holes or low detail.
Menus and our F6/UMM settings must remain usable from VR. Measure game rendering
and streaming frame times separately at a chosen refresh rate; low-poly art is
not a performance guarantee. No hardware-specific performance estimate was
established here. [dbce-wheel-mod-toolkit, 2026-09-16]

## Cockpits and what AI can contribute

**A believable stylized cockpit is realistic without requiring the owner to
become an expert vehicle modeler.** Start with one reusable rally interior:
dashboard, instrument panel, steering column/wheel, simple door surfaces,
floor/seat area, and optional roll cage. Build enough of the cabin to survive
normal seated head movement; a dashboard picture or front-facing shell will
break when viewed from the side. [dbce-wheel-mod-toolkit, 2026-09-16]

Three useful fidelity targets:

| Target | Practical approach | Main work remaining |
|---|---|---|
| Generic stylized rally cabin | Procedural meshes/Blender scripts with adjustable width, dash depth, seat height and wheel position | Fit to one body, clear windows, test clipping and stereo |
| A few cabin families | Reuse parts for hatchback, coupe, sedan or open-wheel layouts; tune per-car offsets | Additional fit profiles, materials and visibility checks |
| Faithful interior for every vehicle | Model-specific photographs/dimensions and bespoke modeling | Substantial reference gathering, manual review and per-car production |

I can write scripts to construct and revise the simple meshes, create instrument
graphics, automate export/import, attach parts to the car and drive gauges from
the game's existing values. The owner can judge proportions and visibility in
the headset rather than author every vertex. This is a feasible workflow, not
a claim that an interior asset has been made or validated.

Image-to-3D systems such as [Microsoft TRELLIS.2](https://github.com/microsoft/TRELLIS.2)
can generate textured mesh assets. Treat generated parts as drafts. My
engineering assessment is that a complete fitted cabin is better controlled
with modular/procedural geometry: generated output still needs correct scale,
clean usable surfaces, material setup and separate pieces for animated gauges
or a steering wheel. An exterior image cannot establish an unseen, authentic
dashboard layout. For a fictional or deliberately generic interior, exact
real-car knowledge is unnecessary. [dbce-wheel-mod-toolkit, 2026-09-16]

Unity can load models, materials and prefabs through
[AssetBundles](https://docs.unity3d.com/6000.0/Documentation/Manual/AssetBundlesIntro.html).
A proposed cockpit companion would load our own assets and parent them to the
selected car. Exact Unity version, render pipeline, shaders and available APIs
must be checked per game; bundles are not assumed interchangeable. A simple
runtime-generated mesh is an alternative for the first prototype.

The integration may need window material changes, hiding an opaque body panel,
or replacing part of the visible car shell. Merely placing a dashboard behind
opaque windows is insufficient. Do not hide the entire car before checking
what that removes from the bonnet, shadows and exterior views. Functional
mirrors, animated hands and wipers can follow later. Use original or appropriately
licensed assets for distribution. [dbce-wheel-mod-toolkit, 2026-09-16]

## Headset route and bounded next experiments

Keep the Windows game, mods and USB wheel on the PC. For Quest 3, Virtual Desktop
can carry SteamVR/OpenVR output; an OpenXR integration can instead use
[VDXR](https://github.com/mbucchia/VirtualDesktop-OpenXR). An older OpenVR-only mod
does not become OpenXR simply because VDXR is selected. Start with the mod's
documented runtime before adding a translation layer.

Valve's [Steam Frame documentation](https://partner.steamgames.com/doc/steamhardware/steamframe)
describes PC streaming with SteamVR/OpenXR compatibility. That gives a sensible
later validation target for the same PC-side mods. It does not establish that
our exact combinations work, or that Windows wheel mods will run standalone
on the headset. No release-date or availability assumption is needed.

Proposed sequence, not work already performed:

1. **Quick compatibility checks:** current #DRIVE build with UUVR; current Art
   build with the most suitable existing VR route. Establish stereo, head
   translation, scene transitions, UI and frame-time behavior before new art.
2. **First companion mod:** Art bonnet VR with coordinated UMM/VR camera
   ownership, recenter, readable menus and verified coexistence with wheel/FFB.
   Start diagnostic runs without torque, then perform an attended wheel test.
3. **One-car cockpit:** inspect the body/windows, make a generic low-poly cabin,
   fit it and inspect normal leaning/side views. For DRIVE, inspect the shipped
   interior before building anything new.
4. **Woden and Arcade:** bounded modern-IL2CPP UUVR checks. Reuse concepts and
   original cockpit parts only after each game's stereo rendering and camera
   lifecycle pass. Stop and reassess if an essential shader/culling path cannot
   produce a usable two-eye view.

Separate installable VR companions are sensible. Keep game hooks, camera/UI
changes and cockpit assets in the respective game projects; the wheel toolkit
continues to own device/force/telemetry infrastructure. A small explicit camera
ownership interface between each wheel mod and VR companion is preferable to
silently competing transform writers. [dbce-wheel-mod-toolkit, 2026-09-16]

## Local source anchors

| Project / HEAD at inspection | Sources read |
|---|---|
| art-of-sim-rally / `9c31579` | `docs/FINDINGS.md`, `docs/CAMERA.md`, `src/ArtOfSimRally.Mod/BonnetCamera.cs` |
| drive-rally-wheel / `07cbf32` | `docs/STATE.md`, `docs/RESEARCH.md`, local ignored decompilation of `CameraType` and `CameraCarFollower` |
| woden-rally-edge-wheel / `7fcbbbd` | `README.md`, `docs/STATE.md`, `src/WodenRallyEdge.Plugin/MountedCamera.cs` |
| iracing-arcade-wheel / `54c305c` | `AGENTS.md`, `docs/CAMERA.md`, `src/IRacingArcadeWheel.Plugin/MountedCameras.cs`, `ChaseCamera.cs` |

The wider initial survey also found Gravel's UEVR route, OutRun 2006's custom
x86/D3D9 camera/render hooks, and Cruis'n's projected-geometry renderer. Those
remain outside this recommendation following the owner's four-game focus.
