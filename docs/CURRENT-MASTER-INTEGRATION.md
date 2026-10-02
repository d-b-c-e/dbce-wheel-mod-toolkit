# Native safety integration on current master

This candidate starts at master `54a6e951c945cc653e5ce14728ee4351b954120e`,
preserving its history and all 171 existing files. Its native implementation,
burst test, export DEF and public header match the original native 0.6.0 review
input at `dd0ef20ad0cdaccc7a67f10a707dbd2a27a6efe9` byte-for-byte. The 60
subsequent master commits are retained through the parent relationship, including
the current setup, consumer-provenance and ROM-free replay tooling.

The native implementation and burst test incorporate the previously reviewed
shutdown and one-time lock-publication fixes from
`1760aaa58917fb016c81a8c261ef7b5e040f8fbc` without further source changes.
The version remains 600 (0.6.0), with the same 41 C exports, signatures and
four-byte BOOL contract. The separate native 0.8.0/46-export runtime release
branch is outside this candidate. See [the lock-publication review](LOCK-PUBLICATION-REVIEW.md)
for synchronization, failure behavior and caller lifetime requirements.

The existing shared-contract CI now also runs the source-inclusion watchdog and
concurrent initialization regressions on x64 and x86, including a fresh-process
pre-device worker mode. A new actual-DLL lifecycle harness resolves all 41 exports,
checks version and BOOL size, and repeats 32 load/empty-cleanup/worker-restart/unload
cycles per architecture. It only invokes GetWheelFfbVersion, GetLastHResult,
SetHoldTimeoutMs and FreeDirectInput; it does not initialize DirectInput, enumerate
devices, or send forces. Fake effects and a delayed worker in the source-inclusion
suite exercise resource retention and join ordering separately.

CI also runs the existing offline consumer-status and ROM-free replay fixtures,
so later master tooling is covered alongside managed and force-model contracts.
Local Windows verification includes 121 managed tests, exact-module binding
fixtures on CoreCLR x64 and .NET Framework x86, transactional sync (24 assertions),
offline consumer status (76 assertions), native builds, and both lifecycle suites.
The lifecycle harness emits three unused static-header-helper warnings (C4505).
Replay fixtures require Python; local Python is unavailable and their execution
is delegated to the Ubuntu CI job.

These checks establish offline contracts, not physical-device correctness or
bounded shutdown when a driver or log sink blocks. Callers must quiesce API users
and finish FreeDirectInput before unloading. No consumer pin, binary release,
default branch, security setting or owner checkout is changed by this candidate.
