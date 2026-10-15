# Process identity for future closure readers

`Stage-ProcessIdentity.ps1` supplies a read-only process census and a pure classifier. Dot-sourcing does not query processes. This candidate is source preparation under Sana158281; its focused fixtures are authored but not run. No existing frozen reader or consumer recipe adopts it yet.

The incident prompting it was Obstacle precommit: original wrapper 69E9C02E passed and released its lease; independent reader BD103AE9 refused because checker pwsh reused PID50812. Original process creation was 20:43:27.789197Z; checker creation was 20:45:31.825880Z. Sana's separate independent 1056221B closed the completed batch without termination or repeated tests. Those receipts remain unchanged.

## Caller contract

- Record `processId`, `createdUtc` and explicit producer `precision`: `Tick100ns` for a retained .NET Process.StartTime, or `CimMicrosecond` for Win32_Process.CreationDate. Keep original receipt timestamps as strings when reading JSON (PowerShell 7.5: ConvertFrom-Json -DateKind String). Do not infer precision from the number of printed trailing zeros.
- `Get-StageProcessIdentitySnapshot` buffers the full CIM census and returns querySucceeded/rows/error. An exception, including one after partial output, returns querySucceeded=false and no rows. The optional QueryProvider is a fixture seam, not a fallback that converts query failures into empty success.
- `Get-StageProcessIdentityDisposition -Recorded $record -Snapshot $snapshot` returns `OriginalAlive`, `Reused`, `Absent` or `Unknown`, plus `originalAbsent`. Only Reused/Absent close an original identity. All recorded identities must close, and the caller must separately check workload/lease/owner-state conditions.
- Normalize UTC and compare at the coarser actual producer precision. CIM provides microseconds (10 DateTime ticks); .NET timestamps can retain 100ns ticks. Equal microsecond buckets conservatively mean OriginalAlive. A later bucket means reuse; an earlier bucket is ambiguous/Unknown. There is no 1ms tolerance.
- Missing timestamps, unspecified timezone, invalid producer precision, duplicate matching rows, malformed census PIDs or an unsuccessful census remain Unknown. A reused process is never a cleanup target. This helper never terminates a process or changes a lease.

Focused command for a separately serialized slot:

```powershell
pwsh -NoProfile -File tools/playback/Test-StageProcessIdentity.ps1
```

It uses pure supplied snapshots and injected query providers, with no real process census, launches, file writes, device calls or game calls. Future recipe adoption requires its own changed-recipe review; this source does not reopen a completed batch.
