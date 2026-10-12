# Owner activity during rig tests

`Stage-OwnerIdle.ps1` normally requires the requested idle interval. Explicit
owner direction may allow tests while the owner observes or uses the keyboard.
Record that standing direction locally in
`%LOCALAPPDATA%/dbce/rig-activity-policy.json`:

```json
{
  "schemaVersion": 1,
  "allowOwnerActivity": true,
  "recordedAt": "<ISO timestamp>",
  "reason": "<the owner's instruction>"
}
```

`Assert-StageOwnerIdle` then returns `source=owner-activity-allowed` and a null
idle observation. It does not report fabricated idle time. Runners with a
separate activity-interruption check should consult
`Test-StageOwnerActivityAllowed` there too. Preserve the shared lease, one-game
queue, force muting and restoration; this policy changes only the activity gate.
Set `allowOwnerActivity` to false (or remove the local policy) when the owner
withdraws that direction. There is no automatic expiry. Absent policy retains
idle checks; malformed policy is an error.

The optional `AllowOwnerActivity` argument supports explicit per-run admission
and isolated tests. Do not enable either path without owner direction. Keep the
local instruction file out of distributable packages.
