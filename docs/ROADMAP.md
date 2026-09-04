# Roadmap — which projects move onto the toolkit, and in what order

*2026-09-03. Order is by payoff per unit of risk. Each project moves in
slices; a project is "on the toolkit" when it ships no code this repo also
ships.*

## Status

| # | Project | On the toolkit | Still to move |
|---|---|---|---|
| 1 | art-of-sim-rally | telemetry, native FFB DLL | force shaping, `WheelInput`, support bundle |
| 2 | iracing-arcade-wheel | telemetry, native FFB DLL | the Unity Input System kit it would donate |
| 3 | OutRun2006Tweaks-FFB | Forza encoder, the whole FFB device layer (C ABI) | nothing; its force model stays by design |
| 4 | milestone-wheel-tools | Forza encoder, PowerShell helpers | the dinput8 proxy scaffold it would donate |
| 5 | cruisn-collection | — | Python tools now, SDL2 backend later |

Two things are verified only as far as a bench allows and need a drive:
iracing-arcade-wheel's force feedback has never been judged at a powered base,
and OutRun's migrated device layer has been proven at a MOZA R12 through the
toolkit smoke test but not yet in the game.

## Order (and why)

### 1. art-of-sim-rally — DONE (telemetry + native + Fanatec work), 2026-09-04

Why first: it is the superset. Its native DLL is the most complete FFB
implementation in the family (re-acquire, Fanatec-twin fallback, one
DirectInput instance, axis reading), its telemetry encoder is the one with a
test suite, and its `WheelInput` is the direct-read design the input kit is
built from. Moving code out of it *is* building the toolkit. It also has
public users, so every slice is verified against a live install.

| Slice | Toolkit piece | Verification |
|---|---|---|
| Telemetry | `Dbce.Wheel.Telemetry` replaces `ArtOfSimRally.Telemetry` | byte-identical encoder; 39 tests; SimHub unchanged |
| Native FFB | `WheelFfb.dll` vendored as `UnityForceFeedback.dll` | smoke launch: FFB initialised, three devices read; drive |
| Force shaping | `ForceShaper` + `ForceModel.SimLite` replace `FfbController` math | trace compare against today's Fy × trail output |
| Direct read | `WheelInput` core moves to `Dbce.Wheel.Input`; the panel stays | assign, drive |
| Diagnostics | `SupportBundle` sections come from the toolkit | bundle diff |

### 2. iracing-arcade-wheel — DONE (telemetry + native), 2026-09-04

Telemetry and the native DLL are vendored; the input kit extraction is still
open. Its force feedback has still never been judged at a powered base.


Why second: its telemetry encoder is a byte-identical copy and its native DLL
is the same file three fixes behind, so the first two slices are mechanical.
It contributes the pieces art of rally does not have — `Rebinder`,
`BindingStore`, `AxisSetup`, `Layouts`, `InputSupport`, the HID-gap
diagnostics and the IL2CPP-safe `Widgets` — which become the Unity Input
System sub-package. Wait for the session currently working in it to land its
0.2.0; then telemetry and native DLL first, input kit extraction after.

Watch item: its FFB signal is a slip × load proxy that has never been driven
with the base powered; run the sign-by-speed-band check before trusting it.

### 3. OutRun2006Tweaks-FFB — DONE, 2026-09-04

543 lines of DirectInput code removed for 139; the force model untouched. It
selects its wheel by instance GUID (a new toolkit export) to match its remap
layer, and loads `WheelFfb.dll` at runtime from beside `dinput8.dll`. Its
periodic-effect logic went **into** the toolkit on the way past.

Still open, and it needs one drive: the steering input. It was recorded as
"field_1D0 reads zero"; the validation log actually shows |1D0| below 0.011
while field_1D4, supposedly its derivative, reaches 0.54. A new FFB STEERSCAN
diagnostic prints the range of the six candidate floats so one lap settles it.


Why third: its FFB lifecycle code (exit-time zeroing, recreate-and-ramp,
poll rule, separate exclusive handle, periodic effects) is what the native
core absorbs in step 3 of the plan, so once the core carries it the project
can *replace* ~450 lines of its own DirectInput code with the C ABI. Its
force model becomes the `ForceModel.Arcade` preset. Its 311-byte Forza
encoder is replaced by the shared one.

Do first, regardless of the toolkit: the steer input (`field_1D0`) reads zero
through corners, so the spring and damper terms the redesign was built around
produce almost nothing. The toolkit's input validators exist to catch exactly
this; fix the offset before comparing feel.

### 4. milestone-wheel-tools — DONE, 2026-09-04

On `forza_packet.h` and `DbceWheel.psm1`. Steam discovery had been written out
three times inside that one repo. The dinput8 proxy scaffold has still not been
extracted — it is the largest remaining piece of shared native code.


Why fourth: it does not produce FFB (it taps the game's), so the FFB core
does not apply. It contributes the dinput8 proxy scaffold, the SIXDOF →
DRIVING re-type, the UE4 reflection scanner, the FMOD tap and the
PowerShell wizard — the `native/dinput-proxy` module and half the knowledge
base. It consumes the shared Forza encoder (as a C header) and the shared
PowerShell module (Steam discovery is triplicated inside this one repo).

### 5. cruisn-collection — last, and only partially

Why last: the FFB engine lives in a GPL MAME fork and is measured, working
and freshly pivoted; it is the seed for the SDL2 backend, not a consumer of
the DirectInput one. It adopts the shared Python tools (probe, synth, idle
keeper), the shaping constants and the device-quirk data now, and the DLL's
SDL2 backend later if the conformance probe shows no regression. Its
diagnostics (`ffb_trace_report`, `support_bundle`) move *into* the toolkit.

### 6. cannonball-se, mame-racing, xoutput-redux — knowledge only

Their DirectInput FFB code is recorded in the knowledge base (VID:PID
targeting, the non-exclusive-when-SDL-owns-the-device condition, Linux
evdev) and they are not migrated. cannonball-se is upstream-derived; the
decision to offer fixes upstream is separate.

## New games

A new game starts from `templates/` and `knowledge/NEW-GAME-CHECKLIST.md`:
fingerprint the engine (`dbce-mod-toolkit`), pick the loader template, write
the sampler into `TelemetryFrame`, choose a `ForceModel` preset and *measure
the sign*, wire telemetry, panel, installer, support bundle. Everything else
is vendored.

## Not on the roadmap

Public distribution of this repo is a later decision. It is MIT and contains
no game files, so nothing blocks it, but art-of-sim-rally is the only public
consumer today.
