# Changelog

Notable changes to dbce-wheel-mod-toolkit. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The native C ABI
only ever gains exports.

## [0.9.0] - 2026-09-04

**Cruis'n joins, and brings four things this repo did not have.** It is the
first SHAPER-ONLY consumer: the arcade board hands over a finished signed motor
byte, so there is no physics to model and `ForceModel` never runs.

### Added

- **`ForceShaper.SmoothingMs`** - a low-pass time constant in milliseconds,
  `a = 1 - exp(-dt/tau)`. **This is strictly better than what was here.** The
  dimensionless `AttackSmoothing`/`DecaySmoothing` pair is frame-rate dependent:
  the same number smooths harder the faster you call it, so a tune arrived at by
  driving is only valid at the rate it was tuned at. When set, it wins over the
  pair. Taken from Cruis'n.
- **`ForceShaper.ImpulseBypass`** - an input jump this large counts as an event
  by itself. Needed when the source has no event flag to give you; an arcade
  board never says "this one was a wall".
- **`ForceShaper.SettleBelow`** - input zero and output below this, snap to zero
  so the effect can stop. Conditional on the input being zero, which is what
  makes it different from `OutputDeadband`: it kills a dying tail without also
  killing a small force the game asked for.
- **`ForceShaper.RelevelHysteresis`** - hold the last value until it has moved
  far enough to be worth sending. Measured on the Cruis'n trace: device updates
  drop from 9,437 to 5,872 out of 15,552 ticks, for a force difference that
  cannot exceed the threshold itself.
- **`cruisn-vunit@1`** reproduces what `mvffb` did by hand, and
  **`cruisn-vunit@2`** is a 20 ms variant testing a reported loose centre.
- **`data/cruisn-usa-mvffb-0.3.7-wheel-trace.csv`** - 62 s of real motor bytes
  captured on the rig, with the timestamps.

### Verified

- **The conversion does not change the feel, and that is measured rather than
  claimed.** `CruisnVectorTests` transcribes mvffb v0.3.7 verbatim as a
  reference and drives both it and `cruisn-vunit@1` with the captured trace at
  the FFB worker's real 4 ms cadence: **worst difference 33 of 32767, mean 1.8**.
  33 is one re-level step - the closest two implementations can be when they
  differ only in where rounding happens.
  Ablations prove it is not a coincidence: removing `SmoothingMs` costs 10,700,
  removing `ImpulseBypass` 9,974, removing `SettleBelow` 175.
- All four defaults are off, so every existing profile is untouched; the golden
  file shows exactly that.

### Note

Driving the two chains at the trace's own row spacing instead of the worker's
cadence makes the comparison meaningless - the gaps run past `Shape`'s 100 ms
dt clamp and the filters then diverge for reasons unrelated to either algorithm.
That cost an hour and is written into the test's remarks so it is not rediscovered.

## [0.8.0] - 2026-09-04

**Gain and invert moved to the middle of the conditioning chain.** Both were
applied last. Chasing the first one uncovered a second, worse defect, and then
a third; all three are the same root cause, and the fix is one line moved.

The chain is now:

```
deadzone -> EMA -> GAIN/INVERT -> soft saturation -> slew -> fade -> ramp -> clamp
```

Gain and invert are the boundary. Above them the value is in model units;
below them it is in true output units and true output sign, and every stage
below is a device-side limit. **The slew limiter compares against the value
actually sent, so it cannot be correct while that value and the one being
limited disagree on either scale or sign.** That is the whole of it.

### Fixed

- **The slew limiter latched the wheel at full force, in the wrong direction.**
  Above roughly `shaper.strength` 65, gain re-inflated an already-saturated
  value into the hard clamp, so the limiter's reference pinned at 1.0 and a step
  of 3.6/60 could never walk it back. Measured on `arcade-outrun@3` over the
  conformance sequence: the output sat at exactly full scale for **163 of 200
  ticks**, and did so against the model - it asked for -1.02 while the wheel was
  sent +1.0000. A latch, not a limit. Now 0 of 200, mean output 0.87 -> 0.48.
- **`shaper.invert` plus any slew limit meant no force at all.** Invert was
  applied after the limiter, so the limiter's reference had the opposite sign to
  the value it was limiting. A steady input oscillated between -0.060 and 0.000
  forever: the wheel buzzed at the tick rate and no force ever arrived. Invert
  is now a clean mirror, asserted tick by tick.
  Nothing shipped hit this - every profile has invert false - but the tuning
  guide tells a player to set it when the wheel fights them, and all three
  arcade tunes carry a slew limit.
- **Raising `shaper.strength` no longer flattens the knee.** Gain now drives
  into the soft saturation instead of past it, so a crash still outweighs a hard
  corner. `arcade-outrun@1` at 60 km/h, model 1.0 against 3.0: at strength 75 the
  gap was exactly 0.0000, and is now 0.0946.

### Added

- **`arcade-outrun@3`** - `arcade-outrun@1` with `shaper.strength = 75` and
  nothing else changed, so an A/B against @1 isolates that one dial.
  It also exists because the golden file could not otherwise see this release:
  every other profile ships strength 50, where gain is exactly 1.0 and the
  ordering is unobservable. Isolating the change with an identical profile set
  on both sides gives 0/200 ticks changed for all five strength-50 profiles and
  200/200 for @3. **Without @3 in the file this entire release would have
  produced a zero-line conformance diff** - the same blind spot that hid the
  0.7.0 clamp change until the sequence was extended. Third time; the pattern is
  now in the notes rather than the incident.

### Compatibility

No shipped tune changes: all five existing profiles set strength 50 and invert
false, where both moves are provably no-ops, and the golden file shows exactly
that. A consumer that had raised strength above ~65, or set invert, was in the
broken cases above and moves to correct behaviour.

## [0.7.2] - 2026-09-04

No behaviour change. A hazard measured, pinned and explained to players.

### Fixed

- **The tuning guide sent players straight into a defect.** It said "too weak or
  feels dead - raise `shaper.strength`", and separately promised that soft
  saturation keeps a crash feeling bigger than a hard corner. Both cannot be
  true at once: gain is applied at step 7, after the soft saturation at step 3,
  so raising strength re-inflates an already-compressed value into the hard
  clamp. On `arcade-outrun@1` at 60 km/h, a hard corner against a crash:

  | strength | corner | crash | difference |
  |---|---|---|---|
  | 50 | 0.7616 | 0.9951 | 0.2335 |
  | 60 | 0.9139 | 1.0000 | 0.0861 |
  | 75 | 1.0000 | 1.0000 | **0.0000** |

  The file now states the ceiling and points at the better dials (the wheel's
  own driver, or `model.lateral.forceReference`). It is deployed beside the
  game and is the only tuning documentation a player has, so it had to stop
  recommending the thing that breaks.

### Added

- **`RaisingStrengthPastSixtyDefeatsSoftSaturation`** pins the hazard so it
  cannot drift silently, and fails if the ordering is fixed - which is the
  prompt to update the player-facing note with it.
  It is a test rather than a golden-file row on purpose: every shipped profile
  sets strength 50, where gain is exactly 1.0 and the ordering cannot be
  observed at all. A fix would produce a zero-line conformance diff, exactly as
  the 0.7.0 clamp change did before the sequence was extended.

### Known, deliberately not fixed here

- **Gain belongs before the soft saturation, not after it** - the same argument
  0.7.0 made at the model boundary, one stage later. Deferred because it changes
  feel for anyone who has already moved the dial, and that belongs between test
  sessions rather than immediately before one. No shipped profile is affected at
  its shipped setting.
  Raised by art-of-sim-rally, as an ordering caveat behind the `simlite@2`
  equivalence; they flagged it as "not a problem today". On these profiles it is
  a present one above strength 60.

## [0.7.1] - 2026-09-04

### Added

- **`ForceProfile.SimLite()` and `ForceProfile.Arcade()` return a whole tune** -
  the model and the conditioning that belong together - where
  `ForceModelSettings.SimLite()`/`Arcade()` return only the model half. Pairing
  a model preset with a default `ForceShaper` gives a game a deadzone, an output
  deadband and a ramp it never asked for, which is precisely how `simlite@1`
  shipped art of rally's model with this repo's conditioning. The two model
  presets now derive from the two profile presets, so there is one definition of
  each tune in code rather than two that can part company.
  Every value in a preset is stated rather than left to a default, so a changed
  default cannot quietly retune one, and `CodePresetMatchesTheProfileItNames`
  pins each preset to the section it names in `profiles/force-profiles.ini` key
  by key. Verified by perturbing one value and watching it fail.
  Raised by art-of-sim-rally.
  The C++ side never had the hazard: it has no code presets and only ever loads
  from the file.

### Fixed

- **The note on `simlite@2` still described the clamping order 0.7.0 changed.**
  It told a reader the two pipelines diverge in a low-speed slide and pointed at
  a test that no longer exists. They now agree on every row of the curve.

## [0.7.0] - 2026-09-04

### Changed

- **The clamp moved from the model to the device edge.** `ForceModel.Compute`
  no longer bounds its result to +-1; `ForceShaper` bounds the output, where
  the device limit belongs. A clamp inside the model turned a boundary into a
  silent soft knee and discarded the headroom the fade and the soft saturation
  exist to shape.
  The visible harm was not the clamping order but the soft saturation: it
  received an already-clamped value, so every force from full scale upward
  arrived as exactly 1.0 and `tanh` had nothing to compress. A model output of
  1.5 reached the wheel as 7,615 of 10,000, identical to 1.0; it now reaches
  9,051. `arcade-outrun@1` asks for `tanh` so that a crash still feels bigger
  than a hard corner, and was getting a hard clip instead.
  Agreed with art-of-sim-rally, whose vector made it findable and whose
  pipeline this now matches.
  **Consequence worth knowing:** the fade scales a force, it does not cap one.
  A force far past full scale can now reach full output at walking pace, where
  the early clamp used to limit it by accident. That is how art of rally has
  always behaved.

### Fixed

- **The conformance sequence never saturated the model**, so an entire class of
  change was invisible to the golden file - the first attempt at the above
  produced a zero-line diff. Ticks 150-179 now push three times the reference
  force at 7.5 km/h, past full scale and inside the fade at once.

### Added

- **Every dial in `profiles/force-profiles.ini` is now explained in plain terms
  in the file itself**, which is deployed beside the game and read at runtime.
  It previously pointed at C# source. Includes the counter-intuitive ones
  (raising `lateral.forceReference` makes the wheel *lighter*) and a symptom
  table: wheel shakes by itself, no warning before understeer, crashes feel the
  same as hard corners.

## [0.6.0] - 2026-09-04

### Added

- **A conformance vector for `simlite@1`**, supplied by art-of-sim-rally and
  generated by that mod's own shipping `ForceCurve.cs` rather than transcribed.
  640 grid rows plus a smoothing step response, vendored under
  `dotnet/Dbce.Wheel.Ffb.Tests/data/`. It settles a claim this repo had made and
  never enforced.
- **`simlite@2`**, the corrected profile.

### Fixed

- **`simlite@1` was only half art of rally's tuning.** Its model settings were
  right; its shaper settings were this repo's `ForceShaper` defaults, which the
  game does not use. The curve shows no deadzone (500 N at 5 km/h yields
  0.005487 undiminished, and the device value is 54 rather than 0), no output
  deadband, no soft saturation, no slew and no ramp. `simlite@1` is annotated
  rather than edited, because a published version is immutable.

### Known difference, measured rather than hidden

- **Clamping order.** `ForceModel.Compute` clamps to +-1 before returning, so
  the speed fade in `ForceShaper` scales an already-clamped value; art of rally
  fades first and clamps last. The two agree on every row where the model does
  not saturate. Of 640 rows, 16 saturate and 4 differ, all at 5 and 7.5 km/h -
  inside the low-speed fade - with a worst case of 0.109, about 1,087 of 10,000
  at the wheel. It is only reachable when the force exceeds full scale *and* the
  fade is partial, which is a low-speed slide. `ClampOrderDiffersAndIsMeasured`
  asserts it so it cannot drift silently; which order is correct is a decision,
  not a bug fix, and has not been made.

## [0.5.0] - 2026-09-04

### Added

- **`native/dinput-proxy/dinput_proxy.h`** - the half of a dinput8 wrapper
  proxy that is the same every time: loading the real DLL from `system32` by
  full path, patching a vtable slot idempotently, the DirectInput 8 slot
  numbers as named constants, the SIXDOF to DRIVING re-type, and the
  blank-twin filter. The hooks stay per game. Checked in both architectures;
  the slot numbers are `static_assert`s, because a wrong one corrupts an
  unrelated slot and crashes far from the cause.
- **The three `templates/` scaffolds are written.** Each carries the
  non-negotiables for that shape, the toolkit boundary, what to vendor, and the
  wiring from a car-state struct to a force on the wheel. They are documented
  starting points, not projects that compile: the per-game half is exactly what
  no template can supply.

### Fixed

- `Set-IniValueCrlf` refused an empty value, which is legitimate and often
  load-bearing - OutRun 2006 treats an empty `DeviceGuid` as "this input slot
  is off", so the helper could not write the value that disables a feature.

### Notes

- `dotnet/Dbce.Wheel.Input` is **not** a mechanical extraction and is now
  recorded as needing a design decision first: every donor file references
  `UnityEngine.InputSystem`, Il2CppInterop or BepInEx, none of which can live
  in the Unity-free `netstandard2.0` assembly the rules require.

## [0.4.0] - 2026-09-04

Both additions were asked for by art-of-sim-rally, which found the API could
not express what its roadmap needed. Both gaps were real.

### Added

- **`GetDeviceGuid` and `GetAnyDeviceGuid`.** `SetPreferredDeviceGuid` has
  existed since 0.2.0, but nothing handed a GUID out, so only a mod that ran
  its own DirectInput enumeration could use it. A managed mod has no such
  layer, which left it matching by name - exactly what picks the wrong half of
  a Fanatec base's two identically named devices. The smoke test now
  cross-checks the returned GUID against what DirectInput itself reports.
- **Condition effects: `CreateConditionEffect`, `UpdateConditionEffect`,
  `ReleaseConditionEffects`.** A damper is not a periodic effect. DirectInput
  renders spring, damper, inertia and friction as *conditions*, where the base
  computes force from axis position or velocity continuously rather than
  following 60 Hz updates. A hardware damper is smoother and more stable than
  damping synthesised in the model, and is usually the right answer to a
  direct-drive base that oscillates. Slots behave like the periodic ones:
  create once, update cheaply, recreate behind a hold-off if the handle dies,
  and carry `DIEP_START` until the effect has genuinely started.
  Verified at a MOZA R12: damper and spring both created and driven.

37 exports in both builds; 25 smoke stages green in x64 and x86.

## [0.3.1] - 2026-09-04

### Added

- **`Sync-Toolkit.ps1` never overwrites a locally edited file.** It records a
  SHA-256 of everything it writes in `lib/toolkit/MANIFEST.txt`; on the next
  sync a file that no longer matches was edited in the consumer, so it is left
  alone, reported, and the run exits non-zero. `-Force` takes the toolkit's
  copy after saving the local one as `<name>.local-<timestamp>`. This exists
  for `force-profiles.ini`: a tune arrived at by driving should survive a
  routine version bump.
- **`tools/Import-Profile.ps1`** promotes a tune from a game folder's
  `force-profiles.user.ini` into `profiles/force-profiles.ini` as the next free
  version of its family. It never overwrites a published version, keeps
  `inherits` so the tune is not silently altered, and refuses if the parent is
  not published.

## [0.3.0] - 2026-09-04

The force model becomes data, and native mods get it too.

### Added

- **`profiles/force-profiles.ini`** — every parameter that decides how a game
  feels, as a named, versioned profile the game **deploys and reads at
  runtime**. Swapping a tune is a text edit and a relaunch, not a build, so
  A/B-ing two tunes is finally practical. Versions are immutable; `inherits`
  keeps a new one to the lines that changed; a player's
  `force-profiles.user.ini` is never overwritten and wins outright; an
  unrecognised key is reported, not ignored. Four profiles ship:
  `arcade-outrun@1` (OutRun's own constants), `arcade-outrun@2` (a lighter
  variant to drive against it), `simlite@1` (art of rally 0.2.2) and
  `arcade-generic@1`.
- **`native/forcemodel/force_model.h` and `force_profile.h`** — the C++ port of
  `ForceModel` and `ForceShaper`, so a native mod runs the shared model instead
  of its own copy. This was the piece left undone when OutRun's device layer
  moved in 0.2.0, and the two implementations had already drifted.
- **`ForceProfile`** on the managed side, reading the same files.
- **A cross-language golden file.** `conformance.expected.txt` is produced and
  checked by both implementations: every shipped profile through the same
  200-tick sequence, agreeing within 1e-4. A change in how any game feels is
  now a diff in the repo rather than a surprise at the wheel.
- **`-Parts profiles`** in `Sync-Toolkit.ps1`.

### Changed

- **Six constants inside the model turned out to be feel decisions** and are now
  parameters: the damper's authority at a standstill, how far past the ideal
  slip angle the trail collapses, the weight-transfer bounds, the texture
  frequency, and the impact and shift pulse durations. Defaults are the values
  that were hardcoded, so nothing moves until someone moves it.
- `ForceModelSettings.Arcade()` is documented as **not** OutRun's tuning;
  `arcade-outrun@1` is the one that matches the game.

## [0.2.1] - 2026-09-04

### Fixed

- **`Set-IniValueCrlf` duplicated a key written as `key = value`.** The key
  regex required the tight `key=value` form, so on a config using spaces around
  the separator - OutRun 2006's does - it appended a second line instead of
  replacing the first, leaving two copies of the key and letting the game's
  parser decide which wins. Found by using it on a real game config. An
  existing line now keeps its indent and separator exactly; a newly appended
  key copies the spacing of a sibling key in its section. Four cases added,
  plus an assertion on every existing case that the key appears exactly once
  afterwards.

### Added

- **`docs/CONSUMERS.md`** - the register of who vendors what, where each pin
  lives, and which area of a release obliges which consumer to move.
- **`tools/Get-ConsumerStatus.ps1`** - reads each consumer's real pin off disk
  and prints the `Sync-Toolkit.ps1` command for anything behind, so the
  register cannot quietly go stale.

## [0.2.0] - 2026-09-04

The native half. 0.1.0 could serve a Unity mod; this release is what a native
mod - a dinput8 proxy - needs, and the three consumers that needed it are on it.

### Added

- **Hardware periodic effects** in `native/wheelffb`: `CreatePeriodicEffect`
  (returns a slot), `UpdatePeriodicEffect` (envelope, safe to call every frame),
  `ReleasePeriodicEffects`. Road texture and tyre slip rendered by the wheelbase
  instead of synthesised through the constant force, which loses ~26% to
  zero-order-hold roll-off at 60 Hz and another 30-60% to soft saturation when
  it rides on steering load. Carries OutRun2006Tweaks-FFB's driver
  update-strategy probe (`SetParameters` without `DIEP_START` preferred),
  its recreate-after-handle-loss hold-off, and its change gate.
- **`SetPreferredDeviceGuid`**: pick the wheel by DirectInput instance GUID,
  beating index and name. For a mod whose own input layer already chose a
  device - matching by name is guesswork when a Fanatec base shows two
  identically named devices.
- **`native/wheelffb/include/wheelffb.h`**: the C ABI as one macro list, with a
  runtime loader (`WheelFfb_LoadBeside`) so a missing DLL disables force
  feedback rather than stopping the game from starting. `build.ps1` fails if the
  header and `WheelFfb.def` disagree.
- **`native/forza/forza_packet.h`**: the Forza encoder as a header-only C++11
  file for native mods, with a conformance test mirroring the C# one.
- **x86 build**: `build.bat x86`, for 32-bit games. Both builds verified at 32
  undecorated exports.
- **`native/wheelffb/test`**: a smoke test that loads the DLL the way a proxy
  does and walks every stage against a real wheel, applying no torque unless
  asked.
- **`tools/powershell/DbceWheel.psm1`**: registry Steam discovery, DirectInput
  enumeration with the two COM marshalling workarounds, CRLF-safe INI editing,
  and a test suite.

### Fixed

- **An effect whose `Start` lost a race with a focus change never played.**
  `Start` returning `DIERR_NOTEXCLUSIVEACQUIRED` was survivable for the constant
  force, because every update carries `DIEP_START`, but not for a periodic
  effect, whose updates deliberately do not. `Start` is now retried through a
  re-acquire, each slot remembers whether it truly started, and until it has,
  its updates carry `DIEP_START` whatever the probed strategy says. Found by the
  new smoke test on its first run.
- **`DirectInput8Create` is resolved from `system32` by full path.** Inside a
  game modded with a `dinput8.dll` proxy, an import bound to the proxy beside
  the game exe - the layer the toolkit is meant to sit alongside, not inside.
- **`Set-IniValueCrlf` split a CRLF** when appending a key to a file's last
  section: .NET's `\Z` also matches before a trailing newline. Latent in
  milestone-wheel-tools, which only ever replaced.

### Consumers

- **iracing-arcade-wheel** on the telemetry library and the native DLL.
- **milestone-wheel-tools** on `forza_packet.h` and `DbceWheel.psm1`; Steam
  discovery had been written out three times in that one repo.
- **OutRun2006Tweaks-FFB** on the C ABI: 543 lines of its own DirectInput code
  removed, its force model untouched.

## [0.1.0] - 2026-09-03

First release: the foundation, built while the survey of five racing-mod repos
was fresh. Nothing here is new capability; it is the capability that existed in
two to six copies, made one.

### Added

- **`native/wheelffb` — `WheelFfb.dll`**, the DirectInput force-feedback and
  device layer, seeded from art-of-sim-rally's `UnityForceFeedback.dll` (the
  superset) with OutRun2006Tweaks-FFB's lifecycle rules and cruisn-collection's
  hold watchdog folded in: one DirectInput instance per process, FFB-candidate
  fallback when the preferred device has no actuator, device capability query,
  axis reading through the held handle, `NOTEXCLUSIVEACQUIRED` re-acquire,
  own-window binding, `PanicStop`, `InstallExitGuards`, ramp-in after effect
  recreation, `SetHoldTimeoutMs`, `SetLogPath`, `GetLastHResult`,
  `GetWheelFfbVersion`. All 21 original exports kept. 28 exports verified by
  the build.
- **`Dbce.Wheel.Telemetry`** (`netstandard2.0`): the Forza "Data Out" encoder
  in all three layouts (232 sled, 311 FM7 dash, 324 Horizon), byte-identical
  to art-of-sim-rally's for Horizon; `IdleKeeper` (never a degenerate zero
  packet; gear 1); `RpmSynthesizer`. 39 tests, including the original suite.
- **`Dbce.Wheel.Ffb`** (`netstandard2.0`): `WheelFfbNative` binding that loads
  by absolute path on Mono and CoreCLR; `ForceShaper` conditioning chain;
  `ForceModel` with `SimLite` and `Arcade` presets and `InputValidator`
  (dead-input and sign-by-speed-band checks); `DevicePreference` (vid:pid /
  #index / name, virtual-pad blocklist); `FfbDiagnostics`. 21 tests.
- **Tools**: one `forza_probe.py` and one `forza_synth.py` (any layout),
  `ffbprobe` (scripted constant-force encodings against a real base),
  `press.ps1`/`shot.ps1` for unattended tests, `Sync-Toolkit.ps1` for
  consumers, `package.ps1`.
- **Knowledge base**: 15 pages with `[repo, date]` provenance and five
  per-game case files.
- **Docs**: proposal, implementation plan, rebuild roadmap, ADR-0001 (FFB
  output backends: DirectInput primary, SDL2 secondary, one interface).

### Not yet

SDL2 backend; direct-read input kit and Unity Input System kit; templates;
consumer integration (art-of-sim-rally first).
