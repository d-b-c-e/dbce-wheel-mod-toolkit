# Original native force producers: capture and replay

Provenance: [dbce-mods-outrun-2006, 2026-10-08], runtime `7852534`,
HUD-unit follow-up `ee41db9`; [dbce-mods-outrun-arcade, 2026-10-08],
runtime `4336bd9`, first-capture record `0569e87`.

These two games now preserve and reproduce their original force calculations
with delivery muted. Neither result establishes gameplay trajectory playback,
physical torque, or a gain that makes the game equal to Art at 50.

## Keep the original producer

Extract the production calculation into the fixture mechanically, with its
source hash and the production mapper identity. Do not write a second model
that happens to agree with a few expected outputs. Keep game-specific memory
reads and hooks in the consumer; share the envelope, validation principles and
delivery boundary.

Record every state value the producer reads or writes, its input values,
tuning, requests in order, and before/after state. Restore only the first
checkpoint during a continuous replay. Restoring every row would conceal a
missing accumulator, filter or timer. Poison other fixture members so an
unrecorded dependency fails instead of being silently supplied as zero.

CannonBall's original cabinet motor law consumes bounded integers and table
indices. The reader validates those domains *before* invoking the original
code, including steering [-127,127] and motor limits. A structurally valid JSON
number is not sufficient admission to a native lookup table.

## Observe muting, rather than writing a reassuring label

Capture admission must latch delivery suppression for the process, including
after malformed arming, interruption and failure. Calculation remains enabled.
The runner's environment and a metadata `muted=true` flag alone do not prove
that the runtime took the mute branch.

CannonBall records the actual `forcefeedback::set` return after each preserved
request. A complete update requires exactly one request followed by exactly
one return of -1 (the process mute refusal). Missing, early, duplicate or
accepted delivery makes the capture partial, with no complete footer. Fixtures
inject those failures. The wrapper calls the production setter once, not once
for the game and again for evidence. Native DLL absence during the live run is
additional evidence; neither observation measures actuator torque.

OutRun 2006 retains the original calculation with virtual sinks and the
existing feedback-state updates. Its original constant/periodic requests remain
separate from the downstream master strength and native encoding. Do not label
those sparse requests as post-strength wheel commands.

## A completed file has a stronger contract than a successful parse

Use exclusive arming/output identities, a bounded recording buffer and explicit
overflow/failure. Validate the lease during capture and before sealing. Do not
perform file I/O inside the producer interval. Stop/cleanup finalizes a partial
result before later fallible cleanup can bypass it.

CannonBall uses the separately pinned `dbce.wheel.session@1` writer. Both its
main CMake build and standalone tests hash-check the vendored source; its
`.gitattributes` pin preserves those checkout bytes without replacing existing
native binary attributes. Publication does not replace existing output. The
reader requires the complete footer, exact channel inventory, counts, source
identities, monotonic time, consecutive updates and state continuity. It bounds
file, line and row sizes and rejects duplicates and non-finite values.

The standalone CannonBall reader validates `source.jsonl`; it does **not**
read sibling `outcome.txt`. State that boundary explicitly. The runner retains
and checks the outcome separately. OutRun's binary codec has its own complete
checksum/bounds contract; an embedded revision is declared provenance, not a
cryptographic signature of the running module.

## Timing and units need independent evidence

Use a game-update identity to join observations. Wall-clock window captures
bracket what was rendered; they are not exact physics samples. Retain gaps,
state changes and unknown values rather than interpolating them into certainty.

OutRun's old telemetry assumed raw field 0x1C4 times 90 was metres/second.
An original moving run, the HUD draw path and three rendered speed intervals
instead identify field 0x1F8 as the HUD's base kilometre/hour value for the
tested Dino 246 GTS. The corrected encoder divides that value by 3.6. This
supports HUD-consistent telemetry for that case, not measured SI speed for
every car or the MPH display option.

A sparse command histogram is not time-weighted force duty. A nominal mapper
value is not a delivered force, and a spring coefficient is not a constant
force magnitude. Export only the domain actually observed, with component and
eligibility labels. Do not derive normalization gains from these diagnostic
drives or from artificial input that barely covers the speed bands.

## Original results and repeat commands

| Consumer | Original result | Remaining boundary |
|---|---|---|
| OutRun 2006 | Force capture `e66712a8e687450fa15013f9b48b8a7b`: 3,600 rows, exact producer replay. Paired schema-4 discovery `2620f9e8e9f24475bcd2412e3ceaf732`: 3,600 before/after pairs, HUD speed observed. Normal window close; nine owner files and payloads restored. | Sparse pre-master requests; raw force speed input still unqualified. No route replay or physical comparison. |
| CannonBall | `6e6d13fa8ca64ea898e19dcd481e6079`: 1,802 consecutive updates over 60.033117 s, every actual delivery return -1; exact original command/step/nominal and state replay. Normal exit 0; ten owner files and full inventory restored. | Cabinet model, not tyre force. Temporary candidate restored to prior install. No player recorder UI or route playback. |

CannonBall, from its repository root:

```powershell
cmake -S tests -B build-check/ffb-tests
cmake --build build-check/ffb-tests --config Release
ctest --test-dir build-check/ffb-tests -C Release --output-on-failure
py -3 tools/replay_cabinet.py '<capture>/source.jsonl' --replayer build-check/ffb-tests/Release/cabinet_replay.exe
```

The tests are a separate CMake project: rebuilding the game does not rebuild
them. Stale test executables initially rejected the newly required delivery
channel; rebuilding the correct project resolved that failure. Current checks:
31 CTest cases, 14 strict reader/replay cases and 146,347 producer assertions.

OutRun commands and exact evidence are in its
`docs/2026-10-08-hud-speed-discovery.md` and original-signal documentation. Preserve
the exact matching producer build when replaying. Its HUD correction has a
reviewed private package and passing installer fixtures but was not installed
at this checkpoint.

The private supervised runners preserve the owner's display and settings,
snapshot to disk before launch, inspect a fresh game frame before each bounded
offline input, and restore only after normal close. They are experiment
harnesses, not finished public setup scripts.
