## Unpublished constant-impact candidate - 2026-09-16

0.15.0 / native 0.8.0 adds optional independent finite constant effects.
Art consumer integration is separate ongoing work; this entry changes no verified
consumer pin. [Contract](CONSTANT-BURSTS.md) and [shared impact findings](../knowledge/FFB-IMPACTS.md)
distinguish attended diagnostic evidence from unverified production behavior.

# Consumers — who is on which version, and what they take

*The register of every project that vendors this toolkit. It exists so a fix
made here can be traced to the games that still need it, rather than being
assumed to have reached them.*

**Do not read the table below as current state.** Run the script; it reads each
consumer's real pin off disk:

```powershell
.\tools\Get-ConsumerStatus.ps1
```

It distinguishes **behind** from **needs to move**: for each consumer it diffs
the toolkit tree between that consumer's pinned tag and the current one, maps
the changed paths to vendored parts, and only demands action when a part that
consumer actually vendors was touched. A release that only changed the
PowerShell module leaves three of the four alone, and saying so is the
difference between a check people run and one they learn to ignore. It exits
non-zero and prints the exact `Sync-Toolkit.ps1` command for whoever must move.

This page carries what the script cannot know: which parts each consumer takes,
where its pin lives, and what breaks if you get it wrong.

---

## The register

| Consumer | Runtime | Parts vendored | Pin lives in | Needs a rebuild? |
|---|---|---|---|---|
| `art-of-sim-rally` | Unity 2019.4 Mono, Unity Mod Manager | `native`, `dotnet` | `lib/toolkit/VERSION` | no — ships the DLLs as files |
| `iracing-arcade-wheel` | Unity 6 IL2CPP, BepInEx 6 | `native`, `dotnet` | **`CHANGELOG.md`** | no — ships the DLLs as files |
| `OutRun2006Tweaks-FFB` | native `dinput8` proxy, **x86** | `include`, `native-x86`, `profiles` | `lib/toolkit/VERSION` | **yes** — the C headers are compiled in; `profiles` is data it deploys |
| `milestone-wheel-tools` | native `dinput8` proxy, UE4, x64 | `include`, `powershell` | `lib/toolkit/VERSION` | **yes** — `forza_packet.h` is compiled in |
| `cruisn-collection` | MAME fork, SDL2 | none yet | n/a | n/a |

`cruisn-collection` is deliberately not a consumer. Its force-feedback engine
lives in a GPL MAME fork and is the **seed for the SDL2 backend**, not a
consumer of the DirectInput one. See `docs/adr/0001-ffb-output-backends.md`.

## Which release actually affects whom

A release rarely obliges everyone. Match the area changed against the parts
column above before pinging four repos:

| If the change is in | Who must move |
|---|---|
| `native/wheelffb` (the DLL) | art-of-sim-rally, iracing-arcade-wheel, OutRun2006Tweaks-FFB |
| `native/wheelffb/include/wheelffb.h` (the C ABI) | OutRun2006Tweaks-FFB (rebuild) |
| `native/forza/forza_packet.h` | milestone-wheel-tools (rebuild) |
| `dotnet/Dbce.Wheel.*` | art-of-sim-rally, iracing-arcade-wheel |
| `native/forcemodel/*.h` (the shared model) | OutRun2006Tweaks-FFB (rebuild) |
| `profiles/force-profiles.ini` (the tunes) | OutRun2006Tweaks-FFB — but it is **data**, so redeploying the file is enough; no rebuild |
| `tools/powershell/DbceWheel.psm1` | milestone-wheel-tools |

## Per-consumer traps

- **`iracing-arcade-wheel` gitignores `lib/`.** Its vendored artifacts are not
  tracked, so `lib/toolkit/VERSION` exists on disk but never appears in a diff.
  Its pin is a line in its `CHANGELOG.md`; update that when you bump it, or the
  version becomes unknowable from the repo alone.
- **`OutRun2006Tweaks-FFB` must ship `WheelFfb.dll` beside `dinput8.dll`** in
  the game folder. CMake copies it into `build/bin` after every build, but a
  deploy that copies only `dinput8.dll` silently loses all force feedback. It
  is also the only **x86** consumer: `-Parts native-x86`, not `native`.
- **`art-of-sim-rally` renames the native DLL** to `UnityForceFeedback.dll`,
  the name the game's own missing library had and which the mod P/Invokes.
- **`milestone-wheel-tools` compiles `forza_packet.h`** with MinGW, not MSVC.
  Its `build.sh` refuses to run if `lib/toolkit` is missing.
- Both native consumers **compile a header in**, so bumping their pin without
  rebuilding changes nothing. The two Unity mods ship DLLs, so for them the
  copy *is* the update.
- **`profiles/force-profiles.ini` is the exception to all of that.** It is read
  at runtime from beside the game binary, so a new tune reaches a player by
  copying one text file. That is the whole point of it: tuning force feedback
  means driving A, driving B and deciding, and B should not mean a build.

## Updating a consumer

```powershell
# in the consumer repo
.\tools\Sync-Toolkit.ps1 -Version vX.Y.Z -Parts <its parts>
# rebuild if it compiles a header in, then smoke-launch, then commit lib/toolkit
```

Then add a line to its changelog saying which toolkit version and why. The
point of pinning is that a toolkit change never reaches a game without an
explicit, diffable bump.

### A sync never clobbers a local edit

Each sync records a SHA-256 of every file it wrote, in
`lib/toolkit/MANIFEST.txt`. Next time, a file whose hash no longer matches was
edited in the consumer, so it is **left alone**, reported, and the script exits
non-zero. It stays flagged until someone resolves it, rather than being
detected once and quietly overwritten later.

This exists for `profiles/force-profiles.ini`. A tune arrived at by driving is
not something a routine version bump should delete.

| You want | Do |
|---|---|
| the toolkit's copy | re-run with `-Force`; yours is saved as `<name>.local-<timestamp>` first |
| to keep yours | nothing; that one file stays behind, and keeps saying so |
| to keep it properly | promote it, below |

### Promoting a tune back into the toolkit

A tune in `force-profiles.user.ini` beside the game is never touched by
anything here, which also means nothing backs it up. When one is worth keeping:

```powershell
.\tools\Import-Profile.ps1 -From '<game folder>' -Id mytune@1 -As arcade-outrun
```

It appends the section to `profiles/force-profiles.ini` as the next free
version of that family. It never overwrites a published version, keeps
`inherits` so the tune is not silently changed, and refuses if the parent is
not published. Then regenerate the conformance golden file, because a new
profile changes the pinned output:

```powershell
.\native\forcemodel\build_conformance.bat --update
dotnet test Dbce.Wheel.sln
```

## History

What each consumer last took, and when. Add a row when you bump one.

| Date | Toolkit | Consumer | What it took |
|---|---|---|---|
| 2026-09-03 | v0.1.0 | art-of-sim-rally | first consumer: telemetry encoder, then the native DLL |
| 2026-09-04 | v0.1.0 | iracing-arcade-wheel | telemetry + native DLL; local copies of both removed |
| 2026-09-04 | v0.2.0 | milestone-wheel-tools | `forza_packet.h` replaced its local structs; `DbceWheel.psm1` replaced Steam discovery written out three times in one repo |
| 2026-09-04 | v0.2.0 | OutRun2006Tweaks-FFB | the whole FFB device layer via the C ABI, 543 lines removed for 139 |
| 2026-09-04 | v0.2.0 | iracing-arcade-wheel | bumped for the `Start()` / `DIEP_START` fix |
| 2026-09-04 | v0.2.0 | art-of-sim-rally | bumped in a separate session |
| 2026-09-04 | v0.3.0 | milestone-wheel-tools | forza_packet.h unchanged since 0.2.1, so no rebuild; picked up the edit-protecting sync script |
| 2026-09-04 | v0.3.0 | iracing-arcade-wheel | additive only; keeps the vendored DLLs in step |
| 2026-09-04 | v0.3.0 | OutRun2006Tweaks-FFB | the shared force model and the versioned profiles, behind `FFBProfile`; `legacy` stays the default |
| 2026-09-04 | v0.2.1 | milestone-wheel-tools | `Set-IniValueCrlf` no longer duplicates a `key = value` line. The only consumer 0.2.1 obliged to move; the other three vendor nothing it changed |

## Adding a consumer

Copy `tools/Sync-Toolkit.ps1` into the new repo, run it with the parts it
needs, add a row to the register and the history above, extend the `$consumers`
list in `tools/Get-ConsumerStatus.ps1`, and write a case file under
`knowledge/games/`. `knowledge/NEW-GAME-CHECKLIST.md` is the longer route for a
game that has no mod yet.
