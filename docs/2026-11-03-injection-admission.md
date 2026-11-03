# Consumer admission for test injection

Assignment505653, isolated branch `soba/injection-admission-20261103`, base
bffbd68. Opt-in source change for DR2; no rollout to other consumers, devices,
game, owner state, package or public release.

`InjectionTable` accepts an optional callback/context at construction, before
publication. A valid command must pass it before entering the table. Delivery
checks it before range queries, after external device callbacks and after staging
the result, immediately before copying samples back to the caller. The callback
must not reenter the table; external range callbacks remain outside its mutex.

A callback denial/exception or explicit revoke drops all queued objects and
permanently refuses further commands for that table's lifetime. Foreground
return cannot rearm it. The reader polls an admitted session every40ms, including
when no pulse remains, and exits on revocation. It does not arm an idle table.
Unavailable process-birth identity refuses the reader rather than comparing an
uninitialized timestamp. Replaced/expired samples are never replayed.

Gated reads stage both samples and delivery logs. If the last consumer check
fails, neither partial samples nor false delivery records reach the caller.
Legacy tables with no callback keep their existing behavior. Existing controls,
device normalization and force-output policies are unchanged.

`test_inject_admission.cpp` uses the actual table with fake admission/range
callbacks: axis/button/hat delivery, pending-pulse cancellation, denial after
range callbacks and after staging, terminal non-rearm, reader polling after
expiry, exception refusal, explicit revoke and same-thread range reentry.
The existing controls build recipe includes it for x64 and x86.

Validation passed under serialized CPU505908 after remi FINAL505982:
**x64 and x86 each422 controls checks (207 shared vectors),136 shaping checks
and61 admission checks**. DR2's exact copied table passed62 witness/table checks
per architecture; a mutant removing all three consumer checks was detected.
The actual DR2 proxy compiled through existing32 XInput and68 DirectInput A/W
checks, including the force-forwarding negative control. No DLL rebuild or
package replacement. Existing45 project file/policy checks passed too.

Owned job/retained handles quiesced, no forced stop, named CPU lease released
after34.076s. Evidence at
`E:/Source/_work/soba-dr2-adoption/build/injection-505908`:
result SHA256B165B47F9B1B705BC939A8EC105D768D6B5B0DFFFE11119792BA297870F81C47;
exact tested source pins SHA256F353B819B7995026A4ADBBB66F6EBE399BC1B1AF6C15454C4D98DBEC097D6731.
No runtime qualification follows. The callback sees observations at its sample
points; it cannot establish absence of a transient foreground change between
those observations. Consumer-specific OS identity, observer, lease, terminal
signal publication and cleanup qualification remain separate.

Cleanup limitation corrected after the run: the reused helper's job census
observed only its packaged-WindowsApps pwsh root, not proven SDK/compiler/test
descendant membership. Empty job and zero retained root handles are literal
observations, not complete containment qualification. A fresh independent scoped
process audit found all three original soba CPU roots absent and no matching
compiler/test/script processes in the three owned worktrees. Audit SHA256
84896940EE5982E8B146CDA83083DAD8113B671F7040140624FA01C89D14F716,
retained with the DR2 evidence. No processes were terminated and passing tests
were not rerun to conceal this limitation. Future use needs proven membership
or an independently scoped descendant audit before release.
