# Native sampled sessions (development contract 1)

`native/session/session_writer.h` writes `dbce.wheel.session` version 1 from
Windows C++17. It is a source-vendored component, separate from the released
WheelFfb DLL/ABI. Pin its source commit and file hash explicitly. It does not
upgrade a consumer's existing device runtime.

The caller owns one writer on one observation/worker thread. File operations
are synchronous there; never call it from a physics, input, audio or force
hook. If polling and writing share that worker, use actual monotonic timestamps,
expose sampling gaps, and skip missed deadlines rather than manufacturing
catch-up samples. This API has no queue and cannot claim fixed-tick sampling.

Files use exclusive creation and UTF-8 JSON lines (ASCII metadata/channel API,
UTF-16 Windows file paths). Values round-trip as doubles. Metadata records UTC;
sample times are supplied by the caller and must increase. Default bounds are
600 seconds, 64 MiB and 256 KiB per record. Limits close with an incomplete
footer; invalid samples or write failures abandon the file without a successful
footer. Destruction never claims completion. `stop()` explicitly flushes and
closes, and the footer contains exact line-byte/sample counts. A complete file
does not by itself qualify the producer, force reconstruction or trajectory.

Run `./native/session/Test-NativeSession.ps1`. The C++ executable generates
valid, interrupted, invalid-order, nonfinite and three limited fixtures. The
existing managed `SessionReader` validates the native wire format and rejects
incomplete files. Nothing loads DirectInput or opens a device.

`native/dinput-proxy/muted_effect.h` is only the parameter-copy portion of a
diagnostic sink: preserve the source request, send a copy with gain zero and
strip `DIEP_START`. The consumer must guard effect Start/Escape, device commands,
property writes and acquisition, including alternate interfaces and failure
paths. Native API calls can download/restart an effect even without an explicit
Start: see Microsoft's [SetParameters contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417952(v=vs.85)).
This helper's fake tests do **not** certify a consumer's hardware mute.

First consumer: Gravel's developer-only sampled signal candidate, October 7,
2026. Availability and ages distinguish missing/stale audio, input and native
force summaries. These are not ordered effect events, physical torque or
physics-tick poses. Route playback and force qualification remain separate work.
