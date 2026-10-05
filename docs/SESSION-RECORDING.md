# Diagnostic session recording (unreleased)

`Dbce.Wheel.Recording` is an **unpublished shared extension on the v0.12.0
baseline**. It is not present in the published v0.12.0 artifact. Source builds
and the next package include it. No existing native, FFB or telemetry behavior
changes. The library targets `netstandard2.0`, has no Unity/native references,
and adds no runtime NuGet dependencies. Its tests target .NET 8.

This is numeric diagnostic capture and a streaming replay reader, not video or
game-state restoration. Consumers own sampling, channel meaning, force-model
comparison, UI, and any replay command-line tool. Reading a file neither opens
devices nor sends forces or inputs. A recorded native API success is evidence
of acceptance, not evidence of physical force feel.

## Consumer API

All types are in `Dbce.Wheel.Recording`.

```csharp
var metadata = new SessionMetadata {
    Game = "iRacing Arcade",
    PluginVersion = "0.2.0",
    ToolkitVersion = "0.12.0+unreleased.recording",
    StartedUtc = DateTime.UtcNow,
    Properties = new Dictionary<string, string> {
        ["scenario"] = "offline Free Play",
        ["recordingSource"] = "source revision/hash from consumer manifest"
    },
    ChannelUnits = new Dictionary<string, string> {
        ["input.steer"] = "normalized [-1,1]",
        ["car.speed"] = "m/s",
        ["ffb.requested"] = "native magnitude"
    }
};
var recorder = new SessionRecorder(path, metadata, new RecordingOptions {
    QueueCapacity = 512,
    MaxDurationSeconds = 20 * 60,
    MaxFileBytes = 256L * 1024 * 1024,
    MaxChannelsPerSample = 1024
});
var clock = Stopwatch.StartNew();

// Call from the consumer's fixed-tick sampler. The library does not sample the game.
bool accepted = recorder.TryRecord(clock.Elapsed.TotalSeconds,
    new Dictionary<string, double> {
        ["input.steer"] = 0.25,
        ["car.speed"] = 12.5,
        ["ffb.requested"] = 1000
    });
recorder.TryMark(clock.Elapsed.TotalSeconds, "pause", "Paused by user");

// GUI: stop acceptance and wait no longer than 250 ms.
recorder.Stop();
// If Status == Stopping, retain the recorder and poll its status; the worker
// still owns the file. Do not offer it as complete or unload its runtime yet.
// Offline CLI/shutdown with a larger explicit budget:
bool finalized = recorder.Stop(TimeSpan.FromSeconds(5));
// false means the writer still runs; true includes a possible Faulted result.
```

`TryRecord(double, IDictionary<string,double>)` and
`TryMark(double, string, string)` return false when not accepted. All channel
values and elapsed times must be finite. Elapsed seconds must be nonnegative
and nondecreasing across accepted samples and markers; sample times must
strictly increase. Equal-time markers are allowed. Use one monotonic
`Stopwatch` across pause/resume and scene changes, not Unity's scaled game time.
Do not feed older sample timestamps after a newer marker. The consumer decides
whether to continue sampling while paused; the library's wall-clock duration
limit still applies.

Metadata and option values are copied during construction. Channel dictionaries
are copied before `TryRecord` returns, so subsequent caller mutation is safe.
Concurrent mutation *during* that call is unsupported. Names/keys must be
nonblank and at most 256 UTF-16 characters. Marker detail and metadata values
must be non-null and at most 4096 characters; use `""` for an empty detail.
Metadata dictionaries can each contain at most 1024 entries. `StartedUtc` must
be a nondefault `DateTime` with `Kind == Utc`; strings use ordinal key equality.
An empty channel sample is rejected. Undeclared channels are allowed:
`ChannelUnits` describes units, not a required channel whitelist.

### Status and stop contract

| Member | Type and meaning |
|---|---|
| `IsRecording` | `bool`: still accepting calls; does not mean pending bytes are durable |
| `Status` | `RecorderStatus`: `Recording`, `Stopping`, `Completed`, `LimitReached`, `Faulted` |
| `WrittenSamples`, `DroppedSamples` | `long`: live convenience counters |
| `Error`, `LastError` | `string`, aliases, null unless the worker reports failure |
| `StopReason` | `string`: `recording`, `stopped`, `durationLimit`, `fileSizeLimit`, `recordSizeLimit`, `error` |
| `GetCounters()` | Detached `RecordingCounters` snapshot |
| `Stop()` / `Dispose()` | Stop acceptance immediately; wait at most 250 ms; safe to repeat |
| `Stop(TimeSpan timeout)` | Returns `bool`: whether finalization ended within the nonnegative, finite wait budget |

A producer never waits for disk, queue space, or the producer gate. Concurrent
producers contend using `Monitor.TryEnter` and report a drop rather than wait.
Producer work still includes a bounded dictionary copy/allocation; this is not
a hard realtime or allocation-free API. A slow or defective caller-provided
dictionary can delay that caller; it cannot force the GUI's `Stop` to wait
indefinitely. Prefer one producer thread for stable tick/marker ordering.

The dedicated background writer drains accepted records on normal/duration
stop. A byte/record-size limit discards the remaining accepted suffix, counting
every discarded sample and marker. On timeout the caller does not dispose the
stream out from under the worker. A permanently blocked OS write can leave the
worker in `Stopping`; the library cannot guarantee both bounded shutdown and
durable flush on a hung disk. Process exit can interrupt that worker.

Construction creates a new file and refuses overwrite. Create its parent
directory first. Invalid metadata/options and synchronous file-open failures
throw before capture starts. Background serialization/I/O/flush/close failures
are exposed through `Faulted`, `Error`, and `ErrorCount`; producer calls do not
throw those disk errors. Dispose of an abandoned recorder to request stop.

### Bounds and counters

| `RecordingOptions` | Default | Constraint |
|---|---:|---|
| `QueueCapacity` | 2048 | 1–65536 queued records, plus one writer record |
| `MaxDurationSeconds` | 1800 | Finite and positive; caller elapsed and wall clock are both bounded |
| `MaxFileBytes` | 268435456 | At least 16384; includes every byte, including footer/newlines |
| `MaxRecordBytes` | 262144 | 1024–1048576 serialized UTF-8 bytes per header/sample/marker |
| `MaxChannelsPerSample` | 256 | 1–1024 |

The file cap reserves 8192 bytes for its footer, so useful data may stop before
the cap. Size enforcement runs on the writer: `TryRecord == true` means queued,
not necessarily written. A record too large for `MaxRecordBytes` terminates
capture with `recordSizeLimit`. Oversized metadata throws at construction.
Wall-clock expiry is checked between writes and every 100 ms while idle;
elapsed sample/marker timestamps above the limit are rejected immediately.

`RecordingCounters` contains `long` properties:

- `AcceptedSamples`, `AcceptedMarkers`: successfully queued records.
- `WrittenSamples`, `WrittenMarkers`: complete data lines written to the stream.
- `DroppedSamples`, `DroppedMarkers`: rejected calls during acceptance plus
  queued records discarded on a limit or writer failure. Pending records are
  not counted as dropped.
- `QueueFullCount`, `ContentionCount`, `InvalidCount`: rejected calls by reason.
- `LimitCount`: zero normally, one when a configured limit ends capture.
- `ErrorCount`: worker serialization/I/O/flush/close errors.
- `RejectedAfterStop`: calls after acceptance closed, separate from drops.
- `DataBytesWritten`: UTF-8 bytes through the final data line, including the
  header and line endings, **excluding the footer**.

Live snapshots can span adjacent updates; final counters are stable except
`RejectedAfterStop`, which can increase if a consumer keeps calling. The footer
freezes counters at finalization. A normal stop drains all accepted samples
and markers. Drops still require review even with a completed footer.

## JSONL schema and streaming replay

Each UTF-8 line is one object terminated by LF (the reader also accepts CRLF).
There is no BOM. `DataContractJsonSerializer` uses ordinary JSON object
dictionaries, invariant numeric formatting and ISO UTC timestamps with seven
fractional digits. Strings are escaped, including embedded newlines. The file
contains exactly one metadata header, data records, then one footer:

```json
{"kind":"metadata","schema":"dbce.wheel.session","version":1,"metadata":{"game":"fixture","pluginVersion":"test","toolkitVersion":"0.12.0+unreleased.recording","startedUtc":"2026-09-12T18:00:00.0000000Z","properties":{},"channelUnits":{"steer":"normalized"}}}
{"kind":"sample","sample":{"sequence":0,"elapsedSeconds":0.02,"channels":{"steer":0.25}}}
{"kind":"marker","marker":{"elapsedSeconds":0.02,"name":"pause","detail":""}}
```

This excerpt intentionally omits the footer; by itself it is an incomplete
recording. A footer has `kind: "footer"` and a `footer` payload with
`elapsedSeconds`, `completed`, `stopReason`, and `counts` (the counters above,
using camelCase JSON keys). Its elapsed time is that of the last written data
record, or zero for an empty session. `completed: true` with `stopReason:
"stopped"` means orderly explicit stop. A valid limit footer has `completed:
false` and the applicable limit reason. Missing or partial footers do not
establish completion. A write failure normally leaves no footer; a later
flush/close error can happen after valid footer bytes were written, so the
consumer should also inspect recorder status before reporting success.

```csharp
foreach (SessionRecord record in SessionReader.Read(path)) {
    switch (record.Kind) {
        case SessionRecordKind.Metadata:
            SessionMetadata metadata = record.Metadata;
            break;
        case SessionRecordKind.Sample:
            SessionSample sample = record.Sample;
            // sample.Sequence (long), ElapsedSeconds (double),
            // Channels (Dictionary<string,double>) feed consumer-owned replay.
            break;
        case SessionRecordKind.Marker:
            SessionMarker marker = record.Marker; // ElapsedSeconds, Name, Detail
            break;
        case SessionRecordKind.Footer:
            SessionFooter footer = record.Footer; // Completed, StopReason, Counts
            break;
    }
}
```

`SessionRecordKind` values are exactly `Metadata`, `Sample`, `Marker`, `Footer`;
their wire strings are lowercase. `SessionRecord` exposes nullable payload
properties named `Metadata`, `Sample`, `Marker`, `Footer`, plus header `Schema`
and `Version`. `CurrentSchema` and `CurrentVersion` are public constants.
Samples have contiguous zero-based sequences across *accepted* samples; rejected
ticks do not consume a sequence. Dropped ticks are exposed by counters and
elapsed-time gaps, not inferred from a missing sequence. Size-limit discards
are always a suffix, so written sequences remain contiguous.

The reader buffers at most one line (1 MiB cap), validates complete JSON syntax
and UTF-8, required DTO fields, schema/version, record/payload matching,
finite values, ordering, footer counts and footer final position. It rejects
negative counters and counts inconsistent with drops/rejections. It does not
recover or silently skip corrupt lines. Extra fields are ignored for additive
annotations, but unknown kinds and schema versions are rejected. The format
does not contain cryptographic integrity checks; structurally valid edited
channel values cannot be detected by the reader.

Fully enumerate before declaring a recording valid. A prefix enumeration is
useful for inspection but says nothing about the remainder. The footer is held
until EOF is checked, so data after it prevents any footer being yielded.
Malformed or interrupted files throw `SessionReadException : IOException` with
`LineNumber` and `Error` (`SessionReadError`): `InvalidJson`, `InvalidSchema`,
`UnsupportedVersion`, `InvalidRecord`, `InvalidOrder`, `InvalidFooter`,
`MissingFooter`, `TruncatedLine`, `RecordTooLarge`. Filesystem access errors
remain normal I/O exceptions. A final line without a newline is treated as
truncated even if its JSON happens to parse. A crash can lose a buffered tail;
the missing/footer checks expose that interruption.

## Build, integration and verification

```powershell
dotnet test Dbce.Wheel.sln -c Release
.\tools\tests\Test-SyncToolkit.ps1
.\dotnet\Dbce.Wheel.Recording.Tests\Test-RecordingPackaging.ps1
# Optional: use a cached real v0.12.0 archive in place of the offline release fixture.
.\dotnet\Dbce.Wheel.Recording.Tests\Test-RecordingPackaging.ps1 -ReleaseArchive .\dist\dbce-wheel-mod-toolkit-0.12.0.zip
```

The release packager now copies the Recording DLL/XML explicitly and runs the
Recording packaging test after the managed build. Local `Sync-Toolkit.ps1
-Parts dotnet` includes the Recording project's `bin/Release/netstandard2.0`
output as well as existing FFB/Telemetry outputs. Build the full Release
solution first. Local sync retains its existing `local+<revision>.dirty/clean`
provenance, never a release tag. Downloading the published v0.12.0 package
continues to produce only the assemblies actually in that release.

A consumer can instead keep released v0.12.0 under `lib/toolkit` and vendor
this DLL separately under `lib/recording`, with its own source hashes and base
revision manifest. This preserves the published pin while evaluating the
extension. Do not present the extra DLL as part of the v0.12.0 release.

Offline tests cover roundtrip metadata/units/events, culture, mutation
snapshots, nonfinite and invalid inputs, time/sequence order, corrupt and
interrupted JSONL, bounded slow-disk queues, bounded stop with a stalled caller,
drain/repeated stop/concurrency, wall-clock and byte/record limits, and injected
disk failure. The packaging test copies and hashes real built artifacts through
both package copy statements and local sync in temporary directories. None of
these tests load a game or touch a device. Unity Mono and live IL2CPP recording
remain consumer validation work.

Verified 2026-09-12: 170 managed tests passed (49 Recording, 80 FFB, 41
Telemetry), 24 existing transactional sync assertions passed, and all 19 new
package/local-sync/release-compatibility assertions passed with both the
fixture and cached published v0.12.0 archive. Package verification executed the
actual managed copy statements; it did not run the full native release builder.
