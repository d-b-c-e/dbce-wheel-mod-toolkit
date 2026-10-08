# Strict identity for retained force handles

Isolated candidate based on a7fe985 (the reviewed 41-export native 0.6 ABI with
lock-publication/shutdown fixes). It is not an official release, consumer pin or
replacement for current master/native 0.10. The version export remains 600;
source and binary hashes distinguish this unpublished override.

Previously `InitDirectInput` returned success whenever `g_device` existed,
before checking strict selection. Clearing the preference or choosing B while
A was retained therefore reported successful initialization on A. HCT's managed
release-before-switch workaround does not fix this shared API.

With strict selection, a missing or mismatched preferred GUID releases the force
handle and returns 0 with `DIERR_NOTFOUND`. The consolidated candidate uses
Claude's `ReleaseRetainedDevice` from master 5c3503f: stop/release every effect
family and device under the effect lock, clear the last force and active identity,
and keep the watchdog, exit guards and DirectInput instance. This preserves the
protections for a later explicit initialization; the first 7d372ef candidate
used full FreeDirectInput and removed them. It leaves independent input
readers alive. A borrowed A reader becomes dormant, remains dormant if a later
explicit initialization opens B, and resumes only when A is opened again.

There is no implicit switch during the refused call. The caller must handle
failure and make a later explicit initialization attempt. Strict same-GUID reuse
and non-strict legacy reuse remain unchanged. No force arithmetic, gain,
recovery ramp, effect waveform, API signature or export was changed.

## Offline qualification

Both x86 and x64 pass:

```powershell
./native/wheelffb/test/build_retained_identity_test.bat x86
./native/wheelffb/test/build_retained_identity_test.bat x64
./native/wheelffb/test/build_burst_test.bat x86
./native/wheelffb/build/x86/burst_test.exe --pre-init-worker
./native/wheelffb/test/build_burst_test.bat x64
./native/wheelffb/build/burst_test.exe --pre-init-worker
./native/wheelffb/test/build_lifecycle_test.bat x86
./native/wheelffb/test/build_lifecycle_test.bat x64
```

The identity fixture compiles actual production source and has 57 checks: active
fake constant, periodic and condition effects stopped/released on mismatch,
watchdog and installed exit-guard retention, missing preference, same-GUID
reuse, no fallback for absent/failed requested targets, non-strict compatibility,
independent/borrowed readers and explicit Free watchdog join. The initial 49-check fixture against the
unchanged a7fe985 source fails at the first mismatch refusal assertion, as required.
That negative control is retained under `native/wheelffb/build/identity-negative`.

The existing burst suite covers concurrent lock publication, 400 empty releases,
stop-before-join, delayed worker order, cleanup/restart, concurrent diagnostics
and focus/watchdog behavior. The actual DLL fixture resolves all 41 exports,
checks version, empty cleanup, worker restart and 32 unload cycles. It never
enumerates or initializes a real device. Its three unused static header-helper
warnings are unchanged; the new identity fixture builds with /W4 /WX.

Tested DLLs from `build.bat` (/LD /O2 /W4 /EHsc /DNDEBUG):

| Architecture | SHA-256 |
|---|---|
| x86 | 432727681B1A8A6F62D68F2E1ED865FF9F229AECCC628B82FA97147626451968 |
| x64 | 3F6CC51444879D01FEB900098B2DE94242594D093F523E3C34BD511D69C0D946 |

No game, device acquisition, hardware smoke, physical force or consumer install
was run. Stop calls and fake effect state do not certify physical torque release.
Independent source review and consumer-specific qualification remain before a
pin change. Art/DRIVE's 46-export impact support and Woden's input override must
not be replaced by this narrower 41-export binary.
