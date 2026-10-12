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
// --target ds4 (opt-in; x360 is the default and unchanged) runs a virtual DualShock 4 instead, a plain HID gamepad that
// background DirectInput readers see. The same commands map by position (Ds4Mapping: A = Cross, B = Circle, X = Square,
// Y = Triangle, Start = Options, Back = Share, LB/RB = L1/R1, LS/RS = L3/R3, Guide = PS, Up/Down/Left/Right = the
// d-pad hat, triggers = L2/R2 with their digital bits). A DualShock 4 has no XInput readback: the startup submits a
// neutral report and says so; a caller admits it by its own neutral read (hub outrun-shim-pad-input-20261012 C1b/C1c).
using System.Diagnostics;
using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.DualShock4;
using Nefarius.ViGEm.Client.Targets.Xbox360;

double idleMinutes = 30; int slot = 0; string target = "x360";
for (int i = 0; i + 1 < args.Length; i += 2)
{
    if (args[i] == "--idle-minutes" && double.TryParse(args[i + 1], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var minutes)) idleMinutes = minutes;
    else if (args[i] == "--slot" && int.TryParse(args[i + 1], out var n) && n >= 0 && n <= 3) slot = n;
    else if (args[i] == "--target" && args[i + 1] is "x360" or "ds4") target = args[i + 1];
}
var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "dbce", slot == 0 ? "vpad" : "vpad-" + slot);
Directory.CreateDirectory(dir);
var cmdPath = Path.Combine(dir, "cmd.txt");
var logPath = Path.Combine(dir, "log.txt");

void Log(string s) { var line = $"{DateTime.Now:HH:mm:ss.fff} {s}"; Console.WriteLine(line); try { File.AppendAllText(logPath, line + Environment.NewLine); } catch { } }

using var client = new ViGEmClient();
// The target's four operations; the command loop below uses only these.
Func<string, bool> isButton; Action<string, bool> setButton; Action<bool, double, double> setStick; Action<bool, double> setTrigger; Action disconnect;
if (target == "ds4")
{
    var ds4 = client.CreateDualShock4Controller();
    ds4.Connect();
    try
    {
        ds4.ResetReport(); ds4.SubmitReport();
        Log($"connected virtual DualShock 4 controller (command slot {slot}, neutral report submitted; no XInput readback for a DualShock 4); watching {cmdPath}; idle exit after {idleMinutes} min");
    }
    catch
    {
        ds4.Disconnect();
        throw;
    }
    static DualShock4Button Ds4Button(string name) => name switch
    {
        "Cross" => DualShock4Button.Cross, "Circle" => DualShock4Button.Circle, "Square" => DualShock4Button.Square,
        "Triangle" => DualShock4Button.Triangle, "Options" => DualShock4Button.Options, "Share" => DualShock4Button.Share,
        "ShoulderLeft" => DualShock4Button.ShoulderLeft, "ShoulderRight" => DualShock4Button.ShoulderRight,
        "ThumbLeft" => DualShock4Button.ThumbLeft, "ThumbRight" => DualShock4Button.ThumbRight,
        _ => throw new ArgumentException(name),
    };
    static DualShock4DPadDirection Ds4Hat(string name) => name switch
    {
        "North" => DualShock4DPadDirection.North, "Northeast" => DualShock4DPadDirection.Northeast, "East" => DualShock4DPadDirection.East,
        "Southeast" => DualShock4DPadDirection.Southeast, "South" => DualShock4DPadDirection.South, "Southwest" => DualShock4DPadDirection.Southwest,
        "West" => DualShock4DPadDirection.West, "Northwest" => DualShock4DPadDirection.Northwest, _ => DualShock4DPadDirection.None,
    };
    var held = new HashSet<string>(StringComparer.OrdinalIgnoreCase);   // the d-pad words held now
    isButton = name => Ds4Mapping.Buttons.ContainsKey(name) || Ds4Mapping.DPad.Contains(name) || name.Equals("Guide", StringComparison.OrdinalIgnoreCase);
    setButton = (name, down) =>
    {
        if (Ds4Mapping.DPad.Contains(name))
        {
            if (down) held.Add(name); else held.Remove(name);
            ds4.SetDPadDirection(Ds4Hat(Ds4Mapping.Hat(held.Contains("Up"), held.Contains("Down"), held.Contains("Left"), held.Contains("Right"))));
        }
        else if (name.Equals("Guide", StringComparison.OrdinalIgnoreCase)) ds4.SetButtonState(DualShock4SpecialButton.Ps, down);
        else ds4.SetButtonState(Ds4Button(Ds4Mapping.Buttons[name]), down);
    };
    setStick = (left, x, y) =>
    {
        ds4.SetAxisValue(left ? DualShock4Axis.LeftThumbX : DualShock4Axis.RightThumbX, Ds4Mapping.Stick(x, false));
        ds4.SetAxisValue(left ? DualShock4Axis.LeftThumbY : DualShock4Axis.RightThumbY, Ds4Mapping.Stick(y, true));
    };
    setTrigger = (left, v) =>
    {
        byte b = Ds4Mapping.Trigger(v);
        ds4.SetSliderValue(left ? DualShock4Slider.LeftTrigger : DualShock4Slider.RightTrigger, b);
        ds4.SetButtonState(left ? DualShock4Button.TriggerLeft : DualShock4Button.TriggerRight, b > 0);
    };
    disconnect = ds4.Disconnect;
}
else
{
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
    isButton = buttons.ContainsKey;
    setButton = (name, down) => pad.SetButtonState(buttons[name], down);
    setStick = (left, x, y) =>
    {
        pad.SetAxisValue(left ? Xbox360Axis.LeftThumbX : Xbox360Axis.RightThumbX, Axis(x));
        pad.SetAxisValue(left ? Xbox360Axis.LeftThumbY : Xbox360Axis.RightThumbY, Axis(y));
    };
    setTrigger = (left, v) => pad.SetSliderValue(left ? Xbox360Slider.LeftTrigger : Xbox360Slider.RightTrigger, (byte)Math.Clamp(Math.Round(v * 255), 0, 255));
    disconnect = pad.Disconnect;
}
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
                case "press" when p.Length > 1 && isButton(p[1]):
                    int pressMs = Ms(2, 150);
                    setButton(p[1], true); Thread.Sleep(pressMs); setButton(p[1], false); Thread.Sleep(80); break;
                case "hold" when p.Length > 1 && isButton(p[1]): setButton(p[1], true); break;
                case "release" when p.Length > 1 && isButton(p[1]): setButton(p[1], false); break;
                case "stick" when p.Length > 3:
                    int stickMs = Ms(4, 0);
                    bool left = p[1].Equals("L", StringComparison.OrdinalIgnoreCase);
                    double x = double.Parse(p[2], System.Globalization.CultureInfo.InvariantCulture), y = double.Parse(p[3], System.Globalization.CultureInfo.InvariantCulture);
                    setStick(left, x, y);
                    if (stickMs > 0) { Thread.Sleep(stickMs); setStick(left, 0, 0); }
                    break;
                case "trigger" when p.Length > 2:
                    int triggerMs = Ms(3, 0);
                    bool leftTrigger = p[1].Equals("L", StringComparison.OrdinalIgnoreCase);
                    setTrigger(leftTrigger, double.Parse(p[2], System.Globalization.CultureInfo.InvariantCulture));
                    if (triggerMs > 0) { Thread.Sleep(triggerMs); setTrigger(leftTrigger, 0); }
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
disconnect();
Log(quit ? "quit; controller removed" : "idle timeout; controller removed");
