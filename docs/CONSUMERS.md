# Consumers — who is on which version, and what they take

*The register of every project that vendors this toolkit. It exists so a fix
made here can be traced to the games that still need it, rather than being
assumed to have reached them.*

**Do not read the table below as current state.** Run the script; it reads each
consumer's real pin off disk:

```powershell
.\tools\Get-ConsumerStatus.ps1 -Root E:\Source -All
# Structured output for tooling, with the same exit semantics:
.\tools\Get-ConsumerStatus.ps1 -Root E:\Source -Current v0.13.0 -AsJson
```

It distinguishes **behind** from **affected**: for each declared component it
diffs the pinned tag against the comparison tag and checks the source paths that
component uses. A release that changes only the PowerShell module leaves every
other consumer unaffected. Native/header overrides are compared independently;
a newer component is never recommended for downgrade to an older target.

Exit 0 means all declared components are current, newer than the target, or
unaffected. Exit 1 means affected source needs review, or a repo, pin, provenance
or local comparison range needs attention. A missing or invalid explicit pin is
an error, not permission to guess a version. Local/development identities remain
explicit provenance-review items. The tool is read-only: no fetch, download,
build, file update or automatic sync. `-All` is retained for compatibility; all
consumers are shown. Dependency pins do **not** certify installed builds or UX
adoption; use the consumer's package/runtime and UX evidence for those claims.

This page carries what the script cannot know: which parts each consumer takes,
where its pin lives, and what breaks if you get it wrong.

---

## The register

| Consumer | Runtime | Parts vendored | Pin lives in | Needs a rebuild? |
|---|---|---|---|---|
| `art-of-sim-rally` | Unity 2019.4 Mono, Unity Mod Manager | Native output; managed Ffb and Telemetry | `lib/toolkit/VERSION` | DLL replacement alone may not; new API call sites require a rebuild |
| `iracing-arcade-wheel` | Unity 6 IL2CPP, BepInEx 6 | Native output; managed Ffb and Telemetry; separately pinned Recording extension | **`toolkit.version`**, tracked; `recording-toolkit.json` is separate | Same managed/native contract rule |
| `woden-rally-edge-wheel` | Unity 6 IL2CPP, BepInEx 6 | Native output; managed Ffb and Telemetry; separately pinned Recording extension | `lib/toolkit/VERSION`; `lib/recording/provenance.json` is separate | Same managed/native contract rule |
| `drive-rally-wheel` | Unity Mono, BepInEx 5 | **Managed Telemetry only** from the release; separately pinned Recording extension | `dependencies.json` → `toolkit.version`; `vendor/recording/provenance.json` is separate | Rebuild for consumer/API changes; no native wheel library is implied |
| `OutRun2006Tweaks-FFB` | Native `dinput8` proxy, **x86** | Baseline model/profile/encoder/proxy headers and profiles; separately overridden ABI header and x86 native DLL | `lib/toolkit/VERSION`; override in `lib/toolkit/NATIVE-VERSION` and `NATIVE-PROVENANCE.json` | **Yes** for compiled headers; keep matching native DLL in the package |
| `milestone-wheel-tools` | native `dinput8` proxy, UE4, x64 | `include`, `powershell` | `lib/toolkit/VERSION` | **yes** — `forza_packet.h` is compiled in |
| `cruisn-collection` | MAME fork, SDL2 | `force_model.h`, `force_profile.h`, `impact_mixer.h`, `signal_sample.h` and profiles | `lib/toolkit/VERSION`, with `MANIFEST.json` | **Yes** for compiled headers; profile data is deployed separately |

**Observed dependency declarations, 2026-09-17:** iRacing and Drive declare
v0.13.0; Woden v0.12.0; Cruisn v0.11.1; Milestone v0.8.0. OutRun retains a
v0.8.0 baseline with a v0.13.0 native/header override. Art's active owner work
uses a `local+<commit>.clean` source identity; run the tool for its exact current
hash. These observations are not adoption dates or released/installed-version
claims, and may change as the owning tasks work.

Cruisn **is a consumer of force/shaping headers and profiles**. Its SDL output
engine remains in the GPL MAME fork and is still a seed for the planned SDL2
backend, rather than a user of the toolkit DirectInput DLL. Its manifest records
all four headers above, `make_release.ps1` deploys the toolkit profiles, and its
native analysis/verification tools include the shared headers. See
[ADR-0001](adr/0001-ffb-output-backends.md).

## Which release actually affects whom

A release rarely affects everyone. Match the source area and component pin;
separate Recording extensions are not part of published-release inference:

| If the change is in | Consumers to review |
|---|---|
| `native/wheelffb` (DLL implementation, excluding headers/tests) | Art, iRacing, Woden; OutRun's **native override pin** |
| `native/wheelffb/include/wheelffb.h` (C ABI) | OutRun's **native override pin**, then rebuild |
| `native/forza/forza_packet.h` | Milestone and OutRun baseline, then rebuild |
| `dotnet/Dbce.Wheel.Ffb/` | Art, iRacing and Woden; **not Drive** |
| `dotnet/Dbce.Wheel.Telemetry/` | Art, iRacing, Woden and Drive |
| `native/forcemodel/force_model.h`, `force_profile.h` | OutRun baseline and Cruisn, then rebuild |
| `native/forcemodel/impact_mixer.h`, `native/telemetry/signal_sample.h` | Cruisn, then rebuild |
| `native/dinput-proxy/dinput_proxy.h` | OutRun baseline, then rebuild |
| `profiles/force-profiles.ini` | OutRun baseline and Cruisn; deploy data, no code rebuild solely for the profile file |
| `tools/powershell/DbceWheel.psm1` | milestone-wheel-tools |
| Unpublished `dotnet/Dbce.Wheel.Recording/` | Follow iRacing/Woden/Drive's separate provenance and verification; do not infer an official package bump |

Woden also has a profiles file in its toolkit checkout, but its inspected package
allowlist does not deploy that file and its current code does not consume it.
The status tool follows the effective native/Ffb/Telemetry paths, rather than
treating this cached file as adopted runtime profile support.

## Per-consumer traps

- **`iracing-arcade-wheel` gitignores `lib/`, but now tracks `toolkit.version`.**
  Read the explicit pin first. For older checkouts lacking that file, the tool
  retains the historical highest-version changelog inference and labels it as
  legacy inference. An empty, malformed or unreadable explicit file must never
  fall back to changelog guesses. Update the tracked pin and dependency evidence
  together when the owning project adopts a release.
  Its `tools/Sync-Recording.ps1` builds the unpublished Recording extension into
  `lib/recording` and records source-file hashes/base revision and binary hash in
  `recording-toolkit.json`. Keep this separate from the released toolkit pin;
  that manifest's base toolkit is not the current Ffb/Telemetry/native pin.
  Source/binary provenance does not establish gameplay capture acceptance.
- **Drive's release payload contains only `Dbce.Wheel.Telemetry.dll`.** Its
  `dependencies.json` pin does not imply Ffb/native/input adoption. Recording
  lives under `vendor/recording/` with its own source and binary provenance.
- **Woden's Recording extension is separately pinned too.** Its
  `lib/recording/provenance.json` explicitly identifies unpublished source; a
  v0.12.0 toolkit pin does not make that extension part of the published package.
- **`OutRun2006Tweaks-FFB` must ship `WheelFfb.dll` beside `dinput8.dll`** in
  the game folder. CMake copies it into `build/bin` after every build, but a
  deploy that copies only `dinput8.dll` silently loses all force feedback. It
  is also the only **x86** consumer: `-Parts native-x86`, not `native`.
- **`art-of-sim-rally` renames the native DLL** to `UnityForceFeedback.dll`,
  the name the game's own missing library had and which the mod P/Invokes.
- **`milestone-wheel-tools` compiles `forza_packet.h`** with MinGW, not MSVC.
  Its `build.sh` refuses to run if `lib/toolkit` is missing.
- OutRun, Milestone and Cruisn **compile headers in**, so bumping a header's pin
  without rebuilding does not update the executable. Unity mods ship managed
  and native DLLs, but any consumer code calling a newly adopted API still needs
  a rebuild. Always inspect the final package rather than assuming a pin file
  establishes deployment.
- **`profiles/force-profiles.ini` is the exception to all of that.** It is read
  at runtime from beside the game binary, so a new tune reaches a player by
  copying one text file. That is the whole point of it: tuning force feedback
  means driving A, driving B and deciding, and B should not mean a build.

## Updating a consumer

Review component differences and the consumer's current work before choosing an
update. The status tool deliberately avoids issuing a blanket sync command for
split versions, custom vendor layouts, unknown pins or local builds.

```powershell
# For consumers using the normal sync layout, in that consumer's repo:
.\tools\Sync-Toolkit.ps1 -Version vX.Y.Z -Parts <its parts>
# Rebuild compiled headers/API callers, inspect the package, then verify in game
# when attended. Keep the consumer's explicit pin and provenance in the commit.
```

Then add a line to its changelog saying which toolkit version and why. The
point of pinning is that a toolkit change never reaches a game without an
explicit, diffable bump.

### OutRun's native/header override is not a full toolkit adoption

The overnight strict-device-selection change takes the official v0.13.0 x86
`WheelFfb.dll` and matching `wheelffb.h`, while retaining the v0.8.0 model,
profiles, encoder and proxy header byte-for-byte. Replacing the entire toolkit
would also change shaping and is outside that scoped adoption.

- `lib/toolkit/VERSION` remains the baseline pin.
- `lib/toolkit/NATIVE-VERSION` starts with the override **package** version;
  native component version `0.6.0` is a separate field, not that package pin.
- `lib/toolkit/NATIVE-PROVENANCE.json` schema 1 records
  `baselinePackageVersion`, `overridePackageVersion`, `architecture: x86`,
  source/archive identity and `files` entries containing relative `path`,
  `archivePath` and `sha256`. Its two overridden paths are
  `include/wheelffb.h` and `native/x86/WheelFfb.dll`.
- The status tool requires both marker and manifest, matching declared versions,
  the exact two expected paths, and matching SHA-256 values. It reports invalid
  provenance or missing files instead of silently falling back to the baseline.
  Local hash consistency is not an independent archive signature check.
- Baseline diffs omit the overridden ABI header and native output. Override
  diffs include only the native output implementation and that ABI header.
  Neither component's status certifies the other, and comparing against an older
  target never suggests downgrading the override.

OutRun's sync wrapper guards `include`/`native-x86` while this override marker
exists. Replacing the split intentionally requires its explicit
`-ReplaceNativeOverride` path and both groups together; do not bypass the guard
to make a status report green. A future model/profile adoption is a separately
reviewed consumer change.

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
| 2026-09-12 | v0.12.0 + separate unpublished extension | iracing-arcade-wheel | Released native/dotnet vendored; tracked `toolkit.version`; separate Recording source hashes/base revision in `recording-toolkit.json`, DLL under `lib/recording`. Consumer reports net6 integration builds; live recording remained to test at that checkpoint. |
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
