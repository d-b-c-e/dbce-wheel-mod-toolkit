# Process identity for future closure readers

`Stage-ProcessIdentity.ps1` supplies a read-only process census and a pure classifier. Dot-sourcing does not query processes. Source review passed under Sana158594; the one authorized PROCESS-IDENTITY-PURE-01 run passed 25 pure checks on PowerShell 7.6.6. No existing frozen reader or consumer recipe adopts it yet.

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

## Optional pre-entry persistent-service classification

`Stage-OwnedProcess.ps1` retains actual worker lineage as authoritative. Default callers keep their existing
supplemental command-line scope. A future explicitly reviewed operator may call
`Set-StageOwnedPersistentServices -EnteredUtc $enteredUtc -Services $services` once during preflight, before its
first observation or child launch. This source does not update any frozen helper, hash pin or active operator.

The bounded list (at most eight, no duplicate PID) has this record shape:

```powershell
$services = @([pscustomobject]@{
    processId = 1234
    createdUtc = '2026-10-19T08:00:00.1234560Z'
    precision = 'CimMicrosecond'
    admittedAtUtc = '2026-10-19T08:05:00.0000000Z'
    coordinator = 'named-coordinator'
    role = 'existing-service-keeper'
    custodyReceipt = [pscustomobject]@{
        path = 'C:\private\receipts\service-custody.json'
        sha256 = '0000000000000000000000000000000000000000000000000000000000000000'
    }
})
```

These values are illustrative, not admission for a real process. Before configuring, the operator must verify
the exact custody receipt contents and SHA256, identity, coordinator disposition and explicit admission time.
The helper validates/copies that pinned reference; it does not read a receipt file or derive authorization from
a routing name, role, path, hash string or process name. An unverified reference is not an admission grant.
Both creation and explicit admission must strictly predate the supplied actual ENTER; creation in the same
coarse producer bucket as ENTER is ambiguous and refused. Timestamps must be zoned with supported precision.
The list is copied only after all records validate, cannot be widened after configuration, and ordinary caller
variables cannot replace it. The helper's internal fixture state remains a fixture seam, not a security boundary.

Every observation revalidates the exact identities against its same complete process census. Only a matching
supplemental path match can be classified as admitted. Recorded worker lineage, including newly adopted
descendants, always wins and is never exempted. Reused PIDs retain their normal supplemental scope; known absent
services are not reported as alive. Missing/ambiguous creation, duplicate service rows or a failed census refuse
classification and latch query safety false. No stale admitted list survives an unsafe observation.

`Complete-Owned` reports the last verified `admittedPersistentServices` separately from `ownedLineage` and
`remainingScopedChildren`, plus `persistentServiceClassificationSafe`. Admission is preservation of an explicitly
coordinator-owned service, not proof that it closed. Transient cleanup, live retained handles, exact lease release
and owner-state checks still apply. No service discovery, termination or automatic receipt adoption is added.

The changed source is qualified only with parser checks and the existing synthetic fixture scripts in a separately
authorized slot: `Test-StageOwnedProcess.ps1` and unchanged `Test-StageProcessIdentity.ps1`. Those fixtures supply
their own census/mock handles; they do not enumerate or operate on live services. A passing fixture result is not
runtime adoption, and historical receipts retain their original helper hashes.

## Focused qualification, October 15

Source/test commit `acc222055e76b6ba7f4ea4442cfcc7480eaaa1c0`, helper SHA256 `183C7C63B829A2EC7894A1F9F994FFFD511B09C37A389A9EE26B51406F2BF11F`, fixture SHA256 `47F4AD7909031F5D2EACC8F8CEFBEAADC01286B0EC7B57061D5A0F2608338659`. The existing WindowsApps PowerShell 7.6.6 path was resolved before lease acquisition. Under grant158594, the sole command ran once from20:54:56.2094278 to20:54:58.2606314UTC; child56724 created20:54:57.2907371UTC exited0 and its streams closed. Exact shared lease fd6ecdf06791483fa88e6e4b2af026a3 was released and absent. A subsequent targeted child-PID lookup found it absent and checked the lease again; it did not invoke the candidate live census.

Hub evidence: `artifacts/process-identity-pure-20261015/` in `E:/Source/dbce-project-mgmt`. Original result SHA256 `21A10044DD2187FB4CB017D3809971AB3D41D350B2721EDC47AC066B47D816D7`; independent child/lease receipt `F2B7C594AE02EB1EBE1124269E7ACE188E122410DA142FD452EE01F032AA8C6B`. These are pure classifier/query-error fixture results and actual runner closure, not live CIM adapter qualification, game testing or adopted consumer behavior. No extra suite or fixture repeat was run.
