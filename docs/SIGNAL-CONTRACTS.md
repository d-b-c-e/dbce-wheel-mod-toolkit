# Signal contracts (source release 0.11.0)

These device-free contracts are shared by native and managed consumers. Game
addresses, collision labels, coordinate transforms and ROM guards stay in the
game adapter. Neither an FFB waveform candidate nor a successful device API call
proves that a driver felt a collision.

`ImpactMixer` combines signed structural force with an explicit 100 ms torque
envelope. Enable it deliberately; it reserves 25% of the constant-force budget,
even between events. The positive and return lobes have equal areas. Up to eight
events can overlap, with bounded pulse amplitude and a common final strength.
Timestamps are monotonic seconds. Invalid inputs or a clock rewind cancel the
pending output. Its bound excludes separately downloaded spring, friction and
damper effects. Do not present it as a total device torque limit.

The shared `impact-contract.csv` contains analytical output expectations, used
by native and managed tests. Existing force profiles and their reference vectors
are unchanged. Physical acceptance requires labelled driving, including a clean
straight, cornering, wall contact and car contact on each tested wheel.

`SignalSample` carries value, unit, emulated frame, sample time, source and
quality. A fresh or briefly held measured zero differs from unavailable speed.
Consumers choose the acceptable age; future, stale, nonfinite and unknown-unit
samples are unusable. Source values distinguish HUD OCR, numeric HUD production,
game memory and derivation. Never promote an OCR value to memory telemetry by
copying it into a different packet format.

Managed `DevicePreference.Choose` now fails explicit missing or ambiguous
selections. Instance GUIDs survive enumeration reordering on the same host.
VID/PID identifies a product, not an individual unit; duplicate products need an
instance identity. Existing DLL enumeration now supplies its instance GUID but
cannot supply VID/PID metadata. That preference fails closed unless the caller
supplies metadata, instead of silently picking another wheel. Explicit indexes
remain session-local. Native device ABI selection behavior is unchanged and
still needs equivalent integration; no DLL binary was rebuilt in this release.

Validation: 109 managed tests, MSVC and GCC native contract checks, and all ten
legacy profile vectors. No physical force was applied. Cruis'n consumes source
headers; other consumers have not been upgraded or tested by this work.
