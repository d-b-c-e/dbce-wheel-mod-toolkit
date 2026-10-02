# Woden HAT39 shutdown backport — source candidate

Based exactly on unpublished input override `9ad1f64044b57fa05c119697518a47f2a86ba0de`. Adapts independently reviewed `f8f0619b5588f2d11b44f4becd4198775d4a8bcf` to the pre-burst native implementation. Source candidate native version is 0.5.2/502; toolkit base version remains v0.12.0. No tag, package pin or default branch is updated.

Preserves all 39 export names, their Cdecl signatures, GetInputCapabilities bit 0, 32 POV directions, existing force mathematics, profiles and managed APIs. It does not import the v0.13 41-export binary, burst exports, cadence or consumer source changes.

Shutdown requests atomic watchdog stop, stops all effect families under the effect lock, drops that lock, joins to actual worker exit, then releases resources. Public calls serialize across cleanup. The worker uses internal force/zero helpers, avoiding API-mutex/join inversion. Timeout/HRESULT are atomic; logging has an independent mutex and is removed from DllMain. Zero/watchdog now stops all effect families and clears started/demand bookkeeping so the next explicit update restarts effects. This required pre-burst adaptation is included in the fake tests; it is not a gain or cap change.

Inherited tradeoff: join, driver Stop and logging can block indefinitely. Worker-self shutdown fails closed and unexpected wait errors retry while retaining resources. These rare branches are source-reviewed, not fault-injected in the present tests. No safe-to-unload claim under loader lock, hung driver, or unquiesced consumer producers.

Consumer must stop/drain producers, invoke cleanup outside loader/effect locks, and prohibit new calls before unloading. Woden's successful managed binding is process-pinned, but its Unity lifecycle/caller quiescence and driver behavior still require independent review and later acceptance.

Run `native/wheelffb/test/test_shutdown.bat x64` and `x86`: compiles actual production source with fake effects/workers; never initializes DirectInput. A 650 ms delayed worker demonstrates the old timeout defect, stop-before-join, effect-lock availability, retained resources, blocked concurrent output, repeated cleanup/restart, concurrent logging/timeout/HRESULT and all-family zero behavior. Includes all 39 compile-time signature assertions and version/capability checks. Run `test_digital_input.bat` for both architectures: all directions/neutral/boundaries, simultaneous hats and all 161 buffer capacities.

For negative-control reproduction, create ignored `test/legacy/WheelFfb.cpp` from exact `git show 9ad1f640:native/wheelffb/WheelFfb.cpp`, then run `test_shutdown.bat x64 legacy`. Expected exit 1: returnedEarly=1 and releasedEarly=1. That source copy is test-only and never committed or packaged.

Build with `native/wheelffb/build.bat x64` and `x86`. Set WODEN_BUILD_TOOLCHAIN to compiler/SDK identity, then run `node tools/woden-native/Verify-Build.cjs <new-evidence.json>` from a clean checkout. It verifies source blobs, DLL hashes, machine types and exact export sets without loading either DLL. Historical input DLL provenance remains metadata association; fresh builds get their own exact source identity.

Do not copy these binaries into any consumer or installed directory. Independent shared/consumer review, separately versioned package preparation, Unity loading and attended rig lifecycle acceptance remain required. Current force caps/settings and frozen releases remain unchanged; user wheel testing is not ready.
