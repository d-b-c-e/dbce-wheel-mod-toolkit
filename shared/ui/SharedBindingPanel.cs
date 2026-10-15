// MIT. Extracted from iRacing Arcade SharedBindingPanel: presentation only.
// Uses the existing shared binding contract and selection policy; owns no reader or writer.
#nullable disable
using System;
using Dbce.Wheel.Input;

namespace Dbce.Wheel.Ui
{
    public interface ISharedBindingPanelUi
    {
        void Label(string text);
        void Help(string text);
        bool Button(string text, bool enabled = true);
    }

    /// <summary>One selected action at a time keeps the same controls usable in a bounded rectangle.
    /// Devices are only the host's accepted snapshot, never an OS discovery result.</summary>
    public sealed class SharedBindingPanel
    {
        private readonly SharedBindingDeviceSelection selection = new SharedBindingDeviceSelection();
        private int action, devicePage;
        private object previousHost;

        public void Draw(ISharedBindingHost host, ISharedBindingPanelUi ui)
        {
            if (host == null || ui == null) throw new ArgumentNullException();
            if (!ReferenceEquals(previousHost, host)) { action = devicePage = 0; previousHost = host; }
            string capturing = host.CapturingAction;
            bool listening = capturing != null;
            ui.Label("Shared controls");
            ui.Help("Active: " + host.ActiveSource);
            ui.Help(host.AccessReason);
            if (host.SavedPendingRestart) ui.Help("Saved for next start. Active bindings are unchanged; restart to use saved controls.");
            ui.Help("Choose an already-open wheel, shifter or other device for each action. A new or disconnected device needs the matching Controls Setup download with the game closed. Wheelkit is optional.");
            ui.Help(host.CaptureMessage);
            if (listening)
            {
                ui.Label("Listening for " + capturing + " · " + Math.Ceiling(host.CaptureSecondsLeft) + " s");
                if (ui.Button("Cancel capture")) host.CancelCapture();
            }

            var rows = host.Rows;
            if (rows.Count == 0) { ui.Help("No admitted control rows are available."); return; }
            action = Math.Max(0, Math.Min(action, rows.Count - 1));
            var row = rows[action];
            ui.Label((action + 1) + " / " + rows.Count + ": " + row.Label);
            if (ui.Button("Previous action", !listening && action > 0)) action--;
            if (ui.Button("Next action", !listening && action + 1 < rows.Count)) action++;
            ui.Help("Active: " + row.Active.Text);
            if (row.NextStart.HasValue) ui.Help("Next start: " + row.NextStart.Value.Text);
            if (row.Draft.HasValue) ui.Help("Draft: " + row.Draft.Value.Text);
            if (!string.IsNullOrEmpty(row.Note)) ui.Help(row.Note);
            bool editable = SharedBindingUiPolicy.CanEditRow(host.Access, capturing, row.Note);

            var devices = host.Devices;
            selection.Reconcile(host, devices);
            if (ui.Button("Use this action's saved device", editable)) selection.SelectSaved();
            devicePage = Math.Max(0, Math.Min(devicePage, Math.Max(0, (devices.Count - 1) / 3)));
            for (int i = devicePage * 3; i < Math.Min(devices.Count, devicePage * 3 + 3); i++)
            {
                var device = devices[i];
                bool selected = selection.Mode == SharedBindingSelectionMode.Explicit &&
                    string.Equals(selection.InstanceGuid, device.InstanceGuid, StringComparison.OrdinalIgnoreCase);
                if (ui.Button((selected ? "Selected: " : "Use: ") + device.Name, editable))
                    selection.SelectDevice(device.InstanceGuid, devices);
            }
            if (devices.Count > 3 && ui.Button("More open devices", editable))
                devicePage = (devicePage + 1) % ((devices.Count + 2) / 3);
            if (devices.Count == 0) ui.Help("No fresh admitted capture device. Saved bindings are retained.");
            if (!selection.CanBind) ui.Help(selection.UnavailableReason);
            if (ui.Button("Bind " + row.Label, editable && selection.CanBind && devices.Count > 0))
            {
                if (selection.Mode == SharedBindingSelectionMode.Saved) host.BeginCapture(row.Action);
                else host.BeginCapture(row.Action, selection.InstanceGuid);
            }
            if (ui.Button("Clear " + row.Label, editable && row.Action != "steer")) host.Clear(row.Action);
            if (row.IsAxis && ui.Button("Invert " + row.Label + ": " +
                ((row.DraftInverted ?? row.NextStartInverted ?? row.ActiveInverted) ? "On" : "Off"), editable))
                host.SetInverted(row.Action, !(row.DraftInverted ?? row.NextStartInverted ?? row.ActiveInverted));
            if (ui.Button("Save bindings for next start", host.Access == SharedBindingAccess.Editable && !listening && host.HasDraft))
                host.Save(out _);
            if (ui.Button("Discard unsaved edits", !listening && host.HasDraft)) host.Discard();
        }
    }
}
