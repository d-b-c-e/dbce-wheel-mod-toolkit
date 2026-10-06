# Cross-game force normalization

The owner wants Strength 50 to feel comparable across the portfolio (STD-003).
The first automated baseline is available in
[the October 6 findings](../knowledge/FFB-NORMALIZATION-2026-10-06.md).
It measures **software requests**, not delivered torque. No runtime tune,
installed setting or wheel profile was changed for that comparison.

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
accepted steering as the initial reference. Check centering direction, normal
corners, release in a slide and recovery before comparing impacts separately.
Woden's offline Strength 35 / cap 50 trial is a candidate for an explicit short
A/B: it reduces clipping while allowing brief peaks above the old 25 cap. It is
not installed or accepted. Restore the original tune after the comparison unless
the owner chooses the candidate. Do not describe raising the cap as harmless.

iRacing needs a new normal-speed owner drive; its short diagnostic cannot settle
cross-game cornering feel. DRIVE needs a new capture on the current FFB runtime
with calculation preserved while only physical/network sinks are muted. F-Zero
needs a validated reference and wheel displacement/velocity context for condition
effects. Record owner acceptance separately from model tests. No unattended
physical-force test is needed for this offline stage.
