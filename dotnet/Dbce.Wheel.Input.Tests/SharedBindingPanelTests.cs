using Dbce.Wheel.Input;
using Dbce.Wheel.Ui;
#if DBCE_INPUT_NET35
using Rows = System.Collections.Generic.IList<Dbce.Wheel.Input.SharedBindingRowView>;
using Devices = System.Collections.Generic.IList<Dbce.Wheel.Input.SharedBindingDeviceView>;
#else
using Rows = System.Collections.Generic.IReadOnlyList<Dbce.Wheel.Input.SharedBindingRowView>;
using Devices = System.Collections.Generic.IReadOnlyList<Dbce.Wheel.Input.SharedBindingDeviceView>;
#endif

internal static class SharedBindingPanelTests
{
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string why) { checks++; if (!ok) throw new Exception(why); }
        var row = new SharedBindingRowView { Action = "shiftUp", Label = "Shift up", Note = "Select Sequential in setup." };
        var unsupported = new SharedBindingRowView { Action = "clutch", Label = "Clutch", Note = "This adapter has no clutch.", Applicability = SharedBindingApplicability.Unsupported };
        var host = new Host { Rows = new[] { row, unsupported } };
        var ui = new Ui();
        var panel = new SharedBindingPanel();
        panel.Draw(host, ui, "");
        Check(ui.Text.Contains("Selected: Shift up") && ui.Text.Contains(row.Note), "conditional supported row stays selected and explains refusal");
        Check(ui.Buttons["Show unsupported controls (1)"] && !ui.Buttons["Bind Shift up"] && !ui.Buttons["Clear Shift up"], "only explicit unsupported row is grouped, temporary row cannot mutate");
        row.Note = "";
        ui.Reset(); panel.Draw(host, ui, "");
        Check(ui.Buttons["Bind Shift up"] && ui.Text.Contains("Selected: Shift up"), "resolved temporary condition reenables the same selected row");
        host.Access = SharedBindingAccess.ReadOnly;
        host.HasDraft = true;
        host.Rows = new[] { unsupported };
        ui.Reset(); panel.Draw(host, ui, "Bindings not saved — reader unavailable");
        Check(ui.Buttons["Discard unsaved edits"] && !ui.Buttons["Save bindings for next start"], "all unsupported rows do not hide retained draft discard or bypass Save access");
        Check(ui.Text.Contains("Bindings not saved — reader unavailable"), "all unsupported rows retain Save failure");
        host.Rows = Array.Empty<SharedBindingRowView>();
        ui.Reset(); ui.Click = "Discard unsaved edits"; panel.Draw(host, ui, "");
        Check(host.Discarded && host.Saves == 0, "no rows still permits explicit discard without Save");
        host.CapturingAction = "steer";
        ui.Reset(); panel.Draw(host, ui, "");
        Check(!ui.Buttons["Discard unsaved edits"] && ui.Buttons["Cancel capture"], "capture keeps discard locked and Cancel available");
        return checks;
    }

    private sealed class Ui : ISharedBindingPanelUi
    {
        internal readonly List<string> Text = new();
        internal readonly Dictionary<string, bool> Buttons = new();
        internal string Click = "";
        public void Label(string text) => Text.Add(text);
        public void Help(string text) => Text.Add(text);
        public bool Button(string text, bool enabled = true) { Text.Add(text); Buttons[text] = enabled; return enabled && text == Click; }
        internal void Reset() { Text.Clear(); Buttons.Clear(); Click = ""; }
    }
    private sealed class Host : ISharedBindingHost
    {
        public SharedBindingAccess Access { get; set; } = SharedBindingAccess.Editable;
        public string AccessReason => "";
        public string ActiveSource => "fixture";
        public Rows Rows { get; set; } = Array.Empty<SharedBindingRowView>();
        public Devices Devices { get; } = new[] { new SharedBindingDeviceView { InstanceGuid = "11111111-1111-1111-1111-111111111111", Name = "Wheel" } };
        public string? CapturingAction { get; set; }
        public double CaptureSecondsLeft => 1;
        public string CaptureMessage => "";
        public bool HasDraft { get; set; }
        public bool SavedPendingRestart => false;
        public void BeginCapture(string action) => throw new Exception("Unexpected capture");
        public void BeginCapture(string action, string instanceGuid) => throw new Exception("Unexpected capture");
        public void CancelCapture() { }
        public void Clear(string action) => throw new Exception("Unexpected clear");
        public void SetInverted(string action, bool inverted) => throw new Exception("Unexpected inversion");
        internal int Saves;
        internal bool Discarded;
        public bool Save(out string message) { Saves++; message = ""; return true; }
        public void Discard() { Discarded = true; }
    }
}
