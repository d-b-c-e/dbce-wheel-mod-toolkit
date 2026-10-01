# Shared repository setup, documentation and release checklist

Review draft, 2026-10-01. This checklist aligns actual repository entry points and
separates source verification, package provenance and game/device acceptance.
It is documentation, not an executable installer or a release authorization.

## Canonical identities and source checkpoints

| Private repository / existing ID | Source checkpoint | Setup/release state |
| --- | --- | --- |
| `dbce-wheel-mod-toolkit` / `1356452339` | Audited `master` `068b8a9`; published `v0.15.0` tag targets `358add53` | Existing package recipe; standalone setup-guide draft awaiting review |
| `dbce-triple-screen-toolkit` / `1389789951` | Main `756af45` | Setup guide and canonical optimizer URL corrections promoted; no dedicated package profile found |
| `dbce-triple-screen-optimizer` / `1380781844` | Main `ac300822`; approved setup branch `d1b514b`; toolkit pin `3b776d86` | Read-only verifier/guide published on review branch; no release builder/profile found |
| `dbce-vr-game-optimizer` / `1275539619` | Main `6c76c8f` | Standalone README/setup documentation promoted; no dedicated release builder/profile found |

Retain display names DBCE Wheel Mod Toolkit, DBCE Triple-Screen Toolkit,
Triple Screen Optimizer and VR Game Optimizer. Preserve assemblies, data/settings
paths, pins, versions, licenses, privacy and histories. Toolkit names remain stable;
these four shared projects are separate from per-game `dbce-mods-<game>` products.

## Repository and owner checks

- [ ] Recheck exact remote base/ref before editing or publishing; record review branch,
  changed files and parent approval. Use a complete exact-source tree for build acceptance.
- [ ] Stage only reviewed scope; preserve all owner WIP. Reconcile overlapping edits
  before promotion and coordinate before live-checkout integration. Unrelated dirtiness
  does not stall independently reviewed remote-only source/documentation work.
- [ ] Preserve each consumer's exact toolkit/component pin. Shared ownership does not
  establish consumer migration, installed version or gameplay acceptance.
- [ ] Keep game hooks/loader/UI/runtime acceptance in adapter repositories; keep
  toolkit and optimizer application roles distinct. No runtime combination is implied.
- [ ] Maintain existing weekly coordination; create no new automation.

## Actual setup, build and validation entry points

Run root-level commands only in an authorized isolated tree. A command listing is
not a passed check or permission to apply settings, launch games or use a device.

| Repository | Prerequisites and commands | Limits |
| --- | --- | --- |
| Wheel toolkit | Managed libraries `netstandard2.0`, tests `net8.0`; SDK/restore access. `dotnet build Dbce.Wheel.sln -c Release`; `dotnet test Dbce.Wheel.sln -c Release`. Native work requires Windows/MSVC C++ tools, discovered by existing scripts with `vswhere`. | Keep classic `.sln`, x64/x86 distinctions, and package/component versions separate. No hardware probe in this lane. |
| Triple toolkit | .NET 10 SDK/restore access. `dotnet build src/Dbce.TripleScreen.Core/Dbce.TripleScreen.Core.csproj -c Release`; `dotnet test tests/Dbce.TripleScreen.Core.Tests/Dbce.TripleScreen.Core.Tests.csproj -c Release`. | Core `netstandard2.0`, tests `net10.0`. Geometry conformance is separate from adapter/rendered acceptance. |
| Triple optimizer | Windows/.NET 10 SDK. Initialize the recorded private submodule with `git submodule update --init --recursive`; inspect `git submodule status external/dbce-triple-screen-toolkit`. `dotnet build TripleScreenOptimizer.slnx`; `dotnet test TripleScreenOptimizer.slnx`. | Do not use `--remote`, advance pin `3b776d86`, weaken Git ownership guards or deploy in this lane. |
| VR optimizer | Windows/.NET 10 SDK. `dotnet build src/VrGameOptimizer.sln`; `dotnet test src/VrGameOptimizer.sln`. App/core/tests are `net10.0-windows`. | Existing x64 identity and admin-requesting app manifest are preserved; no driver/settings application or security change is requested. |

The reviewed triple-optimizer setup branch provides these PowerShell 7 commands:

```powershell
pwsh -NoProfile -File tools/Verify-Setup.ps1
pwsh -NoProfile -File tools/Verify-Setup.ps1 -PackageRoot 'C:\absolute\package-folder'
pwsh -NoProfile -File tools/tests/Test-VerifySetup.ps1
```

The verifier reads source layout/SDK listings or framework-dependent x64 output
structure. It refuses incomplete, drive-relative/root-relative/reparse paths and
self-contained metadata; it never installs, builds, restores, launches or applies.
Pin provenance, complete dependency inventory, integrity and functional acceptance
remain separate. Use the reviewed branch explicitly until it is promoted.

Existing developer launch commands are `dotnet run --project src/TripleScreenOptimizer.App`
and `dotnet run --project src/VrGameOptimizer.App`. These are source facts, not actions
performed in this lane. Game settings, display topology and device use remain unchanged.

## Wheel recipe and version audit

At exact remote master `068b8a9`, `tools/package/package.ps1` uses:

```powershell
.\native\wheelffb\build.ps1
.\native\wheelffb\build.bat x86
.\native\dinput-proxy\build_test.bat
.\native\forza\build_test.bat
.\native\forcemodel\build_conformance.bat
.\tools\powershell\Test-DbceWheel.ps1
.\tools\tests\Test-SyncToolkit.ps1
.\tools\tests\Test-NativeBinding.ps1
.\native\wheelffb\test\build_burst_test.bat x64
.\native\wheelffb\test\build_burst_test.bat x86
```

The existing package invocation is `tools/package/package.ps1 -Version <reviewed-semver>`.
It builds native x64/x86, runs its listed checks, builds/tests managed output with
`-p:Version`, and writes `dist/dbce-wheel-mod-toolkit-<version>.zip`. Default label
`0.1.0` is not a latest-release claim. `-SkipNative` requires reused-binary provenance.
Packaging and native/device checks were not run in this documentation audit.

Important distinctions:

- Fresh release/tag metadata confirms package `v0.15.0`, source `358add53`, ZIP
  size `489861` bytes and four assets. Native `0.8.0`/46 exports and hashes are
  recorded in the existing handoff; no binary download/load/rebuild is claimed here.
- Static audited master has native `kVersion=600` (`0.6.0`) and 41 DEF exports.
  Managed project defaults are `0.1.0`. README source `0.12.0`/historical STATE
  `0.13.0` narrative is not a current package/component/consumer identity.
- Master recipe includes Ffb/Telemetry and fake-burst checks. It does **not**
  include Recording or `Test-RecordingPackaging.ps1`. Those are separate live
  owner changes and must not be attributed to this remote base or frozen release.

## Common release evidence

- [ ] Select a clean source SHA/tag and record exact private dependency gitlinks,
  resolved packages/toolchain and separately sourced package/app/component versions.
  SDK/project default `1.0.0` or `0.1.0` is not an approved product release label.
- [ ] Use an existing reviewed packaging recipe, or review a concrete new strategy.
  Triple toolkit and optimizer apps have no established packaging profile in the
  inspected source. A verifier or successful Debug build does not fill that gap.
- [ ] Record actual command, output path, complete payload inventory, hashes,
  dependency notices, install/update/backup/undo instructions and retained settings.
- [ ] Keep source/config tests, native ABI/fake-effect checks, deployed-payload
  verification and attended game/display/physical acceptance as separate evidence.
- [ ] Preserve public-consumer access and licensing boundaries for private toolkit
  source/artifacts. Carry wheel MIT license and applicable notices without changing visibility.
- [ ] Preserve VR THIRD-PARTY-NOTICES: unmodified separate NvAPIWrapper dependency,
  official on-demand UEVR acquisition and separate PresentMon/XRFrameTools paths.
  No new bundling or redistribution claim is made.
- [ ] Independently review publication scope; never force-push or delete history.
  Source-only branches/default promotions remain separate from binary deployment/releases.

## Verification completed and remaining gaps

Complete optimizer base/pinned-toolkit archives matched all 99 Git source blobs.
The reviewed three-file optimizer candidate built with zero warnings/errors and
passed 47 tests plus 16 verifier fixtures. Ordinary SDK-directory access failed;
normal reviewed escalation succeeded without ACL/trust changes. Its source/output
verifier also passed against the real Debug layout; no app/game/device was launched.

Wheel documentation audit checks source paths, native static identity, release/tag
metadata, Markdown links and patch scope. Wheel build/native/package/device checks
remain unrun. Earlier standalone triple-toolkit geometry testing was SDK-blocked;
no newer standalone toolkit test result is claimed from the optimizer's older pin.
VR build/tests/UAT remain unrun. Empty GitHub checks/statuses are not passed CI.

Genuine remaining work: reconcile wheel source/release narrative and owner Recording
work before integration; choose reviewed app/toolkit packaging/version/provenance
strategies; coordinate live-checkout synchronization when owner overlap is resolved.
Historical 34 wheel and 34 optimizer entries, the toolkit test and VR artifacts
remain preserved. Owner pause/handoff, installed consumer pins and acceptance are
not inferred from this checklist.
