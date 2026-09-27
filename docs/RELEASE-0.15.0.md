# Toolkit 0.15.0 release handoff

Published **2026-09-27 05:59:03 UTC** as
[v0.15.0](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/releases/tag/v0.15.0)
to unblock Art of Sim Rally's authorized 0.2.6 release. The annotated tag resolves
to `358add53af220eb95bf606e240b230e23a6ea197`, the clean
`codex/finite-constant-bursts` source. Native component **0.8.0** has **46 exports**.
The original archive was promoted unchanged, with no native or managed rebuild.

The default branch contains newer UX documentation but does not contain this
runtime branch. Build/reproduce this release from its tag, not from the current
default branch. Candidate/unpublished wording in the frozen archive describes
preparation status on September 16; the public release supersedes that status.
Recording and consumer UI implementation remain separate work and pins.

## Included changes

Since v0.13.0, the release adds opt-in per-update force tracing, shaped finite
sine bursts with managed capability detection, explicit legacy envelope clearing,
and an independent finite constant-burst pool with optional managed binding.
Use the tagged [constant-burst contract](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/v0.15.0/docs/CONSTANT-BURSTS.md),
[shaped-burst contract](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/v0.15.0/docs/SHAPED-BURSTS.md),
and [logging contract](https://github.com/d-b-c-e/dbce-wheel-mod-toolkit/blob/v0.15.0/docs/FORCE-LOGGING.md)
for API details and physical-verification limits.

## Frozen release identity

| Published asset | SHA-256 |
|---|---|
| `dbce-wheel-mod-toolkit-0.15.0.zip` (489861 bytes) | `1676B31AFBB2273B23140D042BC311F1F523240E2661D2C61E7932F74DCFF6F6` |
| `dbce-wheel-mod-toolkit-0.15.0.manifest.sha256` (63 payload files) | `29D82239FE2CF845892F3EB61C122705C09232A192A036246B85B7A6355A27AB` |
| `dbce-wheel-mod-toolkit-0.15.0.zip.sha256` | `DCDD41FCBB6F9E407E4DE20056D9B11E46FAC3CAE6AFC88A2B5A426C0FE4DE6D` |
| `dbce-wheel-mod-toolkit-0.15.0.provenance.json` | `249546E92A2960E7E09480064FBB163A0996B0D4C78BFFE73083A666F0AB7285` |

All four assets were re-downloaded after publication and matched their local
hashes. The published tag target was checked independently with `git ls-remote`.

## Art repin verification

Art's current `tools/Sync-Toolkit.ps1 -Version v0.15.0` was run with an isolated
`-Destination` copied from its existing
`local+358add53af220eb95bf606e240b230e23a6ea197.clean` pin. Both the draft and
published download paths passed. These five payloads remained byte-identical:

| Vendored file | SHA-256 |
|---|---|
| `dotnet/Dbce.Wheel.Ffb.dll` | `6FC2BE9197D846A477C84E81D39DD420B324BD4B79B30CD54BB78567E270A72B` |
| `dotnet/Dbce.Wheel.Ffb.xml` | `CC18CB06264D981B54F9E546A7FCF64183EF4E7C72CA31606A6CF930EAD21F21` |
| `dotnet/Dbce.Wheel.Telemetry.dll` | `293D09062B7B1FA9FCB7D48B70A6D8F8BFF2F22577EE90487786800F8D76373F` |
| `dotnet/Dbce.Wheel.Telemetry.xml` | `4C4F8099759685B3DA213972C6354E19D4F29DF46822BBFA19D485B7C27D9D2F` |
| `native/WheelFfb.dll` | `A07DDF7E10ADBD016DB204324D5E035B951E21A7D8DD373405C91257EB7AD288` |

Only `VERSION` and the `MANIFEST.txt` header changed in the fixture. Its new
`MANIFEST.txt` SHA-256 is
`6A69ACEBC0ECED35E55A47AE01128F748252D29B6EE41C433FB30C26A709E49D`.
This is the **consumer's five-file manifest**, distinct from the release's
63-file payload inventory. The VERSION timestamp changes on each sync.
The script SHA-256 was
`4EC414B454DAF791EA161514C6CD992D0EF27C43C8C7888462FB8695E4384634`.
All seven files in Art's actual vendor directory were verified unchanged through
the final publication check at 05:59:50 UTC. Art's owning task received the tag,
hashes and repin result; it owns the real repin, consumer build and release.

## Validation and limits

Original frozen-package evidence records 121 managed tests, 24 sync assertions,
104 current/9 legacy/8 burst-only/8 shape-only binding assertions per runtime,
x64/x86 fake-effect tests and model/packet/proxy/PowerShell helper suites.

Publication checks reran all 121 retained managed tests without rebuilding,
after comparing their runtime dependencies to the packaged DLLs. Retained x64
and x86 executables passed finite periodic, finite constant and four logging
modes. An initial logging invocation omitted its required mode and failed its
argument assertion; corrected `off`, `on`, `master-off` and `invalid` runs passed
on both architectures. No production-code repair was required.

Both packaged native DLLs loaded by absolute path, resolved all 46 exports and
returned version 800 in matching 64-bit/32-bit processes. This check did not
enumerate/acquire devices and is **not a hardware smoke test**. The original
candidate's hardware smoke harnesses were compiled but not run. Physical force
shape, expiry and feel are separate consumer/device acceptance, not established
by these offline checks. No game was launched or hardware output requested for
publication. No workflow was dispatched; publication used the frozen artifact.

Detailed local receipts and logs are retained under the documentation worktree's
ignored `dist/release-0.15.0-2a8bbd61f05b409a907f43af14f7f539/` directory, including
`published-verification.json`, original gate logs, fresh tests and both sync logs.
The public provenance asset retains the portable source/hash/check summary.
