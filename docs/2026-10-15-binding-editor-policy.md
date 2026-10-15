# Shared binding presentation policy

Sana138771/138886 assigns one reusable presentation policy, adopted by Livo in
Art/DRIVE and Dine in iRacing. Woden is the working reference and stays unchanged.
Source review is recorded in the hub's
`docs/reviews/2026-10-15-shared-f6-binding-policy-livo.md`.

`BindingEditorPolicy.cs` is Unity-free vendorable source using the existing
`BindingEditorContract.cs`. It makes no device/capture/profile/file/force calls.

Freeze API:

- `SharedBindingDeviceSelection.Reconcile(object currentHost, IReadOnlyList<SharedBindingDeviceView> devices)`
  runs before drawing against one current device snapshot.
- `Mode`: Saved, Explicit or Unavailable; `InstanceGuid` preserves a missing
  explicit target. `SelectedIndex` is -1 for Saved/Unavailable, and `CanBind`
  refuses an uninitialized or unavailable selection.
- `IsInitialized` lets a consumer preserve its existing one-time initial default
  selection. `SelectDevice(guid, devices)` accepts only a current explicit
  choice. `SelectSaved()` is a deliberate user choice, never a disappearance
  fallback. `UnavailableReason` supplies a visible recovery explanation.
- Reconcile latches a missing/replaced-host explicit choice Unavailable, even
  when that GUID reappears. Reconcile never changes Saved to a device. After
  invalidation, SelectDevice or SelectSaved is required.
- `SharedBindingUiPolicy.CanEditRow(access, capturingAction, note)` requires
  Editable, no capture, and no unavailable-action/profile-mode Note. Consumer
  locks and bound checks remain additional conditions. Cancel is independent.

Focused fixture entry is `Dbce.Wheel.Input.Tests --binding-editor-policy`;
source is prepared but has not been compiled or executed. Source-linked consumer
presentation fixtures and affected compile checks will share one named batch.
No installed mod, native output, profile format or accepted tuning changes.
