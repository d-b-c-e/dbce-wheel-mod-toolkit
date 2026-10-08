// dbce-virtual-pad: a virtual Xbox 360 controller (ViGEmBus) driven by a command file, so
// unattended game tests can navigate menus without focus, mouse or keyboard injection.
//
// Commands, one per line, in %LOCALAPPDATA%\dbce\vpad\cmd.txt (consumed and deleted):
//   press A|B|X|Y|Start|Back|LB|RB|LS|RS|Guide|Up|Down|Left|Right [ms=150]
//   hold <button> / release <button>
//   stick L|R <x -1..1> <y -1..1> [ms=0 = until changed]
//   trigger L|R <0..1> [ms=0]
//   wait <ms>
//   quit
// Durations accept either 200 or ms=200, from 0 to 60000. Invalid explicit
// durations are rejected before any controller state is changed.
// Exits (removing the controller) on quit or after --idle-minutes without commands (30).
// --slot N (N > 0) runs another controller with its own folder, %LOCALAPPDATA%\dbce\vpad-N (cmd.txt, log.txt), for
// local multiplayer / split-screen tests; slot 0 (the default) keeps the folder above.
using System.Diagnostics;
using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.Xbox360;

double idleMinutes = 30; int slot = 0;
for (int i = 0; i + 1 < args.Length; i += 2)
{
    if (args[i] == "--idle-minutes" && double.TryParse(args[i + 1], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var minutes)) idleMinutes = minutes;
    else if (args[i] == "--slot" && int.TryParse(args[i + 1], out var n) && n >= 0 && n <= 3) slot = n;
}
var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "dbce", slot == 0 ? "vpad" : "vpad-" + slot);
Directory.CreateDirectory(dir);
var cmdPath = Path.Combine(dir, "cmd.txt");
var logPath = Path.Combine(dir, "log.txt");

void Log(string s) { var line = $"{DateTime.Now:HH:mm:ss.fff} {s}"; Console.WriteLine(line); try { File.AppendAllText(logPath, line + Environment.NewLine); } catch { } }

using var client = new ViGEmClient();
var pad = client.CreateXbox360Controller();
pad.Connect();
try
{
    var startupClock = Stopwatch.StartNew();
    int inputIndex = NeutralStartup.Ensure(
        () => { try { return pad.UserIndex; } catch (Nefarius.ViGEm.Client.Targets.Xbox360.Exceptions.Xbox360UserIndexNotReportedException) { return null; } },
        XInputReadback.Read,
        () => { pad.SetAxisValue(Xbox360Axis.LeftThumbX, 1); Thread.Sleep(10); pad.ResetReport(); pad.SubmitReport(); },
        Thread.Sleep, () => startupClock.ElapsedMilliseconds);
    Log($"connected virtual Xbox 360 controller (command slot {slot}, XInput index {inputIndex}, neutral readback verified); watching {cmdPath}; idle exit after {idleMinutes} min");
}
catch
{
    pad.Disconnect();
    throw;
}

var buttons = new Dictionary<string, Xbox360Button>(StringComparer.OrdinalIgnoreCase)
{
    ["A"] = Xbox360Button.A, ["B"] = Xbox360Button.B, ["X"] = Xbox360Button.X, ["Y"] = Xbox360Button.Y,
    ["Start"] = Xbox360Button.Start, ["Back"] = Xbox360Button.Back, ["Guide"] = Xbox360Button.Guide,
    ["LB"] = Xbox360Button.LeftShoulder, ["RB"] = Xbox360Button.RightShoulder,
    ["LS"] = Xbox360Button.LeftThumb, ["RS"] = Xbox360Button.RightThumb,
    ["Up"] = Xbox360Button.Up, ["Down"] = Xbox360Button.Down, ["Left"] = Xbox360Button.Left, ["Right"] = Xbox360Button.Right,
};
short Axis(double v) => (short)Math.Clamp(Math.Round(v * 32767), -32767, 32767);
var idle = Stopwatch.StartNew();
bool quit = false;

while (!quit && idle.Elapsed.TotalMinutes < idleMinutes)
{
    Thread.Sleep(100);
    if (!File.Exists(cmdPath)) continue;
    string[] lines;
    try { lines = File.ReadAllLines(cmdPath); File.Delete(cmdPath); } catch { continue; }
    idle.Restart();
    foreach (var raw in lines)
    {
        var p = raw.Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (p.Length == 0) continue;
        int Ms(int i, int fallback) => CommandDuration.Read(p, i, fallback);
        try
        {
            switch (p[0].ToLowerInvariant())
            {
                case "press" when p.Length > 1 && buttons.TryGetValue(p[1], out var b):
                    int pressMs = Ms(2, 150);
                    pad.SetButtonState(b, true); Thread.Sleep(pressMs); pad.SetButtonState(b, false); Thread.Sleep(80); break;
                case "hold" when p.Length > 1 && buttons.TryGetValue(p[1], out var hb): pad.SetButtonState(hb, true); break;
                case "release" when p.Length > 1 && buttons.TryGetValue(p[1], out var rb): pad.SetButtonState(rb, false); break;
                case "stick" when p.Length > 3:
                    int stickMs = Ms(4, 0);
                    bool left = p[1].Equals("L", StringComparison.OrdinalIgnoreCase);
                    double x = double.Parse(p[2], System.Globalization.CultureInfo.InvariantCulture), y = double.Parse(p[3], System.Globalization.CultureInfo.InvariantCulture);
                    pad.SetAxisValue(left ? Xbox360Axis.LeftThumbX : Xbox360Axis.RightThumbX, Axis(x));
                    pad.SetAxisValue(left ? Xbox360Axis.LeftThumbY : Xbox360Axis.RightThumbY, Axis(y));
                    if (stickMs > 0) { Thread.Sleep(stickMs); pad.SetAxisValue(left ? Xbox360Axis.LeftThumbX : Xbox360Axis.RightThumbX, 0); pad.SetAxisValue(left ? Xbox360Axis.LeftThumbY : Xbox360Axis.RightThumbY, 0); }
                    break;
                case "trigger" when p.Length > 2:
                    int triggerMs = Ms(3, 0);
                    var slider = p[1].Equals("L", StringComparison.OrdinalIgnoreCase) ? Xbox360Slider.LeftTrigger : Xbox360Slider.RightTrigger;
                    pad.SetSliderValue(slider, (byte)Math.Clamp(Math.Round(double.Parse(p[2], System.Globalization.CultureInfo.InvariantCulture) * 255), 0, 255));
                    if (triggerMs > 0) { Thread.Sleep(triggerMs); pad.SetSliderValue(slider, 0); }
                    break;
                case "wait": Thread.Sleep(Ms(1, 500)); break;
                case "quit": quit = true; break;
                default: Log($"unknown: {raw}"); continue;
            }
            Log($"did: {raw.Trim()}");
        }
        catch (Exception ex) { Log($"failed: {raw.Trim()}: {ex.Message}"); }
    }
}
pad.Disconnect();
Log(quit ? "quit; controller removed" : "idle timeout; controller removed");
