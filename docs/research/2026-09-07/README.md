# Racing-game mod feasibility research

Research completed 2026-09-07; all four reports collected into this checkout.
[dbce-wheel-mod-toolkit, 2026-09-07]

Four research tasks covered six Windows PC games, using isolated toolkit
worktrees. Their completed initial feasibility reports are linked below.
Start with the [combined findings and recommended next experiments](SUMMARY.md).
These are research conclusions, not implementation or gameplay-validation claims.

Follow-up: [Super Woden: Rally Edge assessment, 2026-09-12](../2026-09-12/super-woden-rally-edge.md).
See the [research index](../README.md) for all seven games.

## Streams and deliverables

| Stream | Games | Completed report |
|---|---|---|
| Horizon Chase | Horizon Chase Turbo; Horizon Chase 2 | [Horizon Chase](horizon-chase.md) |
| Slipstream | Slipstream | [Slipstream](slipstream.md) |
| Rush Rally | Rush Rally Origins; Rush Rally 3 | [Rush Rally](rush-rally.md) |
| V-Rally | V-Rally 4 | [V-Rally 4](v-rally-4.md) |

The four reports were reviewed and copied from their task worktrees, with
matching SHA-256 hashes verified after copying.
The sequel listed in the request as "Horizon Chase Turbo 2" is officially
[Horizon Chase 2](https://store.epicgames.com/p/horizon-chase-2?lang=en-US).

## Questions every report must answer

For each game, assess wheel input, telemetry, FFB and bonnet/hood camera
separately. Establish which features already exist before proposing additions.

- Identify the PC distribution, inspected build, engine, runtime and architecture.
- Distinguish native wheel support from gamepad emulation; assess steering,
  independent pedals, shifters, bindings and modern device compatibility.
- Find existing telemetry interfaces and integrations, then identify available
  car-state fields and a credible export route where needed.
- Separate existing FFB, physics-derived signals and synthesized arcade cues.
- Confirm available camera modes. For a new bonnet view, examine projection,
  geometry, sprites, culling and track visibility as well as camera hooks.
- Identify reusable mods and techniques, likely loader/hook routes, blockers,
  feature-specific feasibility/confidence and effort ranges with assumptions.
- Specify the smallest experiment that can confirm or reject each uncertain route.

Evidence must distinguish official documentation, static local inspection,
original mod code, community reports and inference. No feature is treated as
absent merely because a search did not find it. Cite sources with review dates.

## Local installation inventory

Read from Steam manifests and confirmed install directories on 2026-09-07.
The Steam common directory is `D:\Program Files (x86)\Steam\steamapps\common`.
[dbce-wheel-mod-toolkit, 2026-09-07]

| Game | Steam app | Installed build | Directory under common |
|---|---|---|---|
| Horizon Chase Turbo | 389140 | 20246174 | `Horizon Chase` |
| Slipstream | 732810 | 15704123 | `Slipstream` |
| Rush Rally 3 | 2020860 | 21554208 | `Rush Rally 3` |
| V-Rally 4 | 658700 | 3570632 | `VR4` |

No matching Rush Rally Origins or Horizon Chase 2 installation was found in the
Steam/Epic manifests checked during setup. This does not establish ownership or
rule out another installation location. Their research can proceed from public
sources; binary-specific conclusions remain pending local evidence.

## Initial evidence affecting scope

[Rush Rally 3's developer-authored Steam description](https://store.steampowered.com/app/2020860/Rush_Rally_3/)
already advertises configurable wheel input and force feedback. Its stream must
assess remaining compatibility, telemetry and camera needs before recommending
a replacement device layer. Reviewed 2026-09-07.

## Toolkit boundary and validation

Read `docs/STATE.md` and `docs/CONSUMER-EXPERIENCE.md`. The source/package version
is 0.12.0; the native device component is 0.5.0. Game hooks, samplers and cameras
belong in future per-game repositories. Shared device, force and telemetry
plumbing belongs here. The shared input kit and SDL backend remain future work.

The initial streams use web research and read-only static inspection. No game
launch, deployment or hardware force test is part of this research pass. The
owner's MOZA R12, SIMAGIC DS-8X and MOZA stalk inform compatibility questions;
their operation in these games has not been tested by these streams.

The combined findings compare feature-level confidence and the cost of the next
experiment. Successful loader execution, telemetry on the wire, camera rendering
and attended wheel-feel validation remain future work.
