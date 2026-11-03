# Raw snapshot branch reconciliation — November 2

Merge `abd0538a19581017982101db92e3342e606e16c4` integrates all eight commits
from retained branch `zari/revo-raw-snapshot-regression-20261027`, tip
`6a761e59b6576a63f1e2239eef68437d6bb9aa52`, onto master baseline
`fd2976e3b7e652a7dd0f9e1ee23f79ed14efa8af`. GitHub visibility was PUBLIC;
the owner approved pushes this session, and the coordinator received the exact
merge/results before publication. This is shared source and offline qualification;
no consumer is activated or updated.

## Merge decisions

Only `dbce_fence.hpp` and `test_fence.cpp` required conflict resolution.
Keep master's macro recording, background-controller cooperative-level behavior
and newer XInput force fence. Add the branch's format-checked panel suppression,
optional raw snapshot store and bounded message trace. The cooperative-level
forwarding is now enclosed by snapshot invalidation without changing its existing
flags/fallback policy. Keep current F7 compare handling and all current overlay,
camera-action and frame-contract code.

The fake-device fixture combines the branch's A/W signatures and reported identity
controls with master's device type/cooperative flag witnesses. The legacy raw-state
variable was renamed from `small` to `legacyState` because Windows headers define
`small` as a macro. The isolated panel fixture gained fake implementations for
master's newer CompareAvailable/CompareFromKey dependencies and checks the actual
F7 handler's unavailable, first-down, repeat and release behavior. No product edit
was made to fix these fixture failures.

## Registered CPU-only qualification

All commands used named remi leases, one compiler process at a time. Fake COM and
Win32 calls only; no real queue, window, device, discovery, acquisition or output.

| Existing command | Result |
|---|---|
| `native/dinput-proxy/build_ffb_layer.ps1` | 20 effect-gain checks; 374 fence checks per x64/x86; 221 actual production-hook checks per x64/x86; production layer compile and x86 layer/panel/overlay compile. PASS. |
| `native/panel/build_test.ps1 -InputCustodyOnly` | Per x64/x86: 16/16 cursor-custody checks, 193 message checks, zero failures. PASS. |
| `native/dinput-proxy/build_xinput_fence_test.ps1` | Per x64/x86: 19/19 checks against fake XInputSetState. PASS. |

The first layer batch failed compiling the `small` fixture variable. The second
layer batch passed and its results were retained. Panel qualification first found
missing F7 fake functions, then a newly added fixture assertion accidentally
resolved `handle` to main's existing HWND lambda. Explicit production namespace
qualification corrected the assertion. Final panel and XInput runs pass. Failure
logs remain alongside successful logs; no failed run is reported as a pass.

GO/lease intervals: 498079 / `05ac4bcd`, 498101 / `1efba1f7`,
498122 / `d69bd96c`, 498145 / `2407d631`. All released immediately after each
batch, including failures. No process or rig custody remains.

Evidence worktree: `E:\Source\_work\remi-revo-snapshot-reconcile-20261102`.
Ignored `artifacts/remi-revo-reconcile-20261102/` holds `ffb-layer.log`,
`ffb-layer-retry.log`, `panel-input.log`, `panel-input-retry.log`,
`panel-input-final.log`, `xinput-fence.log` and `result.json` (source identity and
file hashes). This checkpoint supersedes UNRUN wording in the original branch
documents **only for these offline checks**.

## Remaining gates

No package, emitted consumer ABI/release, vendoring, installation, invocation seam,
real message/provider delivery, calibration admission, multi-span patch quiescence,
rendered interaction, gameplay, device behavior, physical force or owner acceptance
is established. Revo must adopt exact reviewed files/provenance separately; retain
its current installed candidate and original runtime evidence. No broader native
wheel DLL or hardware smoke was needed or performed: wheelffb source is unchanged.
