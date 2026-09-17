# Shaped finite sine bursts - 0.14.0 local candidate

Native 0.7.0 adds one Cdecl export (42 total), without changing existing
signatures. No public release or physical validation is implied.

Native: `int PlayShapedPeriodicBurst(int slot, int magnitude, int freqMilliHz,
int phaseHundredths, int fadeMs)`.
Managed: `bool PlayShapedPeriodicBurst(int slot, float magnitude01, float freqHz,
int phaseHundredths, int fadeMs)` and `SupportsShapedPeriodicBursts`.

Use the existing idle `CreatePeriodicBurst` and `StopPeriodicBurst` on the same
four-slot pool. Magnitude 0..10000 (managed finite 0..1), frequency1000..100000
millihertz (managed finite1..100Hz), phase0..35999 hundredths of degrees,
fade1..slot duration milliseconds. Wrapper checks fade1..500; native validates
against the allocated slot duration. Zero magnitude stops. Invalid requests
leave the existing effect unchanged. Foreground, panic and lifecycle rules are
identical to legacy finite bursts. Native duration remains bounded1..500ms.

The driver receives sine offset0, requested phase, full requested initial and
sustain amplitude, attackTime0, fadeLevel0. Fade occupies the **final fadeMs**
of the existing lifetime. Full-lifetime fade therefore declines immediately.
6.25Hz/120ms/phase9000/full120ms fade requests an immediate positive peak and a
smaller negative rebound ending at .75 cycle; wheel motion/feel are unverified.
No software force stream, SOLO flag, reacquire/retry, or pending event exists.

Before starting, shaped play requires S_OK from SetParameters and GetParameters
and an exact echo of duration, magnitude, period, phase, offset and envelope.
Positive adjustment statuses, readback mismatch or unsupported readback reject
the event. Start(1,0) is attempted once and must also return S_OK. Rejection
stops/releases the COM effect; the slot and duration remain allocated. A fresh
legacy landing may recreate the idle handle without explicit reprepare.
The consumer should latch unavailable shaping rather than retry a rejected
crash, and must not advertise an unshaped fallback as a shaped kick.
LastError/GetLastHResult and a bounded native rejection log report failure.
API echo is not measurement of physical output, even when every call succeeds.

Every positive legacy PlayPeriodicBurst explicitly sends phase0 and a null
DIENVELOPE with DIEP_ENVELOPE, clearing any preceding shape in the SAME slot.
Continuous Update still rejects finite slots. Optional binding leaves old
0.6.x DLLs usable for legacy bursts and 0.5.x DLLs usable for their existing API;
missing shaped exports return false with an explanatory error, without an
unhandled EntryPointNotFoundException.

Microsoft specifies phase rounding is possible, and defines envelope levels
and times: [periodic effects](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418719(v=vs.85)),
[DIENVELOPE](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416625(v=vs.85)).
Strict echo validation may reject a driver that cannot report these fields.

Validation uses fake COM effects, including the production native adapter and
exported shaped route: copied phase/envelope, no SOLO, lifetime, shaped-to-legacy
clearing, bounds, background/zero-stop, adjusted/error results, and retained
slot metadata on failure. Existing x86/x64 package and logging gates also run.
Hardware smoke is compiled but not executed for this task: autonomous hardware
launch/output was explicitly excluded. No physical waveform is verified.

Offline evidence: full local0.14.0 package gates passed on2026-09-16:
121 managed tests,24 sync assertions,80 current native binding assertions plus
9 pre-burst and8 burst-only compatibility assertions per runtime, native
fake-burst/shaping and force-logging tests on both architectures, and all
existing conformance/helper/proxy checks. Native builds export42 entries.
Smoke harnesses compiled on x64/x86 only; neither was launched.
