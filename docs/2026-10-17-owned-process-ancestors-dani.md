# Supplemental process scope: runner ancestors

Private branch `dani/owned-process-ancestors-20261017`, based on canonical
`fe490b95`. Claims: `tools/playback/Stage-OwnedProcess.ps1` and its existing
`Test-StageOwnedProcess.ps1`. Haru accepted ownership coordination in 220582.

iRacing runtime 220073 showed a completed outer tool shell matching `sourceRoot`
in its command line. That shell was a runner ancestor, not an owned game child.
The shared observer now verifies the runner and its ancestors against the same
complete process census, then excludes those ancestors only from supplemental
text matching. Owned lineage remains authoritative, including an owned ancestor.
Unknown creation, duplicate identity and cycles refuse; newer reused parent PIDs
are never excluded. No termination, lease, device or caller API change.

The existing synthetic fixture passes 52 checks. Its first run exposed a fixture
expectation error: making an ancestor owned also correctly adopts the synthetic
runner descendant. The corrected expected IDs preserve that owned identity.
Canonical source is unchanged pending Haru's ordinary integration. No runtime
repeat or game/physical acceptance is claimed.
