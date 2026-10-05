# Recorded playback and force observation v1

For offline summaries with explicit cross-game compatibility boundaries, see
[command-summary comparison](COMMAND-SUMMARY-COMPARISON.md). This does not relax
the same-case identity requirement below.

The toolkit owns portable case identity, bounded force-observation files and
comparison. Consumers own game sampling, input codecs, initial-state restoration,
force signals/models and game execution. MAME INP stays MAME INP; F-Zero's `.fzpt`
and machine-state sidecar stay with F-Zero. Woden's existing numeric session
recording remains separately pinned and is suitable for **signal reprocessing**;
it does not establish deterministic game-input replay.

This contribution is source-only, independent of the native DLL and unpublished
Recording extension. It has no device, input-injection, process-launch or network
API. Offline playback always uses `output="observe"`, `physicalOutput=false`.
Hardware recording/acceptance remains a separate attended consumer workflow.

## Portable contract

`tools/replay/replay_case.py` is the reference Python 3.10+ implementation, using
only the standard library. Native/managed adapters may emit the same JSON files.
All objects reject duplicate/unknown keys, nonfinite numbers and unsupported
versions. Files use UTF-8, LF and invariant JSON numbers. Hashes are lowercase
SHA-256. Paths are relative to the case directory, never absolute or escaping
through `..` or symlinks. Source artifacts are retained unchanged, not embedded
in Git or copied into the toolkit. Do not ship ROMs or machine snapshots.

Case manifest (`case.json`):

```json
{
  "schema":"dbce.wheel.replay-case", "version":1,
  "caseId":"example-drive", "game":"example", "adapter":"example@1",
  "capability":"signal-reprocess",
  "clock":{"domain":"monotonic", "ticksPerSecond":10000000},
  "source":{"path":"drive.jsonl", "sha256":"<64 lowercase hex>", "format":"dbce.wheel.session@1"},
  "initialState":null,
  "artifacts":{},
  "provenance":{"sourceRevision":"<revision>", "dirty":false, "executableSha256":"<64 lowercase hex>"}
}
```

`capability` is `signal-reprocess` or `game-input-replay`; the latter requires
an `initialState` artifact with the same path/hash/format shape as `source`.
`artifacts` maps names (e.g. config, profile, checkpoints, cartridge-identity)
to that shape. Hashing artifacts verifies identity, not their game-specific
contents, licensing, replay determinism or physical force. The manifest contains
no commands; validation never executes a game or a supplied script.
`clock.domain` is `emulated` or `monotonic`; `ticksPerSecond` is a positive integer.
Use integer emulated clock units for noninteger frame rates; `frame` is a separate
optional index. Woden uses captured monotonic elapsed time and actual model dt.

Force observation JSONL begins with this exact header:

```json
{"kind":"header","schema":"dbce.wheel.force-observation","version":1,"caseId":"example-drive","caseSha256":"<SHA-256 of exact case.json bytes>","clock":{"domain":"monotonic","ticksPerSecond":10000000},"model":"consumer-model@1","configSha256":"<64 lowercase hex>","profileSha256":"<64 lowercase hex>","output":"observe","physicalOutput":false}
```

Every request has the exact shape below. `sequence` is contiguous from zero;
ticks are nondecreasing and nonnegative; frame is null or a nonnegative index.
Requests on the same tick preserve their sequence order. `effect` is a stable
consumer slot name (e.g. `steering`, `spring`, `road`, `impact-left`).

```json
{"kind":"force","sequence":0,"tick":1234,"frame":42,"effect":"steering","family":"constant","operation":"set","magnitude":-0.25,"frequencyHz":0.0,"durationMs":null}
```

Families: `constant`, `spring`, `damper`, `friction`, `sine`, `rumble`, `all`.
Operations: `set`, `start`, `stop`, `stop_all`. `all` is only for `stop_all`;
stop operations require zero magnitude/frequency and null duration. Magnitude
is a normalized signed request in [-1,1], **not measured torque**. Adapters must
document normalization/sign; condition strength may use [0,1]. Frequency is Hz
in [0,10000]. Duration is null for a persistent request, or finite milliseconds
in (0,60000]. Include actual event edges; never synthesize repeated impact starts
from a held event. Source units/validity, raw signals and model inputs belong in
the original capture and its adapter mapping; missing data must not become zero.

The stream ends with `{"kind":"footer","complete":true,"count":N}` and a
newline. Truncation, data after footer, count mismatch or any dropped source
samples invalidate exact-reproduction claims. Adapters must report source drops
and refuse unsupported/incomplete captures before generating this completed
stream. A valid observation file proves only the observations are well formed.

## APIs and commands

`validate_case(path)` checks schema and all artifact hashes and returns identity
and capability. `ObservationSink(path, header)` exclusively creates a bounded
file; `emit(request)` snapshots and validates each request; `close()` writes the
footer. Use only from an offline model runner or bounded background writer,
never on a real-time game callback. Exceptions leave an incomplete file; never
label such a file complete. There is intentionally no hardware sink selector.

`caseSha256` binds the stream to the exact manifest bytes, including source and
initial-state identities. Validate that manifest separately before model replay.
Different trial tunes go in the observation header; preserve the original case.

`read_observations(path)` fully validates a bounded stream. `compare(a,b,
tolerance=...)` compares matching case/clock/timeline/event identities, counts
and numeric fields, reporting differences and magnitude error. Changed model,
profile/config identities are reported and allowed for tuning. It cannot score
physical feel, and equality does not prove a game re-executed deterministically.

```powershell
python tools/replay/replay_case.py validate path/to/case.json
python tools/replay/replay_case.py compare baseline.jsonl candidate.jsonl --tolerance 0.000001
python -m unittest discover -s tools/replay -p "test_*.py"
```

Exit 0 means valid/equal, 1 means comparison differs, 2 means invalid input.
Reports go to stdout; redirect to a **new** private evidence file if desired.
Numeric tolerance applies in each field's stated unit; event/tick/frame identity
always matches exactly. Magnitude metrics include only structurally aligned
requests. Readers are streaming and must be fully enumerated; bounds are 16 KiB
per line, 256 MiB per stream, 2,000,000 requests and 64 KiB per case manifest.

## Consumer handoff

- F-Zero: reference `.fzpt` and `.fzpt.state`; retain cartridge hash, exact
  game-visible SNES words and per-frame WRAM hash checks in its adapter. Emit
  pure `FzeroFfbCompute` requests without native wheel initialization.
- Cruis'n: wrap existing INP and retained initial configuration as artifacts;
  continue using its actual worker observation and MAME replay harness. This
  contribution does not migrate or certify that adapter automatically.
- Woden: first capture complete raw game/model inputs, dt, lifecycle gates,
  effective configuration/profile identity and resulting requests. Its offline
  adapter reruns the actual pure force model with an observation sink. Confirm
  source drop counters, channel availability and reset events before comparison.
  Unity game-state restoration and deterministic input replay remain unproven.

The owner requested Woden ready for their next attended capture. Preserve saved
bindings/FFB settings; no unattended game launch, input, device acquisition or
force. An expiring next-launch request must be created when the owner actually
starts, not hours in advance. Provide a durable launcher/command and saved-path
instructions, then independently distinguish build/deploy readiness from the
first actual driving recording and later physical acceptance.

[cruisn-collection, fzero-triple-wheel-integration, woden-rally-edge-wheel;
coordinator source review, 2026-09-28]

Initial offline verification: 16 tests passed; one symlink-creation test skipped
because this Windows account lacked symlink privilege. Cases use synthetic bytes,
no ROMs, game processes or devices. Consumer adoption and actual driving data
are separate checkpoints in [the coordination record](research/2026-09-28-recorded-playback-coordination.md).
