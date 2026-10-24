# Toolkit 0.16.0 release handoff

Component release cut on 2026-10-24 so art of rally can ship stable 0.4.2 with its
shared-input controls from an official toolkit pin. It promotes the exact bytes in
the owner-accepted Art 0.4.2 shared-input build (rc30, accepted October 23-24);
nothing was rebuilt.

The tag `v0.16.0` resolves to `bbbf6e3f0cbbcd207383647a8a18fe1d724408e2`
(`mufa/native-091-read-snapshots`), the native source. As with v0.15.0, the
default branch does not contain that runtime line; reproduce from the tag and the
provenance below, not from the current default branch.

## Contents

| File | SHA-256 | Source |
|---|---|---|
| `native/WheelFfb.dll` (x64, version 901, 51 exports) | `44E46799945438BC07DF738472602DB1E3ECC04C3F77CAD5B12FCACF49C69272` | `bbbf6e3`: native-0.9.1 (`c8ec2ee`) plus `GetReadDeviceInfoV1`, `ReadDeviceSnapshotV1`, `GetReadDevicePathV1` from `ecc49bd`/`64ccf67` |
| `shim/dbce_shim.dll` (x64, /MT, clang 19.1.5) | `BC9B030A25834D8CCE562F22588EC5E0D3F5C1FA026AD7AD984776D60CD6254B` | `421bd4b` native/shim, controls, session (on the default branch; `c6594fa` adds docs only); base ABI 2, bridge extension v1 |
| `shim/dbce_shim.h`, `shim/dbce_shim_bridge.h`, `shim/build-receipt.json` | see the release manifest | headers as vendored by Art; the shim's original build receipt |
| `dotnet/Dbce.Wheel.Ffb.*`, `dotnet/Dbce.Wheel.Telemetry.*` | unchanged | byte-identical to v0.15.0 |

The archive has no x86 native, profiles, PowerShell module or tools; consumers that
need those stay on v0.15.0. `Sync-Toolkit.ps1` consumers select `native,dotnet,shim`.

## Evidence and limits

These bytes are the ones the owner drove in Art rc30 (driving, force, performance,
telemetry, H-pattern and separate-monitor triples signed off). Art's package
verifier and installer suites cover the packaged copies. No new native or shim
test was run for this promotion, and it is not a hardware qualification for other
games.
