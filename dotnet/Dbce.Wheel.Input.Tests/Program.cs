using System.Globalization;
using Dbce.Wheel.Input;

int checks = 0;
void Check(bool ok, string description) { checks++; if (!ok) throw new Exception(description); }
double D(string n) => double.Parse(n, CultureInfo.InvariantCulture);
int I(string n) => int.Parse(n, CultureInfo.InvariantCulture);
ControlBinding Parse(string text)
{
    if (!ControlBinding.TryParse(text, out var b, out var why)) throw new Exception(why + ": " + text);
    return b!;
}
int vectors = 0;
CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("fr-FR");
foreach (string line in File.ReadLines(Path.Combine(AppContext.BaseDirectory, "controls-vectors.txt")))
{
    if (string.IsNullOrWhiteSpace(line) || line.StartsWith('#')) continue;
    vectors++;
    string[] p = line.Split('|').Select(s => s.Trim()).ToArray();
    string description = "Vector " + vectors + ": " + line;
    switch (p[0])
    {
        case "parse": Check(Parse(p[1]).ToString() == p[2], description); break;
        case "reject": Check(!ControlBinding.TryParse(p[1], out var b, out var why) && b == null && why == p[2], description); break;
        case "fits": Check(Parse(p[2]).Fits(p[1]) == (p[3] == "ok" ? "" : p[3]), description); break;
        case "norm": Check(Math.Abs(Parse(p[2]).Normalize(p[1], I(p[3])) - D(p[4])) <= 1e-6, description); break;
        case "denorm": Check(Parse(p[2]).Denormalize(p[1], D(p[3])) == I(p[4]), description); break;
        case "rescale": Check(ControlBinding.Rescale(I(p[1]), I(p[2]), I(p[3]), I(p[4]), I(p[5])) == I(p[6]), description); break;
        case "dibutton": Check(ControlBinding.DirectInputButtonDown(I(p[1])) == (p[2] == "1"), description); break;
        default: throw new Exception("Unknown vector type: " + line);
    }
}
Check(vectors >= 60, "Shared vectors were not loaded");
const string ids = " dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
var full = Parse("axis 0" + ids + " range=-2147483648..2147483647 rest=-2147483648 travel=+1");
Check(full.Normalize("steer", int.MaxValue) == 1 && full.Normalize("throttle", int.MaxValue) == 1, "Int32 endpoints");
Check(full.Denormalize("steer", 0) == -1 && full.Denormalize("throttle", 1) == int.MaxValue, "Wide inverse range");
Check(!ControlBinding.TryParse("axis 0" + ids + " range=0..2147483648 rest=0 travel=+1", out _, out var reason) && reason == "bad-range", "Overflow refused");
Check(!ControlBinding.TryParse("button 1" + ids + " name=\"injected\nsteer=\"", out _, out _), "Multiline refused");
foreach (double bad in new[] { double.NaN, double.PositiveInfinity, double.NegativeInfinity })
{
    bool refused = false;
    try { full.Denormalize("steer", bad); } catch (ArgumentOutOfRangeException) { refused = true; }
    Check(refused, "Nonfinite inverse refused");
}
Console.WriteLine($"PASS {vectors} shared controls vectors; {checks} managed checks (no device or game).");
