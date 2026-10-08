# Exact game preference backups

Use `tools/playback/Stage-Registry.ps1` for Windows game settings under HKCU.
Dot-source it in Windows PowerShell 5.1 or PowerShell 7. Keep the game closed
while backing up and restoring, under the shared rig lease. This utility does
not acquire a lease or supervise a process itself.

```powershell
. ./tools/playback/Stage-Registry.ps1
$subkey = 'Software\Pixel Perfect Dude\DRIVERally'
$receipt = Save-StageRegistry -SubKey $subkey -Path ./owner-before.json
$receipt | ConvertTo-Json | Set-Content ./owner-before-receipt.json
# Run the scoped test, close its owned process, then:
Restore-StageRegistry -SubKey $subkey -Path $receipt.path -ExpectedSha256 $receipt.sha256
```

Keep the snapshot and receipt outside the game directory. Vendor this source
with its hash if a consumer needs a standalone runner; do not make a public mod
depend on the private toolkit checkout. Existing game-specific helpers that
preserve and verify the full native inventory remain valid.

The snapshot stores every native value name, registry type and exact bytes,
including unsigned DWORDs, binary calibration data, default values and long
Rewired names. Names and values are read again before the immutable file is
written. Restore requires a caller-supplied target that matches the snapshot,
checks the expected SHA-256 and validates all data before writing. It restores
known values, removes extra values individually, and verifies the complete raw
inventory. An absent-before key is recorded explicitly: restore removes the
test-created values and then the empty leaf. A pre-existing key is never deleted;
if missing, it is recreated from the validated snapshot. Nested keys are refused
before any write, both during capture and restore, so a partial snapshot cannot
look complete. This helper snapshots a single game preference leaf, not a tree.
Value names use UTF-16 base64 in the JSON to avoid older PowerShell 7 converting
date-shaped names into DateTime. The native name is decoded before comparison.
Any failure is reported; preserve the backup and do not label restoration complete.

## Confirmed export defect, October 8

On the rig, `reg.exe export` exits **0** after encountering a long value name and
silently omits it and subsequent values. The standalone test reproduces this
with invented values: one 285-character name among six values results in only
one exported value. The expanded seven-value fixture (including an eight-byte
Unity DWORD and a date-shaped name) exports only five. Both PS5.1 and PS7 tests
pass all 18 raw-backup checks, including absent, empty, missing and nested keys.

A private reproduction using DRIVE's verified October7 raw snapshot produced
363 exported values from 376 native values. The 13 omitted values included the
R12 pedal bindings/calibration, other controller maps and the saved camera.
Deleting the key and importing that export loses those settings. Comparing
before/after `.reg` files can still pass: both are incomplete. This is why
`reg export` success, file size, and `.reg` byte equality are insufficient.

PowerShell `Get-ItemProperty` also throws on some unsigned Unity DWORD values.
Do not repair this by skipping unreadable values or coercing data through strings.

Run `tools/playback/Test-StageRegistry.ps1` in both shells. It creates and removes
only an isolated randomly named test key, retains evidence in the temporary
directory, and does not open a game or a force device. The reg.exe reproduction
is informational on hosts where Microsoft has fixed the defect; exact native
snapshot/restore checks remain mandatory.
