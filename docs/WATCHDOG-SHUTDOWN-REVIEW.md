# Isolated shutdown candidate — publication held

Base: toolkit v0.13.0, commit dd0ef20ad0cdaccc7a67f10a707dbd2a27a6efe9; native 0.6.0, 41 exports. This is not a replacement for native 0.8.0/46-export builds.

FreeDirectInput now requests atomic watchdog stop, stops constant/periodic/condition effects under g_lock without releasing them, drops g_lock, waits for actual worker exit, then releases resources. Unexpected wait failure retains resources and retries. A watchdog-thread caller cannot self-join or return: it fails closed by blocking. Driver Stop, logging, or worker completion may still block indefinitely; force-kill and timeout-then-free are deliberately absent.

All exported calls use one recursive API mutex, preventing lifecycle changes and output calls from overlapping shutdown. The watchdog uses internal ZeroForces and SetDeviceForces helpers rather than acquiring that API mutex, avoiding API-mutex/join inversion. This widens the patch beyond a join-line change and needs independent review. Consumers must stop producers, drain their calls, call shutdown outside DllMain/loader lock and outside toolkit g_lock, and prohibit new calls before unloading. Serialization does not make calls into an unloaded module safe. Loader-lock context cannot be portably detected here; it remains a caller requirement. Toolkit DllMain does not invoke shutdown or join.

Fake-only tests cover stop-before-delayed-worker-exit for all three effect families, worker acquisition of g_lock while shutdown joins, no release before exit, concurrent output blocked through cleanup, release once, repeated cleanup and watchdog restart. Final x64/x86 suites pass with the self-call guard present. No driver or physical-device acceptance is claimed.

Fzero retained src/fzero_ffb.cpp:290-297 calls PanicStop, releases effects, FreeDirectInput, then WheelFfb_Unload. Consumer producer quiescence and the earlier effect-release calls need independent review; no consumer edit was made.

OutRun retained src/hooks_dinputffb.cpp:599-607 SelectionChanged calls ZeroAllForces, FreeDirectInput, then resets initialization/slot state. Its process-detach Shutdown explicitly calls PanicStop under loader lock (1261-1267) and avoids FreeDirectInput/FreeLibrary. The new API mutex may block this PanicStop path; consumer loader-lock compatibility is an adoption blocker. Neither the switch UI path nor thread quiescence was runtime-tested.

All retained DLLs and consumer trees were inspected read-only; no runtime patch, consumer repin, binary release, deployment or device operation was published.

Further shared-state correction: timeout and HRESULT are atomic; each timeout decision uses one captured timeout value. Log and SetLogPath use an independent recursive logging mutex, protecting cached enablement, path selection and append operations. The logger never takes API/effect locks and the join never holds the logging mutex. Logging was removed from DllMain so no logger wait is added under loader lock. File I/O and logging can still delay a worker and shutdown indefinitely. Concurrent fake-worker log append/path redirection and timeout/HRESULT exercise passes on x64/x86, as do the existing shutdown checks; this is exercise coverage, not race-detector proof. Updated source/test patches and immutable hashes are in watchdog-race-corrected-* artifacts. Baseline stays native 0.6/41 exports.
