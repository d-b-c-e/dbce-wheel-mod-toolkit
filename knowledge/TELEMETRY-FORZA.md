# Forza "Data Out" telemetry — layouts, traps, and semantics

Why Forza: SimHub, ShakeIt, SimRacingStudio, most dashboards and motion
software parse it with **zero custom configuration**. Nothing about a game
needs to resemble Forza. Provenance in brackets.

## Three layouts; receivers validate by exact length

| Layout | Bytes | Structure | Emitted by |
|---|---|---|---|
| Sled (FM7 "sled") | 232 | physics block only | — |
| FM7 "Dash" | 311 | sled + 79-byte dash at 232 | OutRun2006Tweaks-FFB |
| Horizon 4/5 | 324 | sled + **12 unknown bytes at 232** + dash at 244 + **1 trailing byte** | art-of-sim-rally, iracing-arcade-wheel, cruisn-collection, MAME `midvunit_v.cpp` |

**323 bytes (232 + 12 + 79) is rejected silently.** SimHub logs `UDP Reader on
port 8000 started receiving unprocessed data` at packet rate — 19,880 times in
one race — and never connects. 331 is accepted but immediately `Paused=True`.
Established by brute-forcing sizes against SimHub's `ForzaReader`. If a dash
stays dead while the values look right, **check the length first, and read the
receiver's own log.** `[milestone-wheel-tools docs/forza-format.md, 2026-08-30]`

The two dash layouts are **not interchangeable**: a profile expects one.
`Dbce.Wheel.Telemetry.ForzaPacket.Write(frame, buffer, layout)` emits any of
the three from one `TelemetryFrame`; the sled is identical across them and the
dash shifts by 12.

## Offsets (Horizon; FM7 dash = Horizon − 12)

Sled: `IsRaceOn`@0 s32, `TimestampMS`@4 u32, `EngineMaxRpm`@8, `EngineIdleRpm`@12,
`CurrentEngineRpm`@16, `Acceleration XYZ`@20, `Velocity XYZ`@32, `AngularVelocity`@44,
`Yaw/Pitch/Roll`@56, `NormalizedSuspensionTravel`×4@68, `TireSlipRatio`@84,
`WheelRotationSpeed`@100, `WheelOnRumbleStrip`×4 **s32**@116, `WheelInPuddleDepth`@132,
`SurfaceRumble`@148, `TireSlipAngle`@164, `TireCombinedSlip`@180,
`SuspensionTravelMeters`@196, `CarOrdinal/Class/PI/Drivetrain/Cylinders` s32@212.
Dash (Horizon): `Position XYZ`@244, **`Speed` m/s @256**, `Power`@260, `Torque`@264,
`TireTemp`×4@268, `Boost`@284, `Fuel`@288, `Distance`@292, `BestLap`@296, `LastLap`@300,
`CurrentLap`@304, `CurrentRaceTime`@308, `LapNumber` u16@312, `RacePosition` u8@314,
`Accel`@315, `Brake`@316, `Clutch`@317, `HandBrake`@318 (u8 0–255), **`Gear`@319**,
`Steer` s8@320, `NormalizedDrivingLine`@321, `NormalizedAIBrakeDifference`@322,
unused@323.

Wheel arrays are **FL, FR, RL, RR** everywhere. Wrong order produces a
dashboard that looks plausible and is wrong; the C# `WheelValues` struct names
the corners for that reason. `Speed`@256 and `Gear`@319 are the anchors that
were validated live against SimHub. `[art-of-sim-rally tests; cruisn-collection]`

## Semantics that took a session each to learn

- **`IsRaceOn` must go false in menus** or dashboards never park — and it
  should be **true from the start-line countdown**, not from the green light,
  so a bass shaker follows the engine while revving on the line. Forces still
  wait for the green. `[art-of-sim-rally, 2026-09-02]`
- **Keep emitting between races.** SimHub freezes on the last received values
  when a stream stops. Emit zeroed frames at ~10 Hz while no game is live
  (`IdleKeeper`), **carrying the same `EngineMaxRpm`/`EngineIdleRpm`**: a fully
  zero packet is degenerate and can wedge SimHub's normalisation so it ignores
  the real stream afterwards. `[cruisn-collection ForzaKeeper]`
- **Gear 0 renders as reverse; Forza has no neutral.** Show 1 for neutral.
  `[MAME midvunit_v.cpp]`
- **Missing data is −1, not a confident zero.** A brake light reads 0 as
  "pedal released", not "unknown". Leave the field alone. `[OutRun2006Tweaks-FFB]`
- **Games without RPM**: synthesise from speed and a gear table
  (`RpmSynthesizer`); dashboards normalise against the max/idle you send, so
  send the same constants every frame.
- Some games' internal RPM scale is odd (iRacing Arcade tops ~805 on one car;
  Cruis'n USA raw tach ~912 at redline) — consumers should use the ratio, or
  scale into a believable 0–7,500 band. `[iracing-arcade-wheel, cruisn-collection]`
- Rate: physics rate (50–60 Hz) is fine; drift-free pacing beats fixed sleeps
  for standalone senders. Live host/port change: rebuild the socket and
  **park the old destination** with three `IsRaceOn = false` frames.
  `[art-of-sim-rally TelemetryPump]`
- **Port 8000 contention**: MOZA Pit House and SimHub both bind it. Let
  SimHub take the feed; drive the MOZA dash through SimHub's MOZA plugin.
  `[OutRun2006Tweaks-FFB]`
- A **sentinel in the unused byte 323** (`'R'` mod, `'K'` keeper, `'S'` synth)
  lets a probe tell whose packets it is seeing on a shared port.

## Where the numbers come from when the game does not have them

- **HUD OCR** — Cruis'n's displayed speed exists in no dumped memory (every
  decoding r < 0.55); the mod reads the MPH digits off the framebuffer with
  template matching. Correct by construction. `[cruisn-collection]`
- **Audio engine parameters** — Gravel's FMOD `setParameterValue` calls carry
  speed, slip, suspension, torque every frame. See `FMOD-AUDIO-AS-TELEMETRY.md`.
- **Steering torque as lateral load proxy** when no lateral acceleration
  exists (`accX = -constant_force × 9.81`). `[milestone-wheel-tools]`

## Tools

- `tools/forza/forza_probe.py` — listens, detects layout by length, prints
  live values, counts speed *decreases* (proves the emitter is not stuck
  ramping), logs CSV, warns on non-Forza lengths.
- `tools/forza/forza_synth.py` — emits a scripted drive in any layout to
  prove the receiver's profile without a game.
- Run the probe on a **mirror port** when SimHub holds the real one.
