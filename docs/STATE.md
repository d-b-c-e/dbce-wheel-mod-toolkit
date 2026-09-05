# Where we left off

**Last session: 2026-09-05.** Start here when picking this up fresh.

Independent review and first fixes: [REVIEW-2026-09-05.md](REVIEW-2026-09-05.md).
The review baseline is tagged `assessment-2026-09-05` (`4e99136`). Version
0.10.0 adds source-force rise detection, fixes centered directionless impacts,
repairs the missing profile references, and adds CI. 105 managed tests pass.
No hardware force or consumer deployment was used to assess physical feel.
Cruis'n is adopting the pure header component; its SDL device lifecycle remains
separate. Selection identity, signal provenance and fake-device lifecycle tests
are still priorities, not completed features.

---

## TL;DR

**Current source release: v0.10.0.** Do not trust a version number written anywhere in
prose, including here - run `.\tools\Get-ConsumerStatus.ps1`, which reads each
consumer's real pin off disk.

**The force model is data.** Every parameter that decides how a game feels is a
named, versioned profile in `profiles/force-profiles.ini`, deployed beside the
game and read at runtime, so a tune is swapped by editing a text file rather
than by building. The model is shared with native mods
(`native/forcemodel/force_model.h`), and the C++ and C# implementations are
pinned to one golden file so they cannot drift. The file explains every dial in
plain terms, because it is the only tuning documentation a player has.

OutRun2006Tweaks-FFB selects one with `FFBProfile`. It still defaults to
`legacy`, its own built-in model, so **nothing about how it drives has changed
until someone chooses a profile** — which is what makes an A/B possible.

Since 0.3.0, in order: the native half went out (a C ABI header with a runtime
loader, hardware periodic effects, device selection by instance GUID, an x86
build, a shared PowerShell module - OutRun deleted 543 lines of its own
DirectInput code for 139); `GetDeviceGuid` and condition effects landed;
`simlite@1` was checked against art of rally's own force curve and found to
carry this repo's conditioning rather than the game's, corrected as
`simlite@2`; the clamp moved out of the model to the device edge, which is what
lets soft saturation tell a crash from a hard corner; and a code preset now
returns a whole tune rather than a model half that pairs with the wrong
conditioning.

Everything is verified **at the bench**, including against the real MOZA R12.
Nothing migrated this session has been **driven in a game**.

The single highest-value next action is one lap of OutRun 2006 with
`FFBDiagnosticLog = true`. It settles two things at once: whether the migrated
force-feedback layer works in the game, and which `EVWORK_CAR` field is
actually steering.

---

## Verified working (evidence, not assertion)

| Thing | How it was verified |
|---|---|
| `WheelFfb.dll` exports | 37 undecorated exports in **both** x64 and x86, checked by `dumpbin` against `WheelFfb.def`; `build.ps1` also fails if `include/wheelffb.h` disagrees with the `.def` |
| Full C ABI at real hardware | `native/wheelffb/test/smoke.c`, **25/25 stages in both architectures** at a MOZA R12: enumerate, open exclusive, constant force, 10 updates with none rejected, two `GUID_Sine` slots, envelope updates, `ZeroForces`, `PanicStop` with autocentre restored, `FreeDirectInput` |
| Device selection by GUID | Same test: a GUID belonging to nothing **falls back** to the candidate search; the real instance (`D71B8350-61B7-11F1`) is honoured |
| Driver periodic support | The R12 enumerates Square, Sine, Triangle, SawtoothUp, SawtoothDown |
| Managed libraries | **87 tests** (48 `Dbce.Wheel.Ffb`, 39 `Dbce.Wheel.Telemetry`) |
| `forza_packet.h` | Conformance test passes: 232 / 311 / 324, Speed@256, Gear@319, identical sled across layouts, 323 refused |
| `DbceWheel.psm1` | Own suite green: 8 INI edge cases plus discovery. Found Steam on the **non-default** `D:\Program Files (x86)\Steam`, and 3 DirectInput devices including the R12 as key `0006346e`, type `0x18` |
| milestone-wheel-tools | `dinput8.dll` builds against the vendored header (MinGW GCC, x64); all four PowerShell scripts parse |
| OutRun2006Tweaks-FFB | `dinput8.dll` builds (Win32 Release) and CMake copies `WheelFfb.dll` beside it |
| Packaging and release | `package.ps1` runs every suite and produced the 402 KB zip; `v0.7.1` published with it attached |

## Built but NEVER DRIVEN ⚠️

| Thing | What is missing | Risk |
|---|---|---|
| **The shared force model in OutRun** | `FFBProfile` defaults to `legacy`, so the shared path has never executed in the game | Wiring verified only by the binary containing it. The first drive on `arcade-outrun@1` is the test |
| **OutRun's migrated FFB layer** | Has not run inside the game once since the swap | The bench test covers the DLL, not the wiring. `DeferredInit` firing, the remap layer's GUID being present at that moment, and the model still driving the output are all unproven |
| **Periodic effects actually playing** | The smoke test runs them at **magnitude 0** on purpose, so nothing has ever been felt | Creation, the strategy probe and envelope updates are proven; that they produce audible road texture is not |
| **OutRun's steering field** | Unknown which of six floats it is | The spring and damper terms are running on roughly 1% of their intended signal. Do not tune them first |
| **iracing-arcade-wheel force feedback** | Never judged at a **powered** base | Its signal is a slip × load proxy; the sign has never been confirmed. Run the sign-by-speed-band check in `knowledge/FFB-SIGNAL-DESIGN.md` |
| **Milestone telemetry after the encoder swap** | No packet seen on the wire since | Byte-identical by construction and by conformance test, but the Horizon **sentinel byte changed from 0 to `'R'`** |
| **The Fanatec fixes** (art-of-sim-rally 0.2.2) | No Fanatec hardware here | Written from a user's support bundle. A reply to that user is outstanding |

Do not describe any of these as working until they have actually run.

---

## Next actions, in priority order

### 1. One lap of OutRun 2006 — settles two things

Copy **both** DLLs next to `OR2006C2C.EXE` (the build already puts them
together), set `FFBDiagnosticLog = true` in `OutRun2006Tweaks.ini`, drive a lap.

```powershell
Copy-Item E:\Source\OutRun2006Tweaks-FFB\build\bin\*.dll `
          E:\Source\outrun2006\outrun2006-redux\game\ -Force
```

Then read `OutRun2006Tweaks.log`:

- `FFB: Initialization complete (WheelFfb)` means the migration works in game.
- The `FFB STEERSCAN` line prints the observed min/max of `1C8`, `1CC`, `1D0`,
  `1D4`, `1DC`, `1E0`. Steering is whichever reaches about ±1, changes sign with
  the corner, and returns to zero on the straights.

Both DLLs, `force-profiles.ini` and the config key are already deployed there.
`FFBProfile = legacy` for now: settle the steering field first, because the
spring and damper terms of every profile depend on that signal, and comparing
tunes before it is fixed compares nothing.

`OutRun2006Tweaks.ffb.log` beside it carries the DirectInput side.
Note the deployed `DINPUT8.dll` there is from 2026-03-30 and predates all of
this; back it up first if you want the old behaviour back quickly.

### 2. iRacing Arcade force feedback at a powered base

Never done. Log `sign(force)` against `sign(steer)` by speed band before
trusting the signal.

### 3. Reply to the Fanatec user

art-of-sim-rally 0.2.2 shipped the direct wheel input, the crash fix, the Flip
button, the lighter default and a troubleshooting page
(`art-of-sim-rally/docs/TROUBLESHOOTING.md`, not in this repo). They have not
been told.

### 4. Confirm Milestone packets on the wire

`milestone-wheel-tools/tools/forza_listen.py` (that repo, not this one) with
`mirror_port` set, one race. Checks the sentinel change did not upset
anything.

### 5. Extract the dinput8 proxy scaffold

Now the **largest remaining piece of duplicated native code**: OutRun and
Milestone each carry their own. Milestone's is the better donor (vtable index
table, SIXDOF re-type at both read sites, blank-twin filter). See
`knowledge/DINPUT8-PROXY.md`.

### 6. Still deferred

SDL2 backend behind the same interface (seed: cruisn `mvffb`, see ADR-0001);
`dotnet/Dbce.Wheel.Input`; the three `templates/` scaffolds, which are still
empty `.gitkeep` directories; OutRun's force model as a C++ `Arcade` reference.

---

## Things that already bit us (don't rediscover)

- **An effect whose `Start()` fails never plays, and only the constant force
  notices.** `DIERR_NOTEXCLUSIVEACQUIRED` on `Start` is survivable for a
  constant force, whose every update carries `DIEP_START`; a periodic effect's
  updates deliberately do not, so it sets parameters perfectly and stays silent
  for the session. Found by the smoke test on its first run.
- **In a game modded with a `dinput8.dll` proxy, resolve `DirectInput8Create`
  from `system32` by full path.** An import binds to the proxy beside the game
  exe, so the FFB layer ends up running *through* the layer it should sit
  beside.
- **.NET's `\Z` also matches before a trailing newline.** It split a CRLF when
  appending a key to a file's last section. Use `\z`.
- **`GetVersion` collides with the Win32 macro.** The export is
  `GetWheelFfbVersion`.
- **Long heredocs through the Bash tool mangle backslashes.** `'\\\n'` arrived
  as backslash-plus-n and a patch silently failed to match. Write the script to
  a file and run it by path; use `chr(92)` when a literal backslash matters.
- **`powershell -File script.ps1 -Parts a,b` passes one string, not an array.**
  `Sync-Toolkit.ps1` now splits on commas itself.
- **iracing-arcade-wheel gitignores `lib/`.** Its toolkit pin is recorded in its
  `CHANGELOG.md`; do not expect `lib/toolkit` in its diffs.
- **A console app cannot hold an exclusive DirectInput acquisition.** The smoke
  test creates a real top-level window, which is why it now passes `StartEffect`.

---

## Unverified assumptions worth re-checking

- **The constant-force encoding changed under OutRun.** It used a single X axis
  with direction `{0}`; the toolkit sends two axes (falling back to one) with
  direction length equal to `|magnitude|`. The R12 reads the magnitude sign and
  ignores direction, so this *should* be identical or better, but it has not
  been compared by feel.
- **`FFBGlobalStrength` moved from the effect's `dwGain` to the magnitude.**
  Both are linear, so the product is the same. Assumed, not measured.
- **Milestone's Horizon trailing byte is now `'R'` rather than 0.** No Forza
  title reads it and SimHub validates by length, so this should be invisible.
- **OutRun's periodic slots use 25 Hz and 40 Hz**, matching the old code. The
  envelope maths moved but the numbers did not.
- **The `templates/` directories and `native/dinput-proxy` are empty.** The
  README and `CLAUDE.md` describe what belongs in them, not what is there.

---

## Environment (this machine)

- MOZA R12 base (`0006346e`, DirectInput type `0x18`), SIMAGIC DS-8X shifter
  (`05310483`), MOZA multi-function stalk (`0024346e`).
- Steam on `D:\Program Files (x86)\Steam` — **not** a default path.
- Visual Studio 2022 **Build Tools** (no full IDE); CMake ships inside it at
  `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.
- MSYS2 MinGW-w64 at `E:\msys64` — needed only by milestone-wheel-tools.
- Fanatec and MOZA R5 behaviour comes from user support bundles, not hardware
  here.

## Where to read next

`CLAUDE.md` for the rules and the layout, `docs/CONSUMERS.md` (with
`tools/Get-ConsumerStatus.ps1`) for who is on which version,
`docs/ROADMAP.md` for what each consumer still owes, `docs/CONSUMING.md` for how a mod vendors this, `CHANGELOG.md` for
what 0.2.0 changed, and `knowledge/` for anything device- or
signal-related. The per-game case files under `knowledge/games/` say what each
consumer is and what it still owes.
