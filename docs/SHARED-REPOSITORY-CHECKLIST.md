# Shared repository setup, documentation and release checklist

Reconciled 2026-10-02. This checklist separates source verification, local packages,
published releases and attended acceptance. It is not an installer or release authorization.

## Canonical identities and source checkpoints

| Private repository / existing ID | Exact source checkpoint | Setup/release state |
| --- | --- | --- |
| `dbce-wheel-mod-toolkit` / `1356452339` | Master `80ab49031b1968f930fdea1887ae07eed86beaab` | Setup guide promoted; existing package recipe. Native/managed hosted checks passed. Published v0.15.0 remains separate source `358add53af220eb95bf606e240b230e23a6ea197`. |
| `dbce-triple-screen-toolkit` / `1389789951` | Main `44c45bbb41a772db901b44c99c3cb6bbf84175ef` | Setup and seven-test geometry qualification documented; no dedicated package profile or hosted workflow. |
| `dbce-triple-screen-optimizer` / `1380781844` | Main `d49e2c986f2c4e6559911805fb90ff676d25f6c9`; gitlink `3b776d86f7da93b5366b3e53b1b764436f6a590c` | Setup verifier and reviewed portable-builder source promoted. Local private preflight archive only. |
| `dbce-vr-game-optimizer` / `1275539619` | Main `a3243158e0afe3ce446052f5cebf1dd35da71cf1` | Setup, SteamVR discovery isolation and reviewed portable-builder source promoted. Local private preflight archive only. |

Retain DBCE Wheel Mod Toolkit, DBCE Triple-Screen Toolkit, Triple Screen Optimizer
and VR Game Optimizer display names. Preserve assemblies, settings paths, pins,
licenses, privacy and history; these remain separate from per-game products.

## Repository and owner checks

- Recheck exact remote base/ref, review scope and independent approval.
- Stage only reviewed scope; reconcile overlapping edits before promotion and
  coordinate before live-checkout integration. Unrelated WIP does not block
  independently reviewed remote-only work.
- Historical inventories recorded 34 wheel and 34 optimizer entries, one changed
  triple-toolkit test and VR local artifacts. These are historical observations,
  not fresh complete statuses. Old wheel/optimizer owner handoffs remain unverified;
  no owner WIP enters these canonical checkpoints.
- Preserve exact consumer/component pins; publication does not prove installed
  versions, migration, runtime acceptance or owner handoff.
- Keep game hooks/loader/UI/runtime acceptance adapter-owned. Maintain existing
  weekly coordination; create no new automation.

## Setup, build and qualification

Use authorized isolated trees. Command listings do not authorize device/runtime work.

| Repository | Entry points | Verified limits |
| --- | --- | --- |
| Wheel toolkit | `dotnet build Dbce.Wheel.sln -c Release`; `dotnet test Dbce.Wheel.sln -c Release`; Windows/MSVC scripts use vswhere. | Libraries netstandard2.0, tests net8.0. Master run [36985039001](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/actions/runs/36985039001) passed native/managed jobs. Checkpoint-copy fixtures separately passed x86/x64, each 264 continuations and 462 rejections. No device/gameplay acceptance. |
| Triple toolkit | `dotnet build src/Dbce.TripleScreen.Core/Dbce.TripleScreen.Core.csproj -c Release`; `dotnet test tests/Dbce.TripleScreen.Core.Tests/Dbce.TripleScreen.Core.Tests.csproj -c Release`. | Core netstandard2.0, tests net10.0. Base `756af45d64724322af67734bf2fa0356d22c2ac8` passed seven tests and clean Release build; later main changes are documentation only. SDK 10.0.401; no adapter/render acceptance. |
| Triple optimizer | `dotnet build TripleScreenOptimizer.slnx`; `dotnet test TripleScreenOptimizer.slnx`; `tools/Verify-Setup.ps1`; `tools/tests/Test-VerifySetup.ps1`. | SDK 10.0.401 pinned; 47 unfiltered tests and 16 verifier fixtures passed. Initialize the exact private gitlink without --remote. No hosted workflow. |
| VR optimizer | `dotnet build src/VrGameOptimizer.sln`; `dotnet test src/VrGameOptimizer.sln`. | SDK 10.0.401 pinned; 72 safe unfiltered tests passed. Windows x64/admin-requesting manifest retained. No app/settings/process/VR/device acceptance. No hosted workflow. |

Ordinary sandbox SDK access initially failed before compilation; bounded qualification
then passed through approved access without ACL/trust changes. The standalone triple
suite is no longer SDK-blocked and remains distinct from the optimizer's older pin.
Empty hosted statuses do not mean passed CI.

Both apps now include `tools/package/Build-Portable.ps1`, source-manifest and
package verifiers, locks/notices and portable instructions. Use the exact-source
[TSO guide](https://github.com/d-b-c-e/dbce-triple-screen-optimizer/blob/d49e2c986f2c4e6559911805fb90ff676d25f6c9/docs/portable-package.md)
or [VR guide](https://github.com/d-b-c-e/dbce-vr-game-optimizer/blob/a3243158e0afe3ce446052f5cebf1dd35da71cf1/docs/portable-package.md),
absolute paths and documented staging/SDK phases. PowerShell 7.6.5 is qualified;
Windows PowerShell 5.1 is unqualified.

| Local private preflight | Label / payload | ZIP SHA-256 |
| --- | --- | --- |
| Triple optimizer | `0.0.0-preflight.1`; 84 files | `9a6e5eef7d67f74de77aaa80f25e311344802f37dd4dcbd2444bd1c3c7f5a6f6` |
| VR optimizer | `0.0.0-preflight.1`; 84 files | `5e7d9bf50cc2f14aa9af3be11739e7263d8649cd7c18bc3c92de38150677e4af` |

Two clean staging builds per app matched payload and ZIP bytes on the same Windows
host, SDK and existing NuGet cache. Cross-host/fresh-cache reproducibility is unqualified.
Archives remain local/private/unreleased/undeployed. Triple toolkit still needs its
own dedicated package strategy. Reviewed builders are not binary releases.

## Wheel source and release distinctions

Master native remains kVersion=600 (0.6.0), 41 DEF exports; managed defaults 0.1.0.
Published v0.15.0 uses separate frozen source 358add53 (native 0.8.0/46 exports),
four assets and a 489861-byte ZIP. [Release evidence](RELEASE-0.15.0.md) records
its hashes; that archive was not rebuilt here.

Existing `tools/package/package.ps1 -Version <reviewed-semver>` builds native
x64/x86, runs listed offline checks, builds/tests managed output with the supplied
version and writes `dist/dbce-wheel-mod-toolkit-<version>.zip`. Default 0.1.0
is not a latest-release claim; -SkipNative needs exact reused-binary provenance.
The master recipe includes Ffb/Telemetry and fake-burst checks, not the live
owner's Recording package work. The checkpoint-copy fixture is separately invoked:
it is not a release gate, public API, external codec or consumer runtime adoption.

## Reviewed consumer source publications

These are product source identities, not toolkit dependency pins, released package
versions, deployed payloads or visually validated support. Preserve the
[component register](CONSUMERS.md) and each adapter's ownership/evidence.

| Canonical product repository | Exact reviewed source | Evidence boundary |
| --- | --- | --- |
| `dbce-mods-woden-rally-edge` | `95d1ff7ce57eb816a5f6b662b80923a3bec9e2ca` | Source publication; attended input/FFB/triple acceptance remains consumer-owned. |
| `dbce-mods-iracing-arcade` | `ea4ddf912fa791c701ec01491e333ff0eb1af8fd` | Source publication; offline probes do not prove presented views or physical feel. |
| `dbce-mods-sonic-crossworlds` | `b72a44bc6cf8e0bd16975160a1cf493a3836faf2` | Source publication supersedes older unpublished-source wording; loader/render/gameplay acceptance is separate. |
| `dbce-mods-outrun-2006` | `86599699ab3926bef796413d266a9090158383c9` | Hosted run [36989644072](https://github.com/d-b-c-e/dbce-mods-outrun-2006/actions/runs/36989644072) passed at this head. Mod-only build/fixture evidence, not gameplay or a binary release. |
| `dbce-mods-gravel` | `9c84fb7d8f1db5bce6fe398bbac4e7df84c7ea9d` | Hosted run [36989847650](https://github.com/d-b-c-e/dbce-mods-gravel/actions/runs/36989847650) passed at this head. Source/build evidence only. |
| `dbce-mods-drive-rally` | `0d45c3737fe274f6aecf2591607e89bbfccab348` | Source publication; no inferred displayed triple views or game deployment. |

Coordinator-reported F-Zero candidate `14ea520` remains publication-blocked;
GPU validation remains pending. The old replay approval prerequisite is resolved.
This abbreviated reported pin is not a newly verified remote full-SHA publication.
Offline renderer/replay fixtures do not establish visually validated support.

## Release evidence and remaining work

- Record source/tag, dependency gitlinks, packages/toolchain and separate
  app/package/component versions; defaults/preflight labels are not release approval.
- Record commands, payload inventory/hashes, notices, source identity,
  install/update/backup/undo instructions and settings retention.
- Separate source/config tests, native ABI/fake-effect checks, deployed-payload
  verification and attended game/display/device acceptance.
- Preserve private/public access and licenses. VR's separate NvAPIWrapper,
  on-demand UEVR and PresentMon/XRFrameTools boundaries remain unchanged.
- Review publication independently; never force-push/delete history. Coordinate
  live synchronization after owner overlap is reconciled.
- Remaining gates include owner WIP/handoffs, triple-toolkit packaging and explicitly
  authorized app binary releases. These milestones do not mean all projects or
  feature acceptance are complete.
