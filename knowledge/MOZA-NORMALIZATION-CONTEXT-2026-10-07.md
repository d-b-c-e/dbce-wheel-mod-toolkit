# Saved device context for FFB normalization — October 7, 2026

Provenance: read-only inspection of local MOZA Pit House preset files;
`dbce-wheel-mod-toolkit` offline normalization analyzer; existing qualified
Art, iRacing, Woden and DRIVE exports. No game, device API or physical force.

## What the files establish

`Documents/MOZA Pit House/Presets/config.ini` has Motor auto-load enabled and
its saved last-used entry identifies the R12 **Arcade Xbox 270** custom preset.
The matching `.mzpreset` is a ZIP containing JSON, not a plain JSON file.

| Saved field | Value |
|---|---:|
| Game force feedback strength | 40 |
| Maximum torque | 75 |
| Maximum steering angle / game steering angle | 135 / 135 |
| Mechanical damper / friction / spring | 10 / 10 / 10 |
| Game force feedback filter | 50 |
| Force reversal | false |
| Equalizer gains 1–6 | 80 / 50 / 50 / 100 / 100 / 60 |
| Speed-dependent damping / initial speed | 15 / 165 |

The saved preset also includes inertia, road EQ and a force mapping. The angular
values above are the raw saved fields; they are not a new measured travel check.
Firmware version inside a preset is saved metadata, not current device readback.

Inventory: 344 Motor preset archives, 11 custom. Only Formula Legend and
R12-DiRT Rally 2.0-Custom have nonempty explicit game bindings among those custom
presets. The custom **Grid** archive has empty `games` and `gameId = -1` in this
snapshot, correcting the relay's three-bound-custom-presets list. Official
R12 **iRacing** presets also exist; their names do not establish recognition of
**iRacing Arcade**. Executable recognition rules were not inspected.

The files support using a common saved profile as a working assumption. They do
not prove the profile loaded into the base during either October 6 drive, nor
that no other device-side state differed. Record contemporaneous active profile
evidence on the next attended comparison without changing its settings.

MOZA describes game FFB intensity as a multiplier and maximum torque as a peak
limit; damping, inertia and EQ also affect response. A common preset can therefore
respond differently to differently shaped requests. Do not infer a universal
`40% x 75%` physical conversion or measured Nm from these settings.
Source: [MOZA force feedback settings guide](https://mozaracing.com/blogs/blog/moza-pit-house-force-feedback-settings-guide).

Private snapshot under `SessionEvidence/moza-saved-normalization-20261007`
contains original config/preset copies, decoded preset, inventory and receipt.
Original hashes were rechecked unchanged. No preset or owner paths are added
to this repository.

- Config SHA-256: `F2CDE9DE64607E046F1185CC67C2734FE858E9CBEE56D6012894D7FA9630D28D`.
- Selected preset SHA-256: `23D000F15F83FEFF27878BDA83599760DFC81514B762DEB1A67B3C962B45D041`.
- Snapshot checksum manifest SHA-256: `6974592AE268D367D88C4170C3C0D93D948DA524C500E682F8B262DD161DEA30`.

## Ordered request analysis

The previous report's RMS/percentiles could not distinguish rearrangements of
the same force values. The additive `temporal` report now measures signed and
absolute means, threshold occupancy, sustained same-sign holds, sign reversals
and total request variation per eligible second. See the exact definitions in
[FFB-NORMALIZATION](../docs/FFB-NORMALIZATION.md). All describe software requests.
There is no corner detector, frequency-response estimate or automatic gain rule.

Validation: 23 offline analyzer tests pass, including equal distributions with
different sequences, neutral noise, sampling duplicates, final-tail refusal,
speed-band re-entry and reset/gap/invalid continuity. Re-analysis of all three
sealed wrapper comparisons leaves **every historical report field unchanged**
after removing the new temporal field. Inputs and previous reports remain intact.
Windows PowerShell 5.1 wrapper execution with the matched frozen iRacing exporter
prints three labelled timing tables and produces a CSV byte-identical to the
earlier qualified export. No source-fingerprint bypass was used.

Evidence: `SessionEvidence/ffb-temporal-review-20261007` (new reports, source
copies and checks), and `ffb-temporal-wrapper-ps51-20261007` (exporter receipt,
frozen binaries, strict export and report). No runtime, native binary, tune or
installed package was changed by this work.
The first directory seals 15 evidence files; its `SHA256SUMS` hash is
`5CE569F094E4728EBE6D18D28A3FEB271123E6A9345F6088E3F2339656781503`.
Its frozen `verify.py` repeats both new-report equality and historical-field
equality checks with hash validation of the source manifests and artifacts.

The current iRacing diagnostic still has only 5.63 seconds at 20–60 km/h and no
60+ coverage. Its high short peak and small sustained request are not a matched
cornering workload and cannot explain the owner's earlier verdict by themselves.
DRIVE's automated model-3 original has the same workload limitation. Woden's
Classic reference is capped at 25%; it does not qualify the newer Grip model.

## Next attended comparison

Keep Art's pinned Strength-50 reference and the device profile fixed. Record the
exact installed build, selected model, full tune, car, assists and active device
settings with each capture. Drive representative sustained corners, release into
a slide, recovery, then separately labelled impacts. Preserve the original
telemetry and ordered contacts so force timing can be compared against those
events. Keep steering and crash comparisons separate: Art/iRacing/Woden steering
exports omit separate crash effects, while DRIVE's combined export retains
event-time crash strength. Strength 50 alone does not normalize those compositions.

Use the new timing tables to locate suspicious sustained force or rapid changes,
then inspect the corresponding original event. Do not pick a multiplier from
unmatched whole-drive histograms, infer physical torque from request magnitudes,
or attribute the older owner's verdict to a later build's behavior.
