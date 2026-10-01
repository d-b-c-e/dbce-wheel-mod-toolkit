# DBCE Wheel Mod Toolkit setup and release guide

Source and release-metadata audit: 2026-10-01. Canonical repository:
[dbce-wheel-mod-toolkit](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit),
private, stable ID `1356452339`, default branch `master`. No rename is required.
The audited documentation/source base is `068b8a9d0a915daaf88aee788e04fa801c507fd6`.

## Repository boundaries

The toolkit owns reusable wheel/device output, force-feedback bindings, telemetry,
profiles, diagnostics and consumer tooling. Game hooks, force signals, camera/UI
integration, install lifecycle and gameplay acceptance remain consumer-owned.
The consumer setup standard is [CONSUMER-SETUP.md](CONSUMER-SETUP.md); it describes
game-adapter UX, not an installer for this toolkit.

Related separate private projects are
[Triple-Screen Toolkit](https://github.com/d-b-c-e/dbce-triple-screen-toolkit),
[Triple Screen Optimizer](https://github.com/d-b-c-e/dbce-triple-screen-optimizer),
and [VR Game Optimizer](https://github.com/d-b-c-e/dbce-vr-game-optimizer).
Shared names or ownership do not combine their releases or dependency pins.

Read [STATE.md](STATE.md) first and [CONSUMING.md](CONSUMING.md) before adoption.
Preserve the repository's [MIT license](../LICENSE), private access boundaries,
consumer pins and existing weekly coordination.

## Version and source identities

| Identity | Verified source | Meaning |
| --- | --- | --- |
| Published package `v0.15.0` | Fresh release metadata and annotated tag resolve to `358add53af220eb95bf606e240b230e23a6ea197` | Published runtime source, separate from this audited default-branch snapshot |
| Published native `0.8.0`, 46 exports | [Release handoff](RELEASE-0.15.0.md) | Recorded component identity for the frozen release; not inferred from the default branch |
| Audited master native `0.6.0`, 41 exports | `WheelFfb.cpp` has `kVersion = 600`; `WheelFfb.def` lists 41 exports | Static source/ABI facts at `068b8a9`; no binary/device load performed |
| Managed project defaults `0.1.0` | `Dbce.Wheel.Ffb.csproj` and `Dbce.Wheel.Telemetry.csproj` | Project defaults; package script can supply `-p:Version`, so these are not a published-package label |
| Package parameter default `0.1.0` | `tools/package/package.ps1` | Default output label, not the latest release or a recommended new version |
| README's source `0.12.0` and STATE's historical `0.13.0` snapshot | Existing narrative documentation | Historical feature context; do not equate either with current release, native ABI or installed consumer identity |

The release has four assets: ZIP, ZIP SHA-256 sidecar, payload manifest and
provenance JSON. Fresh API metadata confirms names and ZIP size `489861` bytes;
the hashes in the handoff are recorded evidence. This audit did not download or
rebuild binaries. Reproduce a release from its exact tag, retain its artifacts,
and compare consumer payload/component pins separately. Do not silently repin.

## Existing build and test entry points

Managed projects target `netstandard2.0`; tests target `net8.0`. Use an SDK able
to build those targets and restore their NuGet dependencies. From the root:

```powershell
dotnet build Dbce.Wheel.sln -c Release
dotnet test Dbce.Wheel.sln -c Release
```

Keep the classic `.sln`. Native builds require Windows/MSVC C++ tools; the scripts
locate Visual Studio with `vswhere`. Existing entry points include:

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

These paths and commands were checked against the audited source. They were not
run by this documentation audit. Device smoke/probes, torque output, games and
installations are outside this audit; do not treat these listings as permission
or evidence for hardware acceptance.

## Existing packaging recipe

The existing `tools/package/package.ps1 -Version <reviewed-semver>` builds native
x64/x86, runs its listed checks, builds/tests managed assemblies with the supplied
version and creates `dist/dbce-wheel-mod-toolkit-<version>.zip`. It writes a
`VERSION` file containing label, source SHA and timestamp. It also includes the
existing header/profile/tool/documentation/license payloads. `-SkipNative` reuses
binaries and therefore needs their exact provenance; it does not prove identity.

At this remote base, the recipe includes Telemetry/Ffb and fake burst checks,
not a Recording assembly or `Test-RecordingPackaging.ps1`. The live owner checkout
contains separate Recording/package work. Do not mix its recipe or payload claims
into this remote snapshot, the frozen `v0.15.0` archive, or consumer adoption.
No packaging, rebuild, install, sync or new release is performed here.

## Review and release evidence checklist

- Record clean source SHA/tag and package/component versions independently.
- Record actual packaging command, dependencies/toolchain, payload inventory and hashes.
- Separate managed/config tests, native ABI/fake-effect checks and hardware/game acceptance.
- Preserve consumer-owned setup/undo/settings and exact pins; version examples in
  older CONSUMING notes are not instructions to downgrade or update all consumers.
- Reconcile overlapping edits before remote promotion, and coordinate owner work
  before live-checkout integration. Unrelated WIP alone is not a source-review gate.
- Keep public-consumer private-source access explicit; carry applicable license notices.

The missing organization artifact at this base is a standalone repository guide,
not a new runtime or installer. This guide fills that documentation gap. Changes
to version defaults, Recording integration, native APIs or release recipes require
separate source review; none are included here.
