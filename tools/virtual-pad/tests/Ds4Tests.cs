// The DualShock 4 target's mapping (Ds4Mapping.cs), without ViGEm or a device:
//   dotnet run --project tools/virtual-pad/tests/Ds4.Tests.csproj -c Release
int checks = 0;
void Check(bool condition, string why) { if (!condition) throw new Exception(why); checks++; }
// Sticks: 128 at rest; full right 255, full left 1 (symmetric about 128); Y inverted (up is low); clamped.
Check(Ds4Mapping.Stick(0, false) == 128 && Ds4Mapping.Stick(0, true) == 128, "rest is 128");
Check(Ds4Mapping.Stick(1, false) == 255 && Ds4Mapping.Stick(-1, false) == 1, "full right 255, full left 1");
Check(Ds4Mapping.Stick(1, true) == 1 && Ds4Mapping.Stick(-1, true) == 255, "up is low on Y (inverted), down high");
Check(Ds4Mapping.Stick(0.5, false) == 192 && Ds4Mapping.Stick(-0.5, false) == 64, "half travel");
Check(Ds4Mapping.Stick(7, false) == 255 && Ds4Mapping.Stick(-7, true) == 255, "clamped");
// Triggers: 0..255.
Check(Ds4Mapping.Trigger(0) == 0 && Ds4Mapping.Trigger(1) == 255 && Ds4Mapping.Trigger(0.5) == 128, "trigger 0, 255, half");
Check(Ds4Mapping.Trigger(-1) == 0 && Ds4Mapping.Trigger(2) == 255, "trigger clamped");
foreach (var bad in new[] { double.NaN })
{
    bool s = false, t = false;
    try { Ds4Mapping.Stick(bad, false); } catch (FormatException) { s = true; }
    try { Ds4Mapping.Trigger(bad); } catch (FormatException) { t = true; }
    Check(s && t, "NaN refused");
}
// The stick pair: both bytes before any set; an invalid Y (or X) sets nothing at all (a fake setter records calls).
var sets = new List<(byte, byte)>();
Ds4Mapping.Stick(1, 0, (x, y) => sets.Add((x, y)));
Check(sets.Count == 1 && sets[0] == (255, 128), "valid pair set once, X right and Y at rest");
foreach (var (x, y) in new[] { (1.0, double.NaN), (double.NaN, 0.0), (double.NaN, double.NaN) })
{
    sets.Clear(); bool refused = false;
    try { Ds4Mapping.Stick(x, y, (a, b) => sets.Add((a, b))); } catch (FormatException) { refused = true; }
    Check(refused && sets.Count == 0, $"invalid pair ({x}, {y}) refused with no set");
}
// Buttons: the Xbox words by position, case-insensitive; the d-pad words are the hat; nothing else.
var want = new Dictionary<string, string> { ["A"] = "Cross", ["B"] = "Circle", ["X"] = "Square", ["Y"] = "Triangle", ["Start"] = "Options",
    ["Back"] = "Share", ["LB"] = "ShoulderLeft", ["RB"] = "ShoulderRight", ["LS"] = "ThumbLeft", ["RS"] = "ThumbRight" };
foreach (var (k, v) in want) Check(Ds4Mapping.Buttons[k] == v && Ds4Mapping.Buttons[k.ToLowerInvariant()] == v, "button " + k);
Check(Ds4Mapping.Buttons.Count == want.Count, "no other buttons");
foreach (var d in new[] { "Up", "Down", "Left", "Right", "up" }) Check(Ds4Mapping.DPad.Contains(d) && !Ds4Mapping.Buttons.ContainsKey(d), "d-pad " + d);
Check(Ds4Mapping.Guide == "Ps", "Guide is PS");
// The hat: eight directions, none at rest, opposite words cancel.
Check(Ds4Mapping.Hat(false, false, false, false) == "None", "rest");
Check(Ds4Mapping.Hat(true, false, false, false) == "North" && Ds4Mapping.Hat(false, true, false, false) == "South" &&
      Ds4Mapping.Hat(false, false, true, false) == "West" && Ds4Mapping.Hat(false, false, false, true) == "East", "cardinal");
Check(Ds4Mapping.Hat(true, false, false, true) == "Northeast" && Ds4Mapping.Hat(true, false, true, false) == "Northwest" &&
      Ds4Mapping.Hat(false, true, false, true) == "Southeast" && Ds4Mapping.Hat(false, true, true, false) == "Southwest", "diagonal");
Check(Ds4Mapping.Hat(true, true, false, false) == "None" && Ds4Mapping.Hat(false, false, true, true) == "None" && Ds4Mapping.Hat(true, true, true, true) == "None", "opposites cancel");
Console.WriteLine($"PASS {checks} DualShock 4 mapping checks; no controller library or device access");
