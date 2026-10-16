# Shared Unity operator admission

This read-only helper replaces the copied JSON-read predicate in the hub's
`UNITY-PRIMARY-ACTIVATION-OPERATOR.md`. It performs no Windows automation.
Keep that guide's returned-window selection, supported Sky activation,
immediate state refresh and native foreground verification unchanged.

Woden06 (October 16) refused before any Sky call. The first failing snapshot
was not retained, so its exact failing predicate remains unknown. Later data
confirms the expected request ID, process, birth, executable and HWND matched.
Do not label it a UUID mismatch or failed activation. The producer performs
process-lineage observation before publishing a timestamped foreground sample;
a one-shot two-second freshness check is brittle if that publication is late.

The helper preserves the two-second freshness requirement. Only a stale sample
is reread, at 100 ms intervals and only until the original producer deadline.
Changed identity/deadline or another failed check refuses immediately. Errors
name failed checks and retain the exact JSON read in `error.snapshotText`.
`onPending(details, snapshotText)` receives the first stale read even if the
next read succeeds. Errors also retain `firstFailedDetails` and
`firstFailedSnapshotText`; read and parse failures have distinct predicates.
This corrects that brittle behavior; it does not prove Woden gameplay works.

Before a newly coordinated run, import the pinned helper in `node_repl` and
replace the old `beniReadAdmission` definition:

```javascript
globalThis.admissionModule = await import('file:///EXACT/PINNED/UnityOperatorAdmission.mjs');
globalThis.beniFirstStaleRecorded = false; // Initialize once for this fresh run.
globalThis.beniReadAdmission = async () => {
  try {
    return await admissionModule.readUnityOperatorAdmission(
      beniAdmissionPath, beniExpectedRequestId, {
        onPending: async (details, snapshotText) => {
          if (beniFirstStaleRecorded) return;
          await beniDesktopFs.writeFile(EXACT_NEW_FIRST_STALE_FILE, snapshotText, { flag: 'wx' });
          await beniDesktopFs.writeFile(EXACT_NEW_FIRST_STALE_METADATA,
            JSON.stringify(details), { flag: 'wx' });
          beniFirstStaleRecorded = true;
        }
      });
  } catch (error) {
    if (typeof error.snapshotText === 'string') {
      // A fresh run-specific evidence file, never an existing result.
      await beniDesktopFs.writeFile(EXACT_NEW_FAILURE_FILE, error.snapshotText, { flag: 'wx' });
    }
    throw error;
  }
};
```

Use the request spelling supplied by the producer (Woden uses GUID N format).
Do not derive it from a different run or change it after a refusal. Prepare
the import before launch and confirm that the supported window provider is
available, so a missing provider cannot waste another startup window.

Focused offline check: `node tools/playback/Test-UnityOperatorAdmission.mjs`.
Its fake clock and file publisher exercise stale-to-fresh recovery, original
expiry, identity/deadline changes and named request/birth/path/window/status
refusals. No game, UI, device, build or actual delay is involved.
