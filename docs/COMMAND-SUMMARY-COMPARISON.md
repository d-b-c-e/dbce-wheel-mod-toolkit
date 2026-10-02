# Offline command-summary comparison contract

[dbce-wheel-mod-toolkit, 2026-10-02] Local source candidate only. This diagnostic
loads no device, game or native library and changes no runtime profile. Command
normalization is not physical torque calibration or a cross-game feel claim.

The existing `tools/replay/replay_case.py compare` compares ordered observations
from the same case bytes and clock under different tunes. Its identity checks
remain unchanged. `command_summary.py` reuses its bounded JSON decoder, numeric
validation and deterministic encoder for a separate summary/report contract.

## Verified compatibility matrix

Woden's available artifact is synthetic production-controller output with a
fake accepting device. Inspected OutRun tooling includes
`tools/tests/signal_recording_fixture.cpp`; inspected F-Zero tooling includes
`src/fzero_ffb.cpp`, `tests/test_ffb.cpp` and `tools/timing_replay_summary.py`.
Replay, lifecycle and timing checks do not establish equivalent command-summary
definitions. No equivalent OutRun/F-Zero summary artifact was located in the
inspected snapshots. This is an evidence gap, not a claim about all current
consumer tooling. Consumer adapters stay consumer-owned.

| Quantity | Verified Woden model-3 summary | OutRun2006 | F-Zero |
|---|---|---|---|
| Command magnitude | Signed constant request; absolute peak and uniform-sample RMS | Unsupported: unit mapping/summary unverified | Unsupported: unit mapping/summary unverified |
| Cap occupancy | Fraction of 300 samples: abs(command) >= 0.25 - 0.000001 | Unsupported: cap/threshold semantics unverified | Unsupported: cap/threshold semantics unverified |
| Duration | 300 injected samples * 0.02 s = 6 s, shutdown excluded | Unsupported: envelope/clock unverified | Unsupported: envelope/clock unverified |
| Duty | Fraction: strict abs(command) > 0.0015 | Unsupported: sampling/threshold/effect semantics unverified | Unsupported: sampling/threshold/effect semantics unverified |
| Slew | Max abs(current-previous)/0.02 s; initial zero, gate zeros included, shutdown excluded | Unsupported: clock/effect semantics unverified | Unsupported: clock/effect semantics unverified |
| Shutdown zeros | Fake-device ZeroAndStop calls after envelope; order retained separately | Unsupported: primitive/order unverified | Unsupported: primitive/order unverified |

All three game pairs remain unsupported. Unsupported quantities have no numeric
value or delta; zero is never a substitute. Native periodic, rumble and condition
effects cannot be combined with constant steering by summing magnitudes or
calling them Nm. Different cap thresholds, sample weighting, clock semantics,
shutdown boundaries or zero primitives need different definition identifiers.
Equal normalized commands do not imply equal physical torque or feel. No crash
signal or calibration value is invented; Woden's collision channel is unavailable.

## Report v1

From the repository root, using Python 3.9 or newer:

```text
python tools/replay/command_summary.py tools/replay/fixtures/woden-ffb-reference-v1.json
python -m unittest discover -s tools/replay -p "test_*.py" -v
```

The CLI reads at most 1 MiB and writes deterministic JSON to stdout, with no file
mutation. Exit 0 means valid diagnostic, including unsupported comparisons;
exit 2 means invalid input. Schema `dbce.wheel.command-summary-report`, version
1, contains `inputs`, `crossGameComparisons`, `physicalAcceptance:false` and
`crossGameFeelEquivalence:false`. A supported metric has `value`, `unit` and an
exact `definition` identifier. An unsupported metric has `status` and `reason`.
`compare_metrics` computes a scalar delta only for matching units and complete
definitions. This establishes quantity compatibility, not matched stimuli or
model equivalence. Future adapters need consumer evidence and deterministic tests
before adopting an existing identifier.

The sole adapter accepts Woden schema `dbce.woden.ffb-regression-summary@1`, model
3, its fixed options, 20 unique shape/strength cases, 300 samples at 20 ms and
the explicit nonphysical boundary. It retains producer metadata, options,
strength comparisons and complete original rows. It validates finite/bounded
metrics and occupancy/duty consistency. Summary-only input/command/dependency
hashes remain producer claims; only artifact bytes are independently hashed.
Absent source revision/native version is unsupported and is not inferred from
toolkit defaults or historical rig identity.

## Fixture provenance and pending evidence

The JSON is the existing authorized synthetic Woden reference, copied exactly
from the `woden-summary-cli-error-candidate` snapshot's
`components/wheel/tests/WodenRallyEdge.Tests/Fixtures/ffb-reference-v1.json`.
SHA-256: `41bf64ca0fbcded234c27f65a075ef1a11fc3a37707dd606fd0d20cca9076d57`.
The saved correction receipt associates its reference Git blob
`353e8f7664b47d1907707bb8a885dba4da978b03` with historical source
`95d1ff7ce57eb816a5f6b662b80923a3bec9e2ca`.
Inspected `FfbRegressionChecks.cs` SHA-256:
`a3d714059b10633fa8067dac308bf44a73b211132648352714867e296636e88d`,
matching that receipt. This does not assert a fresh consumer HEAD. The summary
retains shaper SHA-256
`f5217f81b5bd305e0efca3aacc459b9066c4366cd220c3e4299c511e23b5431a`.
No native DLL, ROM, private recording, binary or fabricated waveform is included.

Remaining adapters require each consumer's exact authorized summary fixture and
generator, source/output hashes, native/component versions (or fake boundary),
model/options, clock integration, cap/activity thresholds, effect interpretation,
slew/shutdown boundaries and zero-call semantics. Unknowns stay unsupported.
Do not infer normalization factors from command peak or a historical wheel model.

This isolated candidate is held for independent source review and the hosted-usage
gate. No push, hosted workflow or default-branch mutation has been performed.
