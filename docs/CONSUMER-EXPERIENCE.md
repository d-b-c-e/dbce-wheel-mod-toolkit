# Building a clearer wheel setup and recovery experience

For the product-level standard, start with [Shared player experience (UX-1)](CONSUMER-UX.md),
[install/setup guidance](CONSUMER-SETUP.md) and the [acceptance checklist](CONSUMER-UX-CHECKLIST.md).
This page is their technical integration companion; the shared APIs do not
automatically change a consuming game's interface.

The 2026-09-16 [Simple/Advanced contract](CONSUMER-SETTINGS-VIEWS.md) defines
default visibility and migration. Keep its saved presentation preference out of
device resolution, FFB/telemetry policy and calibration state. Render both views
over the same consumer settings; a view change must not reconstruct defaults or
reinitialize hardware. Detailed diagnostics belong in Advanced, while actionable
failures and safe recovery remain available in Simple.

Reviewed 2026-09-07 against toolkit b726d56 and local art-of-sim-rally,
iracing-arcade-wheel and OutRun2006Tweaks-FFB setup surfaces. This document
separates shared capabilities delivered by this change from consumer UI work
still to do. No game or physical force validation is implied.

## Available shared support

### Explain device selection before opening a wheel

UX-1's 2026-09-14 revision requires the product's **FFB device** dropdown to
default to **Use steering wheel**. Resolve the saved Steering binding's exact
physical identity; retain an explicit FFB override separately. Construct the
strict GUID preference from that selected identity. The low-level Automatic
result below is an enumeration suggestion, not a replacement for this product
policy and not permission to fall back to the first wheel. The shared library
does not know the consumer's Steering binding or draw its dropdown.

### Input and FFB may report different device identities

During the 2026-09-16 UX rollout, iRacing's Unity Input System Steering binding
exposed HID identity/serial while the toolkit's DirectInput enumeration exposed
instance GUID/name. The current exported enumeration does not provide a verified
HID path/serial association. `DeviceInfo.VendorId`/`ProductId` are nullable and
the current enumeration wrappers do not populate them. A display-name match,
even unique, or VID:PID alone cannot prove the two interfaces are one device.
[iracing-arcade-wheel and toolkit source review, 2026-09-16]

Keep **Use steering wheel** as the default mode, but show an unresolved reason
and an explicit device choice in Simple when identity cannot be established.
The consumer can offer a saved, owner-confirmed association between the exact
Steering identity and the selected physical FFB GUID, separate from an FFB
override. Label that action clearly; do not silently call an old name-derived
GUID an owner-confirmed association. Record the source of the association.
Rebinding Steering must not reuse an association for a different input device;
disconnect/reorder must retain the missing selection and never choose a substitute.
The Steering binding itself must retain that exact identity. A generic legacy
path such as `<Joystick>/stick/x` must not look up the association of whichever
device currently satisfies it. Require an explicit identity-bound Steering rebind,
or atomically upgrade the binding when the owner confirms the association.
Check a previously saved pair of devices A/B: disconnecting A and leaving B as
the only matching input must not silently switch force output to B.
This extra association step is a current capability gap in automatic setup,
not evidence that cross-backend matching has been verified.

An additive shared metadata API would need real cross-backend validation,
including identical devices and reconnect/reorder cases. It is not part of the
UI rollout; do not update a consumer's native pin on the assumption it exists.

### Resolve the saved output identity

`DevicePreference.Resolve(snapshot)` is pure and returns a
`DeviceSelectionResult`: stable `Status`, suggested `Message`, `Success`,
`ArrayPosition`, nullable `Device`, and nullable `PersistentId`.

| Status | Consumer experience |
|---|---|
| Automatic | Inventory suggestion only; default FFB follows the saved Steering identity. Do not open this suggestion as a fallback for absent/unbound Steering. |
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

The following is the **explicit override** save path. Use steering wheel mode
already has the identity from the saved Steering binding; it does not require
a duplicate wheel-choice confirmation.

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
