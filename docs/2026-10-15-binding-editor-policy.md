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
the original named batch passed 20 focused checks. Source-linked consumer
presentation fixtures and affected compile checks passed the pending-only batch.
No installed mod, native output, profile format or accepted tuning changes.


## Named qualification

F6-SHARED-0630 retained its qualified iRacing dependency verification and20
shared-policy checks, then stopped at a missing Art fixture StatusCard stub.
The failure receipt was preserved. Fixture-only repair/read-only-card coverage
compiled no new production behavior. F6-SHARED-PENDING-0635 then passed the six
pending steps at 11:35:36.574–11:35:59.911Z on 2026-10-15: Art24 source-linked
presentation checks, DRIVE2 actual panel suites/23 assertions, iRacing96 actual
Panel/Widgets checks and all three affected plugin compiles with0 warnings/errors.
No passed step was repeated. Shared source was49cc168; Art8c7c4de, DRIVE9f0c6a9
and iRacinga144225 all consumed byte-exact policy66B3FE76.

Hub evidence: artifacts/shared-f6-binding-policy-20261015/offline-r1 and offline-r2.
Final result SHA-2569DBC50AFFF857660254713B65C227D9F9754B3FC796FC19F9C70F09990DAB6AC.
All205 dependency inputs and source pins remained exact. Owned processes exited,
lease release/actual absence were checked, and direct FINAL EXIT preceded Dine's
next runtime slot. This qualifies source presentation behavior and compilation;
rendered acceptance and moved physical controls remain separate. Nothing was
packaged or installed; accepted existing Art/DRIVE force and performance remain.
