# Shared reader preserves its original open failure

Kobe235659 reported iRacing's vendored SharedRunnerIo reader cascading null
Dispose errors when Open failed under a non-Stop caller preference. Directly
calling canonical5e70915 `Read-StageSharedText` on a missing file under Continue
reproduced FileNotFoundException followed by InvokeMethodOnNull and a null-stream
constructor error. No game or device was used.

The production function now sets ErrorActionPreference=Stop in its own local
scope. Failed Open immediately unwinds with its original IO error, before null
reader construction or disposal. The caller's preference is retained. Existing
delete sharing, bounds, successful reader/stream disposal and atomic writer are
unchanged. This is not an IO retry or a missing-file-as-success fallback.

The existing `tools/playback/Test-StageSharedText.ps1` adds six real inert-file
cases: missing and exclusively locked files, each under Continue,
SilentlyContinue and Stop. Every case requires the original expected base IO
exception, no error/data output cascade, and unchanged caller preference.

The focused fixture passed22 checks on October17, including the retained16
sharing/disposal/atomic-publication cases:40 concurrent publications, legacy
reader failure code5, successful retry and bounded persistent denial. No
compiler/build, runtime, game/window/device/provider/input action occurred;
the tiny local-file fixture held no rig lease. Its fresh temporary root was
verified under the intended temp directory and removed by existing cleanup.

Private worktree/branch:
`E:/Source/toolkits/.worktrees/mivu-shared-reader-errors-20261017`,
`mivu/shared-reader-errors-20261017`, based on5e70915. Claims are only the reader
function in Stage-RunnerLifecycle.ps1, its existing focused fixture and this
handoff. Consumer owners adopt exact production function bytes into their
existing runner with provenance. No game launch or current-run repin follows.
