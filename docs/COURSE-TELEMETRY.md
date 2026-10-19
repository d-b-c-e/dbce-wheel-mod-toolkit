# Course telemetry: course id, progress and a SimHub map feed

2026-10-18, zola (kobe 275148/275734). Prototype, source only. Nothing here has run in a game or talked to SimHub;
see [What is not proven](#what-is-not-proven).

## Why

SimHub draws a course map and a live car marker from four standard values: a world position, a 0-1 progression
along the course, a stable track id and the track length. Our games send Forza "Data Out". SimHub's Forza
Horizon readers give no track id, progression -1 and length 0, so SimHub never records a map from our feeds. The
Forza packet has fixed sizes that SimHub and MOZA Pit House both depend on, so it cannot carry these values.

So the course data is a separate seam beside `TelemetryFrame`. `ForzaPacket` and `TelemetryFrame` are unchanged.
Background and SimHub internals: hub report `docs/reviews/2026-10-18-course-map-telemetry-feasibility-zola.md`.

## Types (`dotnet/Dbce.Wheel.Telemetry`, netstandard2.0)

### October 19: game-specific raw import (Kobe287774)

`CourseRouteFile.Read(folder)` now recognizes these completed raw producers
through the existing offline `Dbce.Wheel.Course` reader:

| Schema | Producer source | Native progress columns |
|---|---|---|
| `dbce.art-course-route@1` | Art `3f31b6f`, `CourseRouteCapture.cs` | coarse `waypoint_index`, native `status` |
| `dbce.drive-course-route@1` | DRIVE `d91c394`, `CourseRouteCapture.cs` | `progress_raw`, nullable `offset_x/y/z`, native race flags |
| `dbce.iracing-course-route@1` | iRacing `8763420` (capture `08357c0`), `CourseRouteCapture.cs` | nullable `raw_time`, `raw_clamped_time`, `participant_index` |

All use `route.json`, `progress.tsv` and the existing `dbce-playback-seal-v1`
seal. The same reader checks sizes before verification, rejects a seal over
4 KiB or one naming anything except those two payloads, calls Playback's
`ArtifactSeal.Verify`, then checks JSON/schema and every TSV row. The 8 MiB total
includes the seal; caps remain 20,000 points and 36,000 rows. Duplicate JSON keys
at any depth, malformed JSON/UTF-8, unknown schema versions, mismatched game/frame,
non-finite or malformed numbers, wrong columns, partial nullable tuples,
backward time/ticks, count mismatches and unwritten states are refused. Seal
hashes bind exact bytes: same-size tampering is refused as well as truncation.

For raw schemas, `file.Raw` is a `RawCourseRoute`. Its `Waypoints` and each row's
`Position` are **native X/Y/Z**, without compass mapping, scaling, list reversal,
clamping, projection or floating-origin correction. `Progress[i].Fields` preserves
the schema's extra columns as immutable strings, including empty values and raw
progress drops. `Metadata` is a cloned JSON element containing all original
known and unknown fields, nulls, units/direction qualification text, reported
distances, origin offsets and native identity fields. Art's missing `courseSample`
and iRacing's missing `capturedUtc` remain absent. Producer `pathHash` is retained
as a producer claim (hex shape checked); it is not reinterpreted as a qualified
course identity. The playback seal verifies the actual metadata/geometry bytes.

```csharp
var file = CourseRouteFile.Read(folder); // read-only, sealed and schema checked
if (file.Raw is { } raw) {
    var nativeXYZ = raw.Waypoints[0];
    var qualification = raw.Metadata.GetProperty("directionStatus").GetString();
    // Inspect raw evidence; this does not supply a qualified CourseSample.
}
```

The existing Woden `Waypoints`, `Progress`, distance/direction/track fields,
`CapturedUtc`, `ToPath()` and geometric comparison methods retain their original behavior
for Woden. They throw for raw imports, directing the caller to `Raw`, so unknown
raw values cannot become fabricated zeros, meters, forward directions or mapped
coordinates. `SchemaId` identifies the admitted schema; `Raw == null` identifies
the original Woden path. Common game/request/build/state/reason fields stay
available on the parent result for all schemas.

This is one reader entry point and one seal-validation path. There is no new
writer, runtime dependency, UDP/SimHub output or consumer binary repin. Actual
game captures, physical units, route direction/origin coherence and conversion
to `CourseSample` remain separate gates. Synthetic raw fixtures and the affected
existing Woden read/comparison fixtures run with:

```powershell
dotnet test dotnet/Dbce.Wheel.Telemetry.Tests/Dbce.Wheel.Telemetry.Tests.csproj -c Release --filter 'FullyQualifiedName~RawCourseFilesTests|FullyQualifiedName~CourseFilesTests' -m:1 -nr:false -p:UseSharedCompilation=false
```

Source qualification: all 43 affected tests passed (14 raw-schema cases and 29
existing Woden reader/comparison cases), zero failures/skips. Final original log,
TRX and source hashes are recorded in hub
`.local/course-raw-readers-samo-20261019/result.json`. No real captures were read,
no files were changed by imports, and no SimHub or game ran. This successor is
based on the existing private course prototype `90f14a0`; it does not independently
integrate that prototype into canonical toolkit main or repin any consumer.

### Original prototype types

| Type | Role |
|---|---|
| `CourseVector`, `CourseAxes` | Metres on north/east/up axes. Unity: north = +Z, east = +X, up = +Y, yaw = `eulerAngles.y`. This is the only mapping under which Unity's yaw already matches SimHub's "0 = north, clockwise, 90 = east". `ToSimHubCarCoordinates` shows the `[-north, up, east]` array SimHub builds |
| `CourseIdentity` | `dbce.<game>.<venue>.<route>.<fwd|rev>[.<hash16>]`. Tokens are `[a-z0-9-]`, 1-40 characters. The id is at most 127 bytes (SimHub `EnvironmentId` is a zero-terminated UTF8[128]). `HashPath` is FNV-1a 64 over the exact float32 route points, for custom or generated courses. `Slug` turns stable internal names into tokens |
| `CoursePath` | The game's own route as a polyline in driving order, as a loop or point to point. Length and projection are 3D, so a bridge is not confused with the road under it. It refuses fewer than 2 points (3 for a loop), non-finite points, and anything under 10 m (a wrong unit). `Reversed()` gives the other direction |
| `CourseTracker` | Turns positions into progress, laps and completion; the rules are below |
| `CourseSample` | One snapshot: id, name, length, loop flag, progress, laps, finished, position, yaw, lap time, session id, discontinuity count, spectator/replay/control flags. `TryValidate` checks the invariants |
| `SimHubCourseFeed` | Draft "dbce course + dash v1" external-sim layout, with an encoder and a decoder. No socket, no SimHub files |
| `Dbce.Wheel.Course` (net8.0, offline only) | `CourseRouteFile.Read` reads a `dbce.course-route@1` capture folder without changing it. It checks sizes before hashing or parsing (8 MB), then the seal through Playback's `ArtifactSeal`, then the schema: exact `schema`/`frame`, `state` Completed or LimitReached, sha256/request-id/UTC formats, 2-20,000 `[x, y, z]` float waypoints matching `waypointCount`, at most 20,001 distances, no duplicate properties, and every `progress.tsv` row (8 columns, finite numbers, time never going back, row count matching `rows`). Points map once through `CourseAxes.FromUnity`. `CourseCaptureCheck` judges a capture's own drive (game progress included) and a recorded trajectory read by Playback's existing `TrajectoryTape` (`dbce.trajectory@1`), so no second trajectory reader exists |
| `CourseComparison` | Offline judge for a route capture. It runs the tracker over a trajectory on the game's route and reports: fix, discontinuities, off-course and invalid counts, unflagged backward steps, p95/max offset, course distance against the plan path, and the game-progress scale and deltas. The game's unit is estimated, not assumed. Thresholds belong to the caller |

### Identity rules

- Ids never come from localized display names. They never come from the length alone either: SimHub's DiRT Rally
  2.0 reader keys maps by length, so equal-length stages share a map.
- SimHub uses the id as a file name. It splits on invalid file-name characters (`WoteverCommon.FileOP.CleanFileName`),
  so `:` or spaces would merge distinct ids. Hence `.` as the separator and `[a-z0-9-]` tokens.
- Direction is part of the id. User-made or generated courses add `HashPath` of the authored points, taken before
  any floating-origin shift.

### Tracker rules (each has a test)

| Situation | Behaviour |
|---|---|
| First sample | Global projection. Beyond `MaxOffsetM` (30 m) there is no fix yet (`NoFix | OffCourse`, progress 0) |
| Normal driving | The search stays within `SearchWindowM` (150 m) of the last position, so a crossing or hairpin cannot snap progress across |
| Point to point | Progress is below 1 until finished. It is exactly 1 once the car arrives forwards within `FinishToleranceM` (3 m) of the end, and it holds there until `Restart()`. SimHub closes a non-looping map only on exactly 1 |
| Loop | Progress wraps to 0 at the line and is never 1. A lap counts only when driving enters a lap the car has never been in |
| Grid behind the line | Crossing the line from the grid starts lap 0; it does not complete a lap |
| Reversing over the line, then forwards again | No second lap. `WrongWay` after 20 m of continuous backwards travel |
| Reset or teleport (a move over `MaxStepM`, 60 m, or a re-find outside the window) | `Discontinuity` and the count goes up (it feeds SimHub's `PhysicsDiscontinuityCounter`). A jump never counts a lap, including a forward jump over the line or a reset back behind it |
| Off course (over 30 m from the line) | `OffCourse`; progress is held at the last good value |
| NaN or infinite position | `InvalidInput`; nothing changes |
| `Restart()` | Clears fix, laps and finish; one discontinuity |

## SimHub definition draft v1

SimHub 9.11.5 added external-sim definitions. The layout below was read from installed SimHub 9.13.2
(`SimHub.Plugins.dll` sha256 4e2ef85c…, metadata and decompiled code, kept privately).

- **Packet.** The packed 55-byte `TelemetryHeader`, then each declared field in order, little-endian. SimHub drops
  a packet whose length or signatures do not match (`ExternalSimTelemetryParser.GetStatus`).
- **Field order.** SimHub's editor sorts standard fields by `StandardFieldType` number
  (`TelemetryDefinition.NormalizeOrder`). Adding a standard field later therefore inserts it mid-packet, which is a
  major layout change. So v1 declares the dash fields now, beside the course fields. When SimHub picks this
  definition for a game's process, it replaces that game's Forza-reader identity in SimHub, and the dashboard must
  keep working from this feed.
- **Signatures.** SimHub's editor computes them, and the emitter copies them in. `TryWrite` writes whatever the
  header holds.

| # | Field | Wire | Value from |
|---|---|---|---|
| 0 | YawDegrees | float32 | `CourseSample.YawDeg` |
| 12-14 | VehiclePositionNorth / East / Up | double | `CourseSample.Position` |
| 16 | SpeedKmh | float32 | `TelemetryFrame.Speed` × 3.6 |
| 18 | VehicleName | UTF8[256] | `SimHubDashText.VehicleName` (may be empty) |
| 19 | VehicleId | UTF8[128] | `SimHubDashText.VehicleId` (may be empty) |
| 20 | EnvironmentName | UTF8[256] | `CourseSample.CourseName` |
| 21 | EnvironmentId | UTF8[128] | `CourseSample.CourseId` |
| 25, 26 | EngineRpm, EngineMaxRpm | float32 | `TelemetryFrame` |
| 31-34 | Throttle, Brake, Clutch, Handbrake | float32 0-1 | `TelemetryFrame` bytes / 255 |
| 37 | TrackLength | double | `CourseSample.LengthM` |
| 38 | Gear | UTF8[8] | `SimHubCourseFeed.GearText` of the game's signed gear. `TelemetryFrame.Gear` cannot tell reverse from neutral |
| 46 | TrackProgression | float32 | `CourseSample.Progress01` |
| 49 | CompletedLaps | uint32 | `CourseSample.CompletedLaps` |
| 52 | CurrentLapTime | double | `CourseSample.LapTimeS` |
| 53, 54 | LastLapTimeSeconds, BestLapTimeSeconds | double | `TelemetryFrame` |

That is 872 bytes of fields, 927 bytes per packet with the header. `TryWrite` takes the header's `SessionId`,
`IsReplay`, `IsUserInControl`, `IsSpectator` and `PhysicsDiscontinuityCounter` from the course sample, so the two
cannot disagree.

## Game adapters: route data confirmed in the shipped code

Read from the installed assemblies without running them. The Mono games (art, DRIVE) were decompiled. The IL2CPP
games (iRacing Arcade, Woden) were read through their BepInEx interop stubs, which give member names and types
but no logic.

| Game | Course and direction | Route points | Game's own progress | Proposed id |
|---|---|---|---|---|
| art of rally (Mono, Assembly-CSharp 7807A1D6…) | `GameModeManager.GetRallyDataCurrentGameMode().GetCurrentStage()` gives a `Stage` with `Area`, `Name`, `SceneConstant` (from `Location`), `IsForwardStage` and `Distance` in **km** (display formats it as km or ×0.62 miles) | `OutOfBoundsManager.GetWaypointList()` (`List<Vector3>`). Built from children of "Waypoints Parent" named `*Waypoint*` (the first child is skipped on official areas), and already reversed for reverse stages. The stage runs from the waypoint closest to the car at start to the waypoint closest to the opposite spawn (`SpawnPositionForward`/`Reverse`) minus 2. Those indices are private, so the adapter recomputes them | HUD slider `(Current - Start) / (Last - Start)` waypoint index; coarse | `art.<area>.<scene-or-name>.<dir>`; area `CUSTOM` adds `HashPath` |
| DRIVE Rally (Mono, 880DDDF1…) | `MonoBehaviourSingleton<GameplayManager>.Get.trackPathController.currentTrackPath`, a `TrackPathDefinition` with `id` (string), `distance` (float), `isReversed` and `nodes` (GeNa spline node ids) | `TrackPathController.WayPoints` (`List<WayPoint>` with `position`), spawned along the path at `waypointDistance` = 5 m | `CheckpointsManager.progress` (m, only increases past waypoints) / `distance`, sent to the HUD | `drive.<biome-or-scene>.<id-slug>.<dir>`. Random race passes a definition chosen elsewhere; a null falls back to `debugTrackPath`. Free roam has no course |
| iRacing Arcade (IL2CPP) | `DynamicTrackData.Initialize(TrackDescriptor)`. The descriptor has `trackDisplayName`, `pathDisplayName` and `isOval`; use the asset name, not display text | `DynamicTrackData.ProgressSpline` (static `LinearSpline2D`) and `IsPathCyclic`. `TrackProgressPath.progressPath` is `Transform[]`, giving 3D positions | `RacerProgressManager.Instance.timesOnProgressPath` (`SyncList<TimeAndClampedTime>`, per participant) and `ServerFindTotalDistanceMetres(ParticipantId)`. `trackProgress` is only a bool switch | `iracing-arcade.<track>.<path>.<dir>` |
| Super Woden Rally Edge (IL2CPP) | Already captured by the mod: `round.Track.name` (trackAsset), `CircuitName`, `RaceConditions.ReverseTrack` | `StageProgress.MyCircuit` gives a `WaypointCircuit` with `waypointList.items` (`Transform[]`), `isClosedCircuit`, `distances` (`float[]`) and `length` | `WaypointProgressTracker.progressDistance` (float); `StageProgress.PlayerPercentage` (float; whether 0-1 or 0-100 is not read yet) | `woden.<track-asset>.<stage-or-circuit>.<dir>` |

The adapters themselves are not written: each belongs in its game repo. Runtime values (units, whether each
singleton exists during a stage, floating-origin offsets) are unread until a game runs.

## Tests

`dotnet/Dbce.Wheel.Telemetry.Tests`:
- `CourseTests.cs`: identity, path, tracker, axes and sample.
- `SimHubCourseFeedTests.cs`: the golden SimHub 9.13.2 field table, header offsets, round trip and refusals.
- `SyntheticCourses.cs`: the generated stage, circuit and figure-eight drives.

No owner capture is committed or needed. Every expected value follows from the generated geometry.

`CourseFilesTests.cs` builds synthetic captures in Woden's exact format. It covers axis mapping, read-only access,
own-drive unit recovery and trajectory judging, plus refusal of 14 malformed route fields, 4 kinds of unclean JSON,
6 kinds of malformed progress rows, and changed, unsealed, missing or oversized captures. `CourseCaptureEvidenceTests`
applies the handoff's pass thresholds to an actual capture folder named by `DBCE_COURSE_CAPTURE`, and, if
`DBCE_COURSE_REFERENCE` names one, to a recorded trajectory of the same course. Otherwise it is skipped.

`PrivateEvidenceTests.cs` runs only when `DBCE_COURSE_PRIVATE` names a folder of private copies of the owner's SimHub
map cache, and is skipped otherwise. It is pinned by sha256: one EA WRC stage and three PCars3 circuits, each a lap
SimHub recorded plus SimHub's cloud consensus map. The tracker follows the real lap along the consensus map, and its
progress must match SimHub's stored progress: median under 5 m, p95 under 15 m, no discontinuities.

## What is not proven

- Nothing has been sent to SimHub. The draft must first be compared with the code SimHub's own editor generates
  for the same definition (a granted step that opens SimHub).
- Map orientation on screen (mirrored or rotated) is unknown until SimHub draws one.
- No adapter exists, and no route has been read from a running game.
- Signatures, registration (`%LOCALAPPDATA%\SimHub\ExternalSims`) and SimHub's online-map upload are owner
  decisions, listed in the hub report.

## Next: Woden route extraction handoff (reserved)

**Status (kobe 277046):** reserved behind Fleet completion and game-release work. There is no launch grant. This is
not an Art or DRIVE release gate, and nothing here registers anything with SimHub. The prototype above is qualified
offline only; it is not integrated and draws no live map.

**What it is.** A read-only dump in the Woden repo: `CourseRouteDump` in the plugin and `CourseRouteCapture` in
Core. It is implemented on Woden branch `zola/woden-course-route-20261019`; see that repo's
`docs/COURSE-ROUTE-CAPTURE.md`.
- **Off by default.** Arming takes a one-launch request file, `BepInEx\config\woden-course-route-next-launch.json`,
  which is claimed before reading like `DiagnosticLaunch`.
- **Hook.** It runs as a postfix beside `GameSampler` on `MainCar.FixedUpdate`, so it never skips or changes game
  code. No UDP, no SimHub, no settings, no game-state writes.
- **Dependencies.** It needs only Unity and the interop; the toolkit Course types are used offline afterwards.

### Exact inputs

| Value | Source (interop, Woden build `f422894d…`) | Type |
|---|---|---|
| Car | `Runtime.Local` (`MainCar`), as `GameSampler` uses it | |
| Progress tracker | `car.field_Private_WaypointProgressTracker_0`. The name is obfuscated, so check it again after any interop regeneration | `WaypointProgressTracker` |
| Route | `tracker.circuit`. Also record whether `Object.FindObjectOfType<StageProgress>().MyCircuit` is the same instance | `WaypointCircuit` |
| Waypoints | `circuit.waypointList.items[i].position`, in order | `Transform[]`, giving Unity world `Vector3` |
| Route facts | `circuit.isClosedCircuit`, `length`, `Kms`, `unitConversionFactor`, `distances` | bool, float, float, float, `float[]` |
| Identity | `race.MyGameround.Track.name` (trackAsset) and `.CircuitName`, from `car.field_Private_RaceConditions_0` as `StageCaptureContext` reads them; `RaceConditions.ReverseTrack`, `RaceConditions.TotalLaps`, `Runtime.GameAssemblyHash` | strings, bool, int |
| Per tick | car `transform.position`, `tracker.progressDistance`, `StageProgress.PlayerPercentage`, `car.Status`, the sampler's simulation seconds | floats, enum |

The route is read once, on the first tick with `Status == RACE`; waypoints do not move. Samples are taken every
physics tick from then until `END` or `DESTROYED`, 600 s, or the row cap.

### Output and bounds

All output is private and never committed. It goes to
`%LOCALAPPDATA%\Dbce\CourseExtraction\woden-<slug(trackAsset)>-<fwd|rev>-<UTC yyyyMMddTHHmmssZ>\`:
- **`route.json`** (`dbce.course-route@1`): game, `gameAssemblySha256`, `trackAsset`, `trackName`, `reverseTrack`,
  `totalLaps`, `isClosedCircuit`, `length`, `kms`, `unitConversionFactor`, `waypointCount`, `waypoints`
  (`[[x,y,z],…]`, float32 round-trip text), `distances`, `circuitSameAsStageProgress`, `capturedUtc`.
- **`progress.tsv`**, with the header `tick	sim_s	px	py	pz	progress_distance	player_percentage	status`.
- **`complete.tsv`**: names, sizes and sha256 in the existing `dbce-playback-seal-v1` form.

Limits:
- **Caps:** at most 20,000 waypoints (refuse above that) and 36,000 rows (600 s at 60 Hz), then stop. Under 8 MB
  in total.
- **Writing:** buffer in memory and write once at the end into a temporary directory, then rename it. A new
  directory each run (`CreateNew`, never overwrite). A failure leaves nothing partial.

### Comparison (offline, after the run)

1. **Route to path:** `CourseAxes.FromUnity` per waypoint, with `isLoop = isClosedCircuit`. Record
   `CoursePath.LengthM` against `length` (and `Kms` × 1000). If `length` is in metres they agree within 1%; that is
   not verified yet.
2. **Identity:** `CourseIdentity.Create("woden", Slug(trackAsset), isClosedCircuit ? "circuit" : "stage",
   reverseTrack ? Reverse : Forward)`. That gives `dbce.woden.kenya-ss1.stage.fwd` if Kenya SS1 is not closed.
   Record `HashPath(waypoints)` as the route's identity.
3. **Existing reference** (no new driving is needed for this step):
   - Inputs: `StagePlayback/references/woden-kenya-20261005/trajectory.tsv` (sha256 ea870d11…, 3,601 rows, 60 s at
     60 Hz) and `stage-context.json` (711747d3…: Kenya SS1, not reversed, 1 lap, car 8). The reference was recorded
     on game build `f422894d…`, the same as the installed game.
   - Run `CourseTracker` over `px/py/pz`.
   - Pass if: the first row gets a fix within 30 m; there are 0 discontinuities; p95 offset is at most 10 m;
     progress never decreases outside `WrongWay` spans; and the distance covered is within ±5% of the trajectory's
     own plan path, about 1,730 m.
4. **Game progress against the tracker** (`progress.tsv`):
   - Find the unit first. `progressDistance` is scaled by the median ratio to tracker distance; that is 1 if it is
     in metres. `PlayerPercentage` is 0-1 or 0-100, judged from its maximum.
   - Pass if: median |tracker − game| is at most 5 m and p95 at most 15 m. Report a systematic offset; never tune
     it away.
5. **Report:** the hashes of `route.json` and `progress.tsv`, the id, and the numbers. No claim of a live SimHub
   map.
