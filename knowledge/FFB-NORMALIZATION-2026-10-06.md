# First cross-game FFB baseline — 2026-10-06

[dbce-mods-art-of-rally, dbce-mods-woden-rally-edge,
dbce-mods-iracing-arcade, dbce-mods-drive-rally, dbce-mods-fzero-snes,
dbce-wheel-mod-toolkit, 2026-10-06]

Owner assigned normalization across these five games and will test tomorrow.
This pass adds a reusable offline analyzer and consumer production-model exports.
No installed tuning or physical-output settings changed. Art's existing five
pending commits were pushed to main at e979c25 with explicit owner authorization;
installed 0.4.1-rc.2 still needs the already documented attended checks.

## Results, nominal software request range

| Case | RMS | Absolute P95 | Peak | Time at configured cap |
|---|---:|---:|---:|---:|
| Art Norway owner, strength 50 | 43.22% | 75.61% | 99.78% | 0.00% |
| Art crash owner, strength 50 | 15.29% | 37.79% | 75.82% | 0.00% |
| Woden Kenya owner analysis, 49.583332 / cap 25 | 19.02% | 25.00% | 25.00% | 46.46% |
| Woden trial, 50 / cap 50 | 30.80% | 50.00% | 50.00% | 8.87% |
| Woden trial, 35 / cap 50 | 22.29% | 36.54% | 40.03% | 0.00% |
| iRacing diagnostic, saved 72 + boost 0.4 | 18.71% | 35.02% | 98.99% | 0.00% |
| iRacing trial, 50 + same boost | 14.96% | 25.99% | 98.96% | 0.00% |
| DRIVE owner signals / current model, 50 | 29.32% | 58.60% | 72.46% | 0.00% |
| DRIVE same source/model, 75 | 43.84% | 87.91% | 100.00% | 1.05% |

These are different workloads, not a ranking of games' strength. Art's two
same-tune drives differ greatly; that alone rules out equalizing whole-drive
RMS as a valid calibration method. iRacing spends 50.93 of 59.86 analyzed seconds
at 5–20 km/h and has no 60+ coverage. DRIVE impacts are excluded. Woden excluded
invalid/reset-boundary intervals and uses its independently recorded analysis
model, not delivery-muted zeros. Refer to the full local report for speed bands.

F-Zero's 601-row synthetic response reaches spring 0.50, damper 0.20 and normal/
rough road amplitudes 0.20/0.35 at Strength 50. These are effect parameters, not
constant-force equivalents. At current default 40 the corresponding maxima are
0.40, 0.16 and 0.16/0.28. Collision detection produces one labelled event edge;
its separate magnitude/duration is not inferred from that edge. No recorded
F-Zero/Deluxe drive was qualified in this pass.

## Why copying a reference/default fails

Art and DRIVE both use 11,500 N references, but their trail curves, filter/soft
saturation chains, damping and event composition differ. Woden uses contact-load
weighted sideways-slip, with a 25% output cap and Strength/100 before shaping.
iRacing uses an engine-scale aligning proxy, Strength/50, low-force boost and
its own speed fade; this capture's saved reference is 4500, not default 5000.
F-Zero's main centering is a condition effect evaluated by the device. Neither a
shared number nor a matching output histogram establishes equivalent feel.

Recommended next work (refined by STD-021): preserve Art's fixed 50 reference;
use Woden's lower-gain/higher-cap experiment to develop a candidate at displayed
Strength 50, collect representative iRacing driving,
qualify DRIVE producer data during muted capture, and keep F-Zero condition
effects separate. Do not silently double Woden's gain to make 50 mean unity.

## Validation and evidence

Art: 11,921 original force rows, zero model/device mismatches, 215,928 assertions.
Woden: sealed 3,601-row source, zero force reprocess errors. iRacing: 3,001 rows,
zero diagnostic issues/native calls. New Woden/iRacing baseline exporters agree
within 7.5e-9 / 3.0e-8 respectively (decimal text round-trip). DRIVE: sealed
6,001-pose/solver source; consumer 53 suites / 641 assertions pass. Exporters
call actual production models, never native force output. F-Zero model and fake
lifecycle tests pass after making the model target explicitly device-free.

The old F-Zero model test included a Windows device-discovery/init smoke and was
inadvertently invoked once during this investigation. It found no configured
wheel and sent no force. Model tests and the new trace now compile out the native
backend with FZERO_FFB_MODEL_ONLY; the separate lifecycle test retains fake APIs.
Do not infer device-free behavior from a test target's name.

Private immutable inputs/reports live under
`LocalAppData/Dbce/StagePlayback/SessionEvidence/ffb-normalization-20261006-01`:
`comparison-sealed.json`, `sealed-report/report.json`, `sealed-report/report.md`,
original-validation reports, baseline-export comparisons and independent CSV
trials. The final manifest has 17 series, pins original bytes/model artifacts,
and is replayable using [the runbook](../docs/FFB-NORMALIZATION.md). Its `inputs/`
contains hash-exact independent copies of every selected source/model/validation
artifact; `art-corpus/` also preserves both complete original Art cases so later
checkout cleanup cannot lose them. The 46 shared replay/analyzer tests pass.
No game was launched or display setting changed. These results do not close
STD-003 physical normalization or a game's release qualification.

## DRIVE road-input follow-up (separate baseline)

Claude's `efe617d` replaced the static-load road term with the game's dirt-road
request. Consumer exporter `bbb9771` now accepts `--recorded-dirt`, with common
absolute-clock alignment, no future/equal-time event lookup, a 100 ms age limit,
explicit missing coverage and per-export provenance. An isolated checkout passed
61 suites / 717 assertions. No runtime tune was changed.

The owner Holzberg reference has 10,395 rumble events. Of 6,001 physics samples,
6,000 have usable preceding dirt, 4,124 nonzero; maximum age is 50.428 ms. Only the
first sample lacks a preceding event. The new model's Strength 50 output with
dirt has RMS 29.33%, absolute P95 58.60%, peak 72.47%, no cap occupancy. Its paired
difference from omitted-road output has RMS 0.270 percentage points and maximum
2.276 points. The road term is present but has little effect on whole-drive RMS;
that statistic alone is insufficient to judge texture or physical feel.

Strength 75 with dirt has RMS 43.85% and cap occupancy 1.05%; it remains an
uninstalled trial. Collision impulses are omitted in both comparisons. Model
filters/oscillator restart at export, so this is not exact live-force replay.

The earlier 17-series sealed comparison above remains untouched. Independent
new evidence lives under
`LocalAppData/Dbce/StagePlayback/SessionEvidence/drive-dirt-comparison-20261006-01`:
`comparison-sealed.json`, `sealed-report/report.md`, the complete sealed original,
four CSVs/sidecars, clean-source archive and exact binaries, test log, analyzer
and dependency. Rerun the copied analyzer into a new directory. Keep effect
clock, physics clock and session-relative clock semantics in each adapter's
documentation; never guess offsets from the first observed rows.

## Owner gold-standard direction and pinned response — October 6, 20:12 CT

The owner's 17:50 direction makes Art the fixed target, not merely the first
comparison case. STD-021 already records the decision. The versioned profile
`profiles/normalization/art-of-rally-50-v1.json` pins source ddabada (0.4.1 model),
Strength 50 and all separate impact settings/default enablement. No release or
new physical acceptance is implied by freezing that implementation.

A device-free .NET executable compiled the exact committed LandingFeedback and
ImpactMixer source into a fake output fixture: 12 normalized-intensity cases and
63 checks pass. At full intensity, crash strength 50 requests a .50 push for
120 ms plus .25/25 Hz rattle for 250 ms (last 150 ms fade); landing/shift defaults
request .05/25 Hz for 120 ms. Crash and shift remain default off. Steering
Strength does not multiply these separate cue controls. The profile records
this explicitly; the owner's saved crash/landing strengths are not migrated.

The analyzer accepts an optional hash-pinned reference artifact. New checks
verify unchanged statistics, full reference retention and refusal of tampered,
unknown, invalid-strength or unidentified references. All **48** shared replay/
normalization tests pass. A new report over the original **17** series retains
every previous statistic exactly; those diverse workloads still cannot establish
an event-matched calibration. Woden 35 and DRIVE 75 remain diagnostic trials,
not recommended slider positions for the requested common-50 behavior.

Private evidence at `SessionEvidence/art-gold-reference-20261006-01` includes
committed source/defaults, the standalone .NET project and fake sink, executable
cue outputs, profile, 35 independently copied comparison inputs, copied analyzer,
new manifest/report and validation. Re-run its Reference.csproj with source.json
and a new effect-output path, or its force_normalization.py with comparison.json
and a new report directory. Source recordings and the earlier baseline were not
modified. Remaining work: matched event/severity captures and model calibration
at 50, then the owner's same-wheel/base-profile physical comparison.

## Comparison wrapper input closure — October 7 cross-review

Claude's hub review found that Compare-FfbToArt pinned Woden source.jsonl but
not force-config.json (consumed by ReadOptions) or complete.tsv (the capture
seal). Re-sealing a different tune could therefore change a trial without its
receipt identifying the changed input. The wrapper now pins all three in the
receipt and comparison artifacts, re-hashes them after export, and retains the
legacy source field. The seal covers the context/schema validated by the actual
exporter. Pin the complete set of inputs used by an export, not only its samples.

The same review found PS5.1 redirected native stderr could abort the analyzer
pipeline before logging/checking its exit. The wrapper now retains analyzer.log
under Continue, restores the prior error policy and checks the captured exit code,
as for export.log.18 boundary checks pass in PS5.1 and PS7, including real Python
stderr and per-input mutation; the old wrapper fails the stderr control.

The full frozen Woden Classic peak25 run under PS5.1 is byte-identical and all
existing statistics for Art/Woden remain unchanged. Earlier reports/receipts
stay immutable; these are provenance/diagnostic corrections, not normalization
or physical acceptance. Repeat commands: hub tools/Compare-FfbToArt.md and
tools/Test-CompareFfbToArt.ps1. Private SessionEvidence/ffb-wrapper-followup-20261007
retains logs, exact source and verification. The exporter's current flat DLL/JSON
copy layout does not support future runtime/satellite subfolders without review.
