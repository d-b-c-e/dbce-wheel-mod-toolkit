# Impact and legacy-input lifecycle candidate — October 8

Unpublished native **0.9.1, 48 exports**, based on **1f7a971** (the reviewed
shutdown/lock publication and actuator-rearm merge, before external-session
APIs). Source/fake-device qualification only; no consumer pin, install, hardware
acquisition or physical force. The worktree/branch still contains `47` in its
name because the Woden incompatibility below was found during qualification.

## Why this is separate from HCT's 41-export pin

HCT's reviewed **50ba139**, tagged `native-0.6.0-retained-identity`, preserves its
41-export native 0.6 ABI. Art and DRIVE require shaped sine and finite constant
impacts. Woden requires its input override. Neither the smaller HCT binary nor
a numerically newer release is a safe substitute without checking those APIs.

This candidate retains all 47 exports from 1f7a971, ports the reviewed strict
retained-device contract, and adds Woden's `GetInputCapabilities` / 160-button
legacy input contract from **9ad1f640**. The 55-export external-session prototype
is deliberately a separate line; no external-session exports are included here.

### Strict retained identity

If a strict preferred GUID is cleared or differs from the retained force device,
`InitDirectInput` stops/releases every effect family and the device, clears the
identity, and returns failure. A later explicit call may open the requested
device. It preserves the watchdog, exit guards, DirectInput instance and input
read slots, including dormant borrowed readers which resume only on their GUID.
Same-GUID and non-strict reuse are unchanged. The fixture includes a live fake
constant impact alongside steering, periodic and condition effects.

### Preserve Woden's actual input contract

Woden's existing managed DLL probes `GetInputCapabilities` bit 0. It reads 128
physical buttons plus 32 exact hat directions through `ReadDeviceState`; it
does **not** consume `ReadDeviceStateWithPov`. The original 47-export candidate
loaded successfully through that managed DLL but reported `SupportsPov=false`.
That negative control demonstrates why export/version checks alone are not
consumer qualification.

The compatibility export now reports bit 0 and the legacy read writes only
`min(buttonCount,160)` bytes, keeping 128-byte callers unchanged. Hats omitted by
a driver are initialized neutral. Failed legacy reads still leave output buffers
untouched; consumers must honor the return code. The newer raw POV snapshot
retains its existing coherent-read, explicit-count and failure-clearing contract.

## Offline evidence

Both x86 and x64 pass the following production-source / fake-COM checks:

- Retained identity: 58 checks, including all four effect families, worker and
  exit-guard retention, missing/failed strict targets, reader-first and borrowed
  readers. On unchanged 1f7a971 the test fails at the first mismatch refusal.
- Burst: concurrent first lock publication, 400 empty releases, stop before join,
  delayed worker order, restart, concurrent diagnostics, focus/hold behavior;
  the separate `--pre-init-worker` mode passes too.
- Constant impacts: signed axes, finite expiry, errors, independent steering and
  periodics, focus/watchdog and full shutdown.
- Constant recovery: `loss`, `recreate`, `recreate-reject`, `boundaries`, `cancel`.
- Input: production 160-button legacy and raw POV routes; four hats/eight
  directions, neutral/invalid/missing hats, count canaries, rounding boundaries,
  single-snapshot reads, failed reads, borrowed-device identity and existing
  actuator acquire/rearm checks.
- Logging: 6,000 production requests in each mode; tracing remains opt-in.
- Actual DLL: all 48 cdecl exports, version 901, capability bit 0, empty cleanup,
  worker restart and 32 unload cycles. No enumeration or initialization calls.

Run the existing `native/wheelffb/test/build_*_test.bat` scripts for
`retained_identity`, `burst`, `constant_burst`, `constant_recovery`, `pov_read`,
`logging`, and `lifecycle`, with `x64` or `x86`. `constant_recovery` is build-only;
run its executable with each mode above. Build the DLL before the lifecycle test.
The three existing unused header-helper warnings and x86 logging signedness
warning remain; the retained-identity fixture builds with `/W4 /WX`.

The actual Woden vendored managed DLL also loads this x64 DLL and reports
Version 901 / SupportsPov true / Ready false. Only module resolution and read-only
version/capability calls were made. The baseline 47-export DLL reports version
900 / SupportsPov false in an independent process.

| Architecture | SHA-256 of qualified DLL |
|---|---|
| x64 | F8361A08BBE08EAAB936B37800D308CF1FC7EDAB34E2839DB16A447FE34178C5 |
| x86 | 1653480E4E4E920DAC98CBE2BA15CAA702F68A0FE1DCCDFD4B948AD942EFF485 |

## Before adoption

Independent source review is pending. Consumer-specific provenance, package
tests and muted runtime checks are next. This is a lifecycle/input update, not
a new force tune, but the delta from Art's native 0.8 baseline includes prior
recovery and actuator-rearm work. It cannot be called physically identical from
these tests. Keep Art's reference arithmetic and every saved setting fixed;
attended force/recovery checks remain after any consumer adoption.
