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

Recommended next work: preserve Art as the attended reference; try Woden's
explicit lower-gain/higher-cap candidate, collect representative iRacing driving,
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
