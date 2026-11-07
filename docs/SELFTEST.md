# The in-mod self-test (dbce.selftest@1)

The mod runs a scripted sequence in its own process at game speed, and the agent reads one result file afterwards
(rung 2 of the testing ladder). The design is `dbce-project-mgmt/docs/architecture/2026-11/04-cross-cutting.md`
D.2-D.4. This page is the toolkit's half of that contract.

| Part | Where |
|---|---|
| Script schema | `schema/dbce-selftest.schema.json` |
| Result schema | `schema/dbce-selftest-result.schema.json` |
| Fixture scripts and their expected `result.json` and host calls | `schema/fixtures/selftest/` |
| C++ interpreter (native proxies, UE4SS, standalone apps) | `native/selftest/dbce_selftest.hpp` |
| Tests | `native/selftest/build_test.ps1` (fake host, x64 and x86), `tools/tests/Test-SelfTestSchema.ps1` |

A C# interpreter for the Unity mods (`shared/selftest/SelfTest.cs`) is not written yet. It should run the same
fixtures and produce the same `result.json` files.

## Agent side

1. Set the developer opt-in: `DBCE_DEV=1` in the game's environment, or `[Dev] SelfTest=true` in the mod's config.
   A player default never has either.
2. Launch the game (under the rig lease, with saves backed up as usual).
3. Write `%LOCALAPPDATA%\dbce\<game>\selftest\script.json`. Only a script written after the game started runs, and
   it runs once per write. Writing it before the launch does nothing.
4. Wait for `selftest\<name>\result.json`, or for the process to exit. The file is written last, through a rename, so
   when it exists the run is over. A script whose name can't be read reports in `selftest\invalid\`.
5. Read `result.json`, the `frame-*.png` and `state-*.json` files it names, and `log.txt`. Restore saves and config.

A frame is evidence that something drew, not acceptance. Visual judgement stays with the owner.

## Script

```json
{ "schema": "dbce.selftest@1", "name": "cameras-and-f6", "muteFfb": true, "timeoutSeconds": 120,
  "steps": [
    { "wait": "menu" },
    { "f6": "open", "page": "Cameras", "view": "advanced" },
    { "capture": "f6-cameras" },
    { "f6": "close" },
    { "start": "race" },
    { "wait": "race" },
    { "camera": "next", "repeat": 3, "each": [{ "wait": "frames:30" }, { "capture": "cam-{i}" }, { "dump": "cam-{i}" }] },
    { "key": "F7" },
    { "set": "Triple.Enabled", "value": false, "session": true },
    { "capture": "triple-off" },
    { "quit": true }
  ] }
```

| Step | Does |
|---|---|
| `wait` | `menu` or `race` (what the mod reports), `frames:N` or `ms:N` from the step's start |
| `f6` | `open` with an optional `page` (id or name) and `view` (`simple`, `advanced`, or `more` for More options), or `close` |
| `capture` | `frame-<name>.png` from the game's own frame, so it works unfocused |
| `dump` | `state-<name>.json`, the mod's probes (the `selfTest.probes` of its `dbce-mod.json`) |
| `start` | a host-specific start, such as `race` |
| `camera` | `next` or `previous` |
| `key` | a mod key such as `F7`, through the mod's own key path; no OS input |
| `set` | a setting for this session only, with `value`; `session` may only be `true`. Never saved (STD-044) |
| `quit` | the game's normal quit path; only as the last step |

- Any step except `quit` can have `repeat` (1 to 100) and `each`, a list of steps without their own repeat. Each
  repetition runs the step, then the `each` steps. `{i}` in a capture or dump name is the repetition, from 1.
- `muteFfb` defaults to `true` (safety 2). Force feedback stays muted for the rest of the session. If the mute fails,
  no step runs.
- `timeoutSeconds` (default 120) covers the whole run. A step still waiting then fails.
- At most one step starts per frame, so each step sees the frame the step before caused. A failure skips the
  remaining steps, but a last `quit` still runs so the game closes.
- Unknown fields, two actions in one step, and an unknown wait or view are refused before anything runs. The reason is
  reported as failure step 0.

## Result

```json
{
  "schema": "dbce.selftest.result@1",
  "name": "failing-step",
  "pass": false,
  "muteFfb": true,
  "ms": 96,
  "failure": {"step": 3, "reason": "no page 'Replays' in this mod"},
  "log": "log.txt",
  "steps": [
    {"step": 1, "do": "wait menu", "status": "ok", "ms": 0},
    {"step": 3, "do": "f6 open Replays more", "status": "failed", "ms": 0, "reason": "no page 'Replays' in this mod"},
    {"step": 4, "do": "capture never-taken", "status": "skipped", "ms": 0},
    {"step": 6, "do": "quit", "status": "ok", "ms": 0}
  ]
}
```

Some steps are left out of the example above. Each step after repeats has a `status` of `ok`, `failed` or `skipped`,
its milliseconds, the `file` a capture or dump wrote, and the `reason` it failed. A step the mod doesn't support fails
with "this mod does not support <step>". `failure.step` is 0 when the script itself or the force feedback mute failed.

## Mod side (C++)

```cpp
#include "selftest/dbce_selftest.hpp"
using dbce::selftest::Status;

struct GameHost : dbce::selftest::Host {
    const char *Where() override { return inRace() ? "race" : inMenu() ? "menu" : ""; }
    Status MuteFfb() override { muteForSession(); return Status::ok(); }
    Status OpenPage(const char *page, const char *view) override
    { return dbce::panel::OpenForCheck(page, view) ? Status::ok() : Status::fail(std::string("no page '") + page + "'"); }
    Status ClosePanel() override { dbce::panel::SetOpen(false); return Status::ok(); }
    // CycleCamera, PressModKey, SetSession, Capture (Busy until the frame is written), Dump, StartRace, Quit,
    // LogTail: whatever the game has. A callback left out answers Unsupported.
};
static GameHost g_host;
static dbce::selftest::Runner *g_runner;

// at Configure time, with the panel's other settings:
dbce::selftest::Options o;
o.folder = dbce::selftest::DefaultFolder(L"dirt4");
o.iniPath = L"...\\dbce-dirt4.ini";            // for [Dev] SelfTest
o.log = modLog;
g_runner = new dbce::selftest::Runner(g_host, o);
panelConfig.selfTestTick = [] { g_runner->Tick(); };
```

- The panel's `PollDev` calls `Config::selfTestTick` on every presented frame of the game's swap chain. The callbacks
  run there, on the present thread.
- A callback that needs a later frame returns `Status::busy()`, and the same call is made again on the next frame. A
  capture does this until its file is written.
- `Dump` fills the probe list. The interpreter writes it as a flat JSON object, in the order given.
- A mod without the toolkit panel calls `Runner::Tick()` once per frame from its own present hook.
