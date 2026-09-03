# Audio parameters are telemetry

Source: milestone-wheel-tools (Gravel, FMOD Studio), 2026-08-29/30.

## The insight

A game with no telemetry still drives its engine audio from live physics
every frame. Hook the audio engine's parameter setter and the parameters
**are** the telemetry: speed, slip, suspension, torque, surface, collisions.

## Hooking FMOD Studio

- Target: `FMOD::Studio::EventInstance::setParameterValue`, by its **mangled
  MSVC name** `?setParameterValue@EventInstance@Studio@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDM@Z`,
  via the delay-load import table.
- **The delay-load thunk self-unhooks**: calling the original slot value
  resolves the import *and* overwrites your hook. Resolve the real function
  with `GetProcAddress` once the DLL is loaded; use the thunk only as a
  fallback, then re-install.
- Do the hooking from the first exported call, never `DllMain`.

## Classifying parameters by call count

Run with a discovery mode that dumps every parameter name with its observed
range and call count. ~175k calls per race = a continuous channel (speed,
slip, suspension); a few hundred = an event (collision, gear change). Gravel's
table lives in `milestone-wheel-tools/games/gravel/README.md`.

## What the numbers mean

- **Impact intensity is effectively binary** (368 impacts: median 1.000,
  mean 0.878) — it says *that* you hit something. Collision **speed** (median
  0.23, max 0.46) is the discriminator; weight magnitude `0.15 + vel × 1.8`.
- A wall scrape fires ~15 events in 200 ms; re-triggering a decaying spike
  turns it into sustained rumble, which is right.
- Zero every channel when no vehicle is live: FMOD's last in-race values
  persist through menus and a bass shaker sits on constant output.
- Say which values are **assumed, not measured** (speed scale eyeballed
  against the speedo, torque scaled to a nominal 400 Nm "the shape is real,
  the units are invented", idle = 12% of max). Keep that list in the docs.

## Applies to

Any FMOD Studio or Wwise game. Wwise has `SetRTPCValue` in the same role.
