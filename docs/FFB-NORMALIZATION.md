# Cross-game force normalization

**Art of Rally is the gold standard** (owner direction October 6; STD-021 refines
STD-003). Hold Art's reference tuning fixed. At Strength 50 in both games, tune
other games' comparable steering, crashes, landings and other available effects
toward Art's level and character. Do not ask the owner to use 35 in one game or
75 in another to satisfy the 50-to-50 requirement.

The versioned software reference is
[`art-of-rally-50-v1.json`](../profiles/normalization/art-of-rally-50-v1.json),
pinned to Art's 0.4.1 implementation `ddabadaccb02f43a93a6492b0fb647555133b33e`.
Code identity is independent of release publication and current saved settings.
Preserve this reference when making candidate calibrations.
The first automated baseline is available in
[the October 6 findings](../knowledge/FFB-NORMALIZATION-2026-10-06.md).
It measures **software requests**, not delivered torque. No runtime tune,
installed setting or wheel profile was changed for that comparison.

## What 50 means in the Art reference

Steering Strength 50 gives gain 1, reference 11,500 N and smoothing 0.2. Art's
landing, crash and shift strengths are separate from steering Strength, so the
whole per-effect profile is pinned rather than setting every slider to 50.

| Component, when enabled | Full-intensity reference request | Default enablement |
|---|---|---|
| Crash push | 0.50 nominal constant-force magnitude, positive X, 120 ms | Off |
| Crash rattle | 0.25 amplitude, 25 Hz, 250 ms, fade over final 150 ms | Part of enabled crash cue when supported |
| Landing | 0.05 sine amplitude, 25 Hz, 120 ms | On |
| Gear shift | 0.05 sine amplitude, 25 Hz, 120 ms | Off |

The profile contains production request vectors at normalized intensity 0.25,
0.5, 1 and 2 (saturated to 1). These are **synthetic production-model requests**,
not measured torque or equal physical crash severity across games. Twelve cases
pass 63 checks through pinned LandingFeedback with a fake sink. Optional cues
are enabled only in the synthetic calculation; saved opt-outs are unchanged.
Do not sum rattle amplitude, push and steering peak into claimed wheel torque:
native effect mixing and the base's limits require separate evidence.

For impacts, retain onset, physical severity inputs where known, peak/amplitude,
duration, frequency, fade, repeat suppression and overlap policy. For steering,
retain speed, load/slip, release and saturation. Art's steering CSV omits its
separate impact effects and cannot qualify crashes. Different tracks at the
same speed remain different workloads. A missing counterpart is unavailable,
not a measured zero; do not invent a new effect to fill the comparison.

## Reproduce the comparison without a rig

Python 3.10+ and each consumer's documented .NET SDK are sufficient for the
managed exports. No game launch, native wheel library, window, input injection
or UDP sink is part of these commands. Start in each named repository root.
Use fresh output names. Keep original references and results outside disposable
checkouts; never edit a source recording to fit a proposed tune.

```powershell
# Art: validate the two original owner captures against production AxleForceCurve.
dotnet run --project tools/testing/Replay -c Release -- --corpus results/regression-corpus/index.json

# Woden: original sealed stage, new CSV, strength, peak percent.
dotnet run --project components/wheel/tools/TelemetryInspector -c Release -- ffb-export $reference $newCsv 35 50

# iRacing Arcade: original session.jsonl, new CSV, strength. Saved reference,
# low-force boost, smoothing and speed fade remain part of this trial.
dotnet run --project components/wheel/tools/IRacingArcadeWheel.Diagnostics -c Release -- ffb-export $sourceJsonl $newCsv 50

# DRIVE: source.jsonl inside a sealed stage reference, new CSV, strength.
dotnet run --project tools/DriveRally.Diagnostics -c Release -- ffb-export $sourceJsonl $newCsv 50
# To include original dirt requests in the current road-texture model:
dotnet run --project tools/DriveRally.Diagnostics -c Release -- ffb-export $sourceJsonl $anotherNewCsv 50 --recorded-dirt

# F-Zero: build with the repository's CMake/MSVC environment first.
cmake --build build-merge --target fzero_force_trace fzero_ffb_tests
./build-merge/fzero_force_trace.exe $newCsv 50

# Toolkit root: analyze pinned files. The output directory must be new.
python tools/replay/force_normalization.py $manifest $newReportDirectory
python -m unittest discover -s tools/replay -p 'test_*.py'
```

Woden and iRacing validate the original model before exporting a trial. DRIVE
validates the original stage but its older recording predates the current mod
FFB: the export is a counterfactual, not a recorded mod-force request. DRIVE's
collision input is omitted, not measured zero. Road input is also omitted unless
`--recorded-dirt` is selected. The exporter pins inputs, executable/model bytes,
settings and coverage in a new `.meta.json` sidecar. See the consumer's
`docs/FFB-COMPARISON.md` for the separately preserved dirt comparison.
F-Zero exports 601 synthetic rows with no native device backend compiled in.
Its stock/Deluxe recorded-drive identity is not established by this fixture.

### Joining an effect tape to physics samples

Use a recorded common clock with known producer semantics. DRIVE's stock rumble
is updated in the car's render `Update`, while solver samples are captured in
`FixedUpdate`'s postfix. Both `effects.tsv.time_s` and `sample.wallSeconds` use
absolute `Plugin.Clock`. The session's `elapsedSeconds` uses a separate origin
and must not be joined directly to effect time. The DRIVE adapter uses the last
strictly preceding request, excludes unknown/stale inputs and never looks ahead
or interpolates random rumble. Preserve measured zero separately from missing.

This temporal reconstruction does not recover the initial hidden filter or
texture-oscillator state of a model that predates the capture. Label the export
counterfactual and disclose resets and omitted components. For new captures,
prefer raw effect inputs and their availability on the same sample as computed
force, effective settings, model version and reset state. Compute before the
recording hook and mute at the physical/network sinks. Native-library failure
must not disable game-signal observation or silently turn unavailable inputs
into claimed measurements. API acceptance is not measured wheel torque.

## Comparison manifest, version 1

The shared tool owns statistics and artifact identity. Consumer adapters own
game channels, model execution, configuration and validation. Use a manifest
with `schema: "dbce.ffb-normalization"`, `version: 1`, and a `series` array.
Each series has the following shape (replace hashes/paths with real values):

```json
{
  "id": "example-50", "game": "Example",
  "quantity": "constant-force-request", "evidence": "counterfactual",
  "model": "production-model@version-and-source-revision",
  "settings": {"strength": 50},
  "limitations": "Steering only; no collision impulses or physical output.",
  "validation": "Original model replay passed; pinned validation report below.",
  "artifacts": [{"path": "validation.json", "sha256": "64-hex-sha256"}],
  "source": {"path": "trial.csv", "sha256": "64-hex-sha256"},
  "format": "csv",
  "mapping": {"time": "time_s", "value": "request", "valid": "valid",
              "epoch": "epoch", "speed": "speed_kmh", "speedScale": 1},
  "cap": 1, "maxGapSeconds": 0.1
}
```

New comparisons should also have a top-level `reference` artifact pointing to
a retained copy of the versioned profile:

```json
"reference": {
  "path": "art-of-rally-50-v1.json",
  "sha256": "0500feda949dac31803e8d904293464e5e2e3692a861a0ca0a53afebb8730287"
}
```

The analyzer checks its hash/schema/identity and retains the full reference in
the report. It does not rescale series or rename a non-50 trial as calibrated.
Historical version-1 manifests without a reference still run as historical
comparisons. The new 17-series gold-reference report reproduces every earlier
statistic exactly while explicitly naming the fixed reference.

Paths resolve relative to the manifest; private absolute paths are also allowed.
Pin the original input, model source/binaries, exporter and adapter validation
report as artifacts for a counterfactual. The manifest hash seals the chosen
settings/mapping. The analyzer verifies hashes; it does not interpret an
arbitrary validation report as proof of game correctness. Obtain that proof
with the consumer validator first. Raw captures, personal paths, game/state
files and the local manifest stay out of public Git/packages.

`format` is `csv` or `session-jsonl` (the toolkit Recording stream). For JSONL,
mapping keys select sample channels, except `$elapsed` selects elapsed seconds.
The reader requires metadata, contiguous sample sequence, a complete matching
footer, and zero dropped/invalid/error/queue-full counts. CSV needs adapter
validation and a pinned source hash. Missing/nonfinite selected values fail
instead of becoming zeros. Requests must be in [-1,1], time nonnegative and
monotonic, valid exactly 0/1. Equal timestamps are counted and have zero duration.

`valid`, `epoch` and `speed` mappings may be omitted (all valid, one epoch,
speed unavailable respectively). Speed is an absolute magnitude; `speedScale`
converts to km/h, e.g. 3.6 for m/s. Do not invent a physical speed for F-Zero's
world-units-per-frame proxy. Allowed quantities are constant-force-request,
spring-coefficient, damper-coefficient and periodic-amplitude; allowed evidence
levels are recorded-model-output, counterfactual and synthetic. Keep effect
families separate. A collision edge is not a force magnitude.

## Statistics and interpretation

Each sample holds until the next timestamp. There is no invented final tail.
Intervals crossing a reset epoch, following an invalid sample or exceeding the
declared maximum gap are excluded and counted by reason. RMS and absolute
percentiles are duration weighted, preventing 100 Hz recordings from outweighing
60 Hz recordings. Reports separate 0–5, 5–20, 20–60 and 60+ km/h; absent coverage
is unavailable, not a zero-force result. `validSamplePeak` includes the final
sample; interval peaks only include samples with accepted duration.

`atCapFraction` counts output within 0.0001 of the declared cap (or above it).
`atUnitFraction` uses full nominal range. These are final-output occupancy, not
an internal clipping detector: smoothing can hide earlier saturation. A high
occupancy is actionable; zero occupancy does not prove no internal clipping.

Do not multiply an already clipped/smoothed baseline to produce a candidate.
Recompute the production model from original signals, retaining reset history.
Different tracks, tyres, assists and maneuvers are different workloads. Speed
bands help expose coverage gaps but do not by themselves match corner severity.
The tool deliberately makes no automatic cross-game gain recommendation.

## Game mapping pitfalls

| Game | Useful original stream | Do not mistake for the same thing |
|---|---|---|
| Art | forces.csv `output`, `time_s`, `epoch`, `speed_kmh` | Separate crash/landing/shift effects are absent from this steering CSV. |
| Woden | `analysis.force.preview/valid/reset`, `motion.speed` | `ffb.preview` is zero in this muted capture because actual delivery lifecycle resets each tick. The independent analysis model preserves force history. |
| iRacing | `ffb.Output` and saved `ffb.*` tune, `car.SpeedMs` | `NativeCommand` and DeliveryScale are delivery gates, not the upstream model force. Its current reference is a low-speed diagnostic. |
| DRIVE | Production DriveForceModel over original solver channels | Stock Logitech effect requests are not the new mod's signed steering force. Kinematic replay contacts are not original solver observations. |
| F-Zero | Separate spring/damper coefficients and road amplitude | Conditional effects depend on physical wheel displacement/velocity. Fallback constant is used only if spring is unavailable; do not add both. |

## Tomorrow's attended checks

Keep the same wheel/base profile and note every mod's exact tune. Use Art's
pinned 50 profile as the reference. Check centering direction, normal
corners, release in a slide and recovery before comparing impacts separately.
Woden's earlier Strength 35 / cap 50 trial demonstrates the gain/cap tradeoff;
it is not the requested final calibration. Develop candidate mappings with
the displayed Strength still 50 and disclose internal gain/shaping/cap changes.
Raising the cap changes possible peak output and needs an attended comparison.
The earlier DRIVE 75 trial is likewise diagnostic, not a substitute for
calibrating its 50 setting. Preserve saved settings until an explicit trial.

iRacing needs a new normal-speed owner drive; its short diagnostic cannot settle
cross-game cornering feel. DRIVE needs a new capture on the current FFB runtime
with calculation preserved while only physical/network sinks are muted. F-Zero
needs a validated reference and wheel displacement/velocity context for condition
effects. Record owner acceptance separately from model tests. No unattended
physical-force test is needed for this offline stage.
