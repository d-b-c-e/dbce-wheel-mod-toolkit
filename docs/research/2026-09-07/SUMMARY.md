# Six-game feasibility findings

Assessment date: 2026-09-07. Synthesis of the four completed research streams.
[dbce-wheel-mod-toolkit, 2026-09-07]

**Horizon Chase Turbo is the strongest first candidate for a new wheel mod.
Rush Rally 3 and V-Rally 4 already supply wheel input, FFB and bonnet cameras;
telemetry is their main potential addition. Slipstream is promising for arcade
wheel input/FFB, but its camera requires a different approach.** These priorities
are engineering judgments from the reports, not results of runtime experiments.

## Feature-by-feature result

| Game | Wheel input | Telemetry | FFB | Bonnet camera |
|---|---|---|---|---|
| **Horizon Chase Turbo** | High feasibility: readable float steering/throttle/brake hooks. Stock PC wheel interface returns null. | High feasibility for speed, controls, race state and game-generated gear/RPM; physical tyre/suspension data not found. | Good candidate for synthesized arcade forces from curve, speed, drift, surface and collision state. | Plausible, with sampled-road/culling risks. Senna has a dedicated cockpit path; standard cars need a new view and testing. |
| **Horizon Chase 2** | Plausible, but PC input implementation not inspected. | Basic export plausible; no player-state hook or native interface verified. | Synthesized cues plausible; actual tyre-force availability unknown. | Real 3D world is encouraging, but hood geometry, camera hooks and road visibility remain untested. |
| **Slipstream** | Feasible with new analog input handling: stock gamepad axes are thresholded into digital flags before driving. | Accessible managed car state supports useful dashboard/event data. RPM/gear would need explicit synthesis where used. | Synthesized centring, damping, drift, surface and collision cues are plausible. | Stylized forward view with original hood artwork is plausible. A faithful 3D bonnet camera would require major renderer/content work. |
| **Rush Rally Origins** | Official wheel support exists; mixed-device compatibility needs investigation. | Plausible from its shared Rush Engine ancestry; no current binary or exporter verified. | Native effects remain unverified; inspect before proposing replacement. | High-risk experiment: overhead-oriented rendering may not supply usable low-angle scenery and visibility. |
| **Rush Rally 3** | Already native, including mixed-device support. Diagnose the actual rig before proposing a fix. | Main new feature: native player-state extraction looks plausible but no hook/format is verified. | Already native. Preserve its signal and repair delivery/lifecycle only if a fault is demonstrated. | Already native: dedicated class, configuration and bonnet attachment evidence. |
| **V-Rally 4** | Already native, with separate-device presets and direct gear actions. A compatibility mod may be unnecessary. | Main new feature: FMOD/car-state sampling is a concrete discovery route. D-BOX support is not a public data-export API. | Already native, with steering, damping and event effects. Assess configuration and delivery first. | Already native, confirmed in decompressed camera configurations. Optional adjustments must preserve separate car/world rendering passes. |

Full evidence, sources and build identities: [Horizon Chase](horizon-chase.md),
[Slipstream](slipstream.md), [Rush Rally](rush-rally.md),
[V-Rally 4](v-rally-4.md). Existing RR3 wheel/FFB support is also explicitly
advertised on its [developer-authored Steam page](https://store.steampowered.com/app/2020860/Rush_Rally_3/);
V-Rally's native setup is documented in the
[KT Racing/Nacon guide](https://steamcommunity.com/sharedfiles/filedetails/?id=1524655078).

## Findings that change the implementation plan

- **Turbo and its sequel need different loaders.** Turbo is x86 Unity 2018 Mono
  using the legacy .NET 3.5 profile. The toolkit's unchanged netstandard2.0
  assemblies are not a suitable baseline there; budget a legacy plugin/native
  bridge. Developer evidence identifies Horizon Chase 2 as Unreal, but its exact
  PC engine version and architecture remain unverified.
- **Slipstream has readable C# code, but loading is the first hurdle.** This
  installed build is FNA/.NET 8, bundled and trimmed, with startup hooks disabled.
  A custom managed launcher needs proof before input work. Its pseudo-3D road
  and sprite cars also make a forward view different from moving a 3D camera.
- **Rush Rally 3 and V-Rally 4 are native C++ projects.** Imports, metadata and
  configuration give good starting points, but protected/transformed on-disk
  code makes player-state discovery less direct than readable managed IL.
- **No verified public telemetry emitter was found for any of these builds.**
  That is a scoped research result, not proof no private integration exists.
  V-Rally's D-BOX path and WRC telemetry mod techniques are leads; neither proves
  a working V-Rally exporter. An online V-Rally UDP parser was not accepted as
  evidence because no matching game emitter or capture was established.
- **Hardware buttons do not establish transmission support.** Turbo has a
  synthetic automatic gearbox; Slipstream would need new drivetrain behavior
  for authentic H-pattern use. RR3 sequential actions exist, but H-pattern/clutch
  behavior is unresolved. V-Rally has direct reverse/1-6 actions; no promise is
  made for every DS-8X position or the complete mixed-device rig.

## Recommended next experiments

1. **Turbo: prove the loader, sampler and ordinary-car camera.** A device-disabled
   x86 legacy plugin should count input/update calls and log one player's speed,
   controls and race state. Test a reversible low view through a straight, crest
   and sharp corner before investing in camera polish. The report budgets
   0.5-2 days for loader work plus 1-3 days for the native bridge; camera viability
   is a further 1-3 days after a working hook.
2. **RR3 and V-Rally: establish the native baseline on the owner's rig.** Verify
   separate axes, binding persistence, gearbox actions and bonnet selection.
   If a fault exists, trace device enumeration or force delivery. For new
   telemetry, RR3's basic state-discovery proof is estimated at 3-10 days;
   V-Rally's FMOD/basic-state discovery at 3-7 days. Do not replace functioning
   native force calculations merely to use the toolkit.
3. **Slipstream: prove the managed launcher before committing.** Budget 2-5 days
   for startup/assets/Steam integration and counted hooks. Then test fractional
   steering/pedals and a sprite-suppressed forward view. The report estimates
   2-4 working weeks for an input/telemetry/synthetic-FFB prototype if loading
   succeeds; a robust release needs further setup and validation work.
4. **Horizon Chase 2 and Origins: inspect a current Windows installation.**
   Their assessments have less evidence because neither PC binary was available.
   Confirm runtime, input/FFB implementation and camera geometry before planning
   full builds. Origins' bonnet view is the largest unresolved content risk.

Estimates are overlapping one-engineer research ranges, not delivery commitments.
Turbo's report estimates roughly 3-6 person-weeks for a first supported single-build
release. Horizon Chase 2's provisional 4-8 person-weeks is substantially less
certain and starts only after access and initial discovery. Basic RR3/V-Rally
telemetry exporters are estimated at 1-3 weeks once a viable sampler is found;
rich tyre/motion channels are separate, more uncertain work. Prototype and
supported-release estimates should not be compared as equivalent deliverables.

## What is actually verified

All four reports completed web research and static local inspection where files
were available. Turbo, Slipstream, RR3 and V-Rally have recorded Steam builds,
binary fingerprints and concrete code/configuration evidence. Horizon Chase 2
and Origins rely on public sources and conditional implementation proposals.

No game was launched for these studies, no mod loader or sampler ran, no telemetry
packet was captured from a new mod, no bonnet view was visually tested and no
wheel force was applied. Compatibility and physical feel on the MOZA R12,
SIMAGIC DS-8X and MOZA stalk remain untested in these games by this research.
The next stage is the small experiments above, not a claim that six mods work.

Future game hooks, samplers, camera changes and settings belong in per-game
repositories. Reuse the toolkit's native device layer, force shaping/profiles,
packet encoding and diagnostics. Its source/package 0.12.0 and native component
0.5.0 are separate versions; the shared input/calibration kit and SDL backend
remain future work.
