# Templates — starting points for a new game

Three shapes cover every consumer so far. Pick by how the game is built, not by
what you want to add:

| Template | Pick it when | Existing example |
|---|---|---|
| `unity-mono-umm/` | Unity with a Mono runtime, so Unity Mod Manager and Harmony work | art-of-sim-rally (Unity 2019.4) |
| `unity-il2cpp-bepinex/` | Unity compiled to IL2CPP, so you need BepInEx 6 and interop assemblies | iracing-arcade-wheel (Unity 6) |
| `native-dinput8-proxy/` | Not Unity, or no managed runtime to hook — a native game you sit beside | OutRun2006Tweaks-FFB, milestone-wheel-tools |

Fingerprint the engine first. `dbce-mod-toolkit` does it across a Steam library
and will tell you which of the three you are in.

## What a template is, and is not

Each is a **documented starting point**, not a project that compiles. The
per-game half — finding the car state, which fields mean what, where to hook —
is exactly the part no template can supply, and a scaffold that pretended
otherwise would rot the first time a game updated.

What they do carry is the part that is the same every time and that everyone
gets wrong once: the non-negotiables, the toolkit boundary, the order of the
force chain, and the wiring from a car-state struct to a force on the wheel.

## Using one

```powershell
Copy-Item -Recurse templates\<shape> E:\Source\<new-repo>
```

Then, in order:

1. Read `knowledge/NEW-GAME-CHECKLIST.md`. It is the long form of what follows.
2. Fill in the `TODO` markers in the copied `CLAUDE.md`. Do not delete the
   non-negotiables; they were each paid for once already.
3. Copy `tools/Sync-Toolkit.ps1` from this repo into the new one and vendor
   what that shape needs. Each template's README names its parts.
4. Write the sampler: game state to `ForceInputs`. This is the real work.
5. **Measure the sign** before tuning anything. A force that pushes the wrong
   way feels like a broken wheel, not like a wrong constant, and every project
   here has got it backwards at least once.
6. Pick a profile from `profiles/force-profiles.ini` as a starting tune, then
   publish the game's own as a new family when it settles.
7. Add a case file under `knowledge/games/` saying what the engine was, what
   worked and what did not.

## The rule that outranks the rest

Everything about **how a force feels** belongs in the toolkit; everything about
**what the car is doing** belongs in the game's repo. When a change could go
either side, ask whether a second game would want it. If yes, it goes here.
