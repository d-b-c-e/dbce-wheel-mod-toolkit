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

| Type | Role |
|---|---|
| `CourseVector`, `CourseAxes` | Metres on north/east/up axes. Unity: north = +Z, east = +X, up = +Y, yaw = `eulerAngles.y`. This is the only mapping under which Unity's yaw already matches SimHub's "0 = north, clockwise, 90 = east". `ToSimHubCarCoordinates` shows the `[-north, up, east]` array SimHub builds |
| `CourseIdentity` | `dbce.<game>.<venue>.<route>.<fwd|rev>[.<hash16>]`. Tokens are `[a-z0-9-]`, 1-40 characters. The id is at most 127 bytes (SimHub `EnvironmentId` is a zero-terminated UTF8[128]). `HashPath` is FNV-1a 64 over the exact float32 route points, for custom or generated courses. `Slug` turns stable internal names into tokens |
| `CoursePath` | The game's own route as a polyline in driving order, as a loop or point to point. Length and projection are 3D, so a bridge is not confused with the road under it. It refuses fewer than 2 points (3 for a loop), non-finite points, and anything under 10 m (a wrong unit). `Reversed()` gives the other direction |
| `CourseTracker` | Turns positions into progress, laps and completion; the rules are below |
| `CourseSample` | One snapshot: id, name, length, loop flag, progress, laps, finished, position, yaw, lap time, session id, discontinuity count, spectator/replay/control flags. `TryValidate` checks the invariants |
| `SimHubCourseFeed` | Draft "dbce course + dash v1" external-sim layout, with an encoder and a decoder. No socket, no SimHub files |

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

## Next: the smallest real integration

**Woden, read-only.**
- The mod already records the course id inputs.
- A private 60 s Kenya SS1 trajectory reference exists.
- A dev-only dump at stage load would capture the `WaypointCircuit` positions, loop flag and length, plus the
  game's `progressDistance` and `PlayerPercentage` samples, to a private file. No UDP, no SimHub.
- Then, offline, `CourseTracker` runs over the dumped circuit and the recorded trajectory, and its progress is
  compared with the game's own.

This needs one bounded kobe runtime grant.
