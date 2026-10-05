# Native force logging (0.13.1 local candidate)

Routine `SetDeviceForcesXY` tracing is now off by default. Initialization,
lifecycle and existing bounded error logs remain on. Force arithmetic, effect
updates, recovery and lifecycle are unchanged; native component 0.6.1 retains
all 41 exports. This is a local test candidate, not a published release.

To diagnose individual force requests, set `DBCE_FFB_TRACE_FORCE=1` in the
**game process environment before launch**. Only the exact value `1` opts in.
The setting is cached once per process; restart to change it. A variable set in
a shell will not reach a game launched through an already-running Steam client.
`DBCE_FFB_LOG=0` still disables all native logs, including force traces.
`DBCE_FFB_LOG_PATH` and `SetLogPath` still select the log location. The consumer's
managed DiagnosticLogging checkbox does not control these native switches.

Tracing writes synchronously to disk and may affect timing. Use it only for
short diagnostic captures. The default path skips the logging call entirely;
it does not format force samples, resolve a path or open a log file per update.

`native/wheelffb/test/build_logging_test.bat` tests x64 (pass `x86` for x86).
It includes the production adapter and runs 6,000 alternating force values
against a fake COM effect without creating a DirectInput device. It verifies
exact force/direction/flags, identical-force updates, zero trace lines by
default, 6,000 lines with opt-in, invalid opt-in rejected, and master-off
suppression. Lifecycle/error log markers remain visible with default logging.
Timing is reported for comparison only, without flaky performance thresholds.
The test runs in the local package gates.

The art-of-sim-rally 0.2.5 T300/Finland early-stutter report motivated this
change. Avoidable synchronous driving I/O is confirmed in the old source;
causality for that report is **not established** without the reporter's capture.

## Candidate evidence — 2026-09-16

Full offline package gates passed: 121 managed tests, 24 sync assertions,
63 current and 8 legacy binding assertions per runtime, native burst and
logging fake-effect tests x64/x86, existing packet/profile/proxy/helper suites,
and native builds with 41 exports. Initial x64 logging comparison: 6,000
alternating production updates took 0.110 ms without tracing and 643.469 ms
with tracing; all modes delivered identical force parameters. Timings vary
with filesystem/cache state and do not predict game frame times.

Zero-output hardware smoke was attempted on both architectures. All exports
loaded and version 601 was returned, but enumeration found no FFB device
(only shifter/stalk), so hardware stages **did not run**. No torque was applied.
The candidate is ready for consumer testing, not hardware-validated release.
Managed source is unchanged; local package assemblies are rebuilt with the
candidate package version 0.13.1 and therefore differ from v0.13.0 binaries.
