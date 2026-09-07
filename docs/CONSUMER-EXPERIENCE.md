# Building a clearer wheel setup and recovery experience

Reviewed 2026-09-07 against toolkit b726d56 and local art-of-sim-rally,
iracing-arcade-wheel and OutRun2006Tweaks-FFB setup surfaces. This document
separates shared capabilities delivered by this change from consumer UI work
still to do. No game or physical force validation is implied.

## Available shared support

### Explain device selection before opening a wheel

`DevicePreference.Resolve(snapshot)` is pure and returns a
`DeviceSelectionResult`: stable `Status`, suggested `Message`, `Success`,
`ArrayPosition`, nullable `Device`, and nullable `PersistentId`.

| Status | Consumer experience |
|---|---|
| Automatic | Offer the available wheel; save its identity only after the player chooses it. |
| Matched | Show the saved wheel as available, not yet connected for force output. |
| NoDevices | Offer Refresh and explain that the wheel needs connection and power. |
| Missing | Keep the saved identity visible. Offer reconnect/refresh or explicit replacement. |
| Ambiguous | Show each matching device with capabilities and full instance GUID in details. |
| NoForceFeedback | Explain that this device cannot provide force feedback; retain it for appropriate input roles. |
| VirtualDevice | Ask for the physical wheel. Virtual FFB candidates are blocked even when explicitly selected. |

Use the enum for localization; the English message is a usable default.
`ArrayPosition` indexes the supplied snapshot; `Device.Value.Index` is the
backend index. Never confuse the two after sorting a picker. `PersistentId`
is the instance GUID when known, not a name guessed to be unique. Do not
silently promote old name/index preferences just because they resolved once.

```csharp
var result = savedPreference.Resolve(WheelFfbNative.ListFfbDevices());
// Draw result.Message, plus Refresh/Choose controls in the consumer's UI.
if (result.Success && playerConfirmedChoice && result.PersistentId != null)
    settings.SavedWheelGuid = result.PersistentId;
```

A successful result describes the enumeration snapshot only. Opening the device
must use strict native selection for a saved GUID, since it can disappear or
fail to open after enumeration. Refresh never needs to apply force. A failed
switch must keep input/shifter controls usable and show the failure next to the
picker rather than only in a log.

### Distinguish calculation from output acceptance

`FfbDiagnostics.RecordOutput` records a computed sample, not physical output.
Call `RecordDelivery(accepted, error)` only after actually calling the output API:

```csharp
bool accepted = WheelFfbNative.SetForce(deviceInteger);
diagnostics.RecordDelivery(accepted, accepted ? null : WheelFfbNative.LastError);
```

The additive `DeliveryAttempts`, `DeliveryFailures`, nullable
`LastDeliverySucceeded`, and `LastDeliveryError` support a small status indicator.
An unreported delivery remains unknown; it never looks successful. A later
accepted call clears the latest delivery error but keeps cumulative failure
counts until `Reset`. Even accepted output does not prove wheel movement.
`Summary()` now says samples were *computed* and separately lists acceptance.
Consumers parsing old prose should move to the structured properties.

### Recover from invalid force samples

Both C# and C++ model/shaper implementations expose `LastInputRejected`
(`last_input_rejected` in C++). Non-finite inputs/settings or overflowing output
clear their internal state and return zero. The next valid call starts cleanly;
no restart is required. Missing optional signals may retain non-finite values
when their `Has*` flag is false. Settings are checked even for disabled effects,
so a malformed tune is visible instead of latent.

When using the model and shaper separately, a rejected model input must also
reset the shaper, otherwise its previous smoothing tail can survive:

```csharp
float raw = model.Compute(inputs, dt);
float output;
if (model.LastInputRejected) { shaper.Reset(); output = 0f; }
else output = shaper.Shape(raw, speedKmh, dt, model.LastWasEvent);
// Report model.LastInputRejected || shaper.LastInputRejected as unavailable data,
// not as a reason to increase wheel strength. Send the zero output as usual.
```

Hysteresis now passes sign reversals and cannot override a lowered peak limit.
Existing versioned profile values are unchanged. These are behavior corrections,
so consumers should record the toolkit version alongside the profile ID.

## Next consumer improvements, in payoff order

1. **One setup status beside the wheel picker.** Distinguish loading failure,
   saved device absent, ready for output, inactive outside driving, and latest
   output rejected. Art-of-sim-rally has a picker-local status surface; extend it
   with the structured selection and delivery results. Do not expose raw HRESULT
   as the sole explanation; keep it in expandable details/support bundles.
2. **Remember each input role by identity.** Wheel, pedals and shifter can be
   different devices. art-of-sim-rally's current shifter picker still saves
   index/name. Reuse GUID enumeration there, retaining disconnected bindings
   until the player explicitly replaces them. Input selection needs its own
   resolver that permits non-FFB devices; do not use the FFB-only resolver.
3. **A no-force input check.** Show steering direction and live pedal/shifter
   states before a drive. Let the player verify travel and inversion, with an
   explicit Save/Cancel step. Keep calibration and deadzones per input role.
   Shared calibration data/validation is a future input-kit feature; it is not
   implemented by this release.
4. **Explain the active tune and provide a way back.** Display requested profile,
   actually loaded profile/version, local overrides and parse/unknown-key errors.
   Preview a proposed tune without changing the active one, then apply only a
   valid complete model/shaper pair. Keep the last working choice on failure.
   OutRun's `FFBProfile=legacy` should be visible as legacy, not mistaken for an
   active toolkit tune. A shared profile-preview result is future work.
5. **Make support evidence easy to share.** Include toolkit/native versions,
   selected identity, active profile, computed/accepted counts, invalid-sample
   state and recent errors in an opt-in support bundle. Keep paths and device
   identifiers reviewable before sharing. Rate-limit UI updates and log only
   transitions, retaining counters for frequent transient failures.

Keep UI drawing, driving/menu detection, preferences and localization in each
consumer. The toolkit supplies facts and decisions so Unity Mono, IL2CPP and
native consumers can build consistent experiences without taking a UI dependency.
