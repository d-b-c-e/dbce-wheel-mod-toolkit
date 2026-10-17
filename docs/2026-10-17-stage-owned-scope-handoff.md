# Stage-OwnedProcess caller-variable collision

Assigned by Maze in hcom 226354; narrow file claim accepted by Haru in 226462.
Private worktree: `E:/Source/toolkits/.worktrees/gala-stage-owned-scope-20261017`.
Branch: `gala/stage-owned-scope-20261017`, based on verified canonical
`f9abfcf9dc295a1da63ef7279bdfaa68ece716ea`.

F-Zero run01 / evidence `326a6969` installed successfully, then its caller used
`$owned` for receipt filenames. Dot-sourced helper state shared that ordinary
variable, so cleanup lost the process identity dictionary. The consumer's
`$receiptNames` correction and original run evidence are retained separately.

The shared helper now keeps its six persistent tracking fields in the explicitly
script-scoped `StageOwnedProcessState` object. Ordinary caller variables cannot
replace the identity/precision dictionaries, retained handles or safety latch
through normal assignment or dynamic lookup. This is a collision-resistant
internal name, not a security boundary or new module/framework.

Exported function names/signatures, the caller inputs `sourceRoot`, `outputRoot`,
`result` and `observeExternal`, and output receipt fields are unchanged. The pure
identity/lineage functions and process identity classifier are unchanged. Timeout,
unknown-identity refusal, exact-handle verification and cleanup policy are
unchanged. Existing fixture setup now accesses the named internal state; that
fixture seam was never a documented consumer API.

The added focused regression assigns caller receipt arrays to `$owned` before
dot-sourcing and again before observation/cleanup. It verifies that caller values
survive, child adoption retains CIM precision, cleanup completes and both owned
identities remain in the receipt. It also places caller sentinels in the other
former internal variable names. No actual process is started or stopped.

Validation: `pwsh -NoProfile -NonInteractive -File tools/playback/Test-StageOwnedProcess.ps1`
passed **68 checks**, synthetic snapshots/fake handles only, in approximately
six seconds. Original output: `.local/stage-owned-scope-tests.log`. `git diff
--check` passed. No broad suite, build, game, rig operation or owner/runtime
change occurred; no heavyweight verification was needed.

Owned files are the helper, its existing fixture and this note. Canonical/shared
source and historical pinned helper copies have not been modified by this
isolated candidate. New consumers can adopt a qualified new pin after local
integration; active/frozen recipes keep their existing recorded bytes until their
owner deliberately reconciles them. No global runner rewrites are required.
