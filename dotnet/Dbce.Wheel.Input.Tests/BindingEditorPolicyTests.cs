using Dbce.Wheel.Input;

internal static class BindingEditorPolicyTests
{
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string why) { checks++; if (!ok) throw new Exception(why); }
        const string a = "11111111-1111-1111-1111-111111111111", b = "22222222-2222-2222-2222-222222222222";
        var wheel = new SharedBindingDeviceView { InstanceGuid = a, Name = "Wheel", IsDefault = true };
        var shifter = new SharedBindingDeviceView { InstanceGuid = b, Name = "Shifter" };
        var both = new[] { wheel, shifter };
        var host = new object();
        var selection = new SharedBindingDeviceSelection();
        Check(!selection.CanBind && !selection.SelectDevice(a, both), "no capture target before a host is reconciled");
        selection.Reconcile(host, both);
        Check(selection.Mode == SharedBindingSelectionMode.Saved && selection.CanBind && selection.SelectedIndex == -1,
            "initial deliberate Saved semantics stay distinct from the default device");
        Check(selection.SelectDevice(a, both) && selection.SelectedIndex == 0 && selection.InstanceGuid == a, "explicit current wheel is selectable");
        selection.Reconcile(host, new[] { shifter });
        Check(selection.Mode == SharedBindingSelectionMode.Unavailable && !selection.CanBind && selection.SelectedIndex == -1 && selection.InstanceGuid == a,
            "removing A while B remains never substitutes B or Saved");
        Check(selection.UnavailableReason == "The input connection changed. Choose a device again to bind controls.", "missing choice has plain recovery text");
        selection.Reconcile(host, both);
        Check(!selection.CanBind && selection.Mode == SharedBindingSelectionMode.Unavailable, "reappearing A does not silently confirm a missing choice");
        Check(!selection.SelectDevice("not-present", both) && !selection.CanBind, "invalid choice keeps the unavailable latch");
        Check(selection.SelectDevice(b, both) && selection.CanBind && selection.SelectedIndex == 1, "explicit current B resolves the unavailable latch");
        selection.Reconcile(host, Array.Empty<SharedBindingDeviceView>());
        Check(!selection.CanBind && selection.InstanceGuid == b, "zero-device snapshot keeps the explicit missing target");
        selection.SelectSaved();
        selection.Reconcile(host, both);
        Check(selection.CanBind && selection.Mode == SharedBindingSelectionMode.Saved && selection.InstanceGuid == null, "explicit Saved resolves the latch without choosing a device");
        selection.Reconcile(new object(), both);
        Check(selection.Mode == SharedBindingSelectionMode.Saved && selection.CanBind, "intentional Saved mode survives host replacement");
        Check(selection.SelectDevice(a, both), "explicit A after Saved");
        selection.Reconcile(new object(), both);
        Check(!selection.CanBind && selection.Mode == SharedBindingSelectionMode.Unavailable && selection.SelectedIndex == -1, "replacement host requires explicit reconfirmation even with the same GUID");
        Check(selection.SelectDevice(a, both) && selection.CanBind, "same-GUID explicit reselection confirms the replacement host");
        Check(SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.Editable, null, ""), "supported idle row allows editing");
        foreach (var reason in new[] { "DRIVE has no clutch.", "This profile does not select H-pattern.", "Not delivered by this adapter." })
            Check(!SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.Editable, null, reason), "unavailable row refuses edit affordances: " + reason);
        Check(!SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.ReadOnly, null, "") && !SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.Missing, null, ""), "profile access still gates row edits");
        Check(!SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.Editable, "steer", ""), "an active capture locks row edits without hiding Cancel");
        var supported = new SharedBindingRowView { Note = "Select Sequential in setup." };
        Check(supported.Applicability == SharedBindingApplicability.Supported, "old row producers default to supported");
        Check(!SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.Editable, null, supported.Note, supported.Applicability), "temporary reason still blocks editing");
        var unsupported = new SharedBindingRowView { Applicability = SharedBindingApplicability.Unsupported };
        Check(!SharedBindingUiPolicy.CanEditRow(SharedBindingAccess.Editable, null, unsupported.Note, unsupported.Applicability), "explicit unsupported row refuses edits even without Note");
        return checks + SharedBindingPanelTests.Run();
    }
}
