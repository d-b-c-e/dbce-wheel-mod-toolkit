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
foreach (string lineBreak in new[] { "\r", "\n", "\r\n" })
{
    Check(!ControlBinding.TryParse("button 1" + ids + " name=\"before" + lineBreak + "after\"", out _, out var why) && why == "bad-name", "Line break in quoted name refused");
    Check(!ControlBinding.TryParse("button 1" + lineBreak + ids, out _, out why) && why == "bad-name", "Line break between tokens refused");
}
foreach (double bad in new[] { double.NaN, double.PositiveInfinity, double.NegativeInfinity })
{
    bool refused = false;
    try { full.Denormalize("steer", bad); } catch (ArgumentOutOfRangeException) { refused = true; }
    Check(refused, "Nonfinite inverse refused");
}
foreach(int angle in new[]{0,4500,9000,13500,18000,22500,27000,31500})
{
    var hat=Parse("hat 0 "+angle.ToString(CultureInfo.InvariantCulture)+ids);
    for(int raw=0;raw<36000;raw++)
    {
        int forward=(raw-angle+36000)%36000;
        bool expected=forward<=4500 || forward>=31500;
        Check(ControlBinding.HatPressed(raw,angle)==expected && (hat.Normalize("confirm",raw)==1)==expected,"Circular POV boundary");
    }
}
foreach(int raw in new[]{-1,65535,0x1234ffff}) Check(!ControlBinding.HatPressed(raw,0),"Neutral hat");
Check(!ControlBinding.HatPressed(0,-1) && !ControlBinding.HatPressed(0,36000),"Invalid target angle");

// ShimSteering: the force wheel identity only from bytes Core accepted exactly; never a second grammar.
const string wheel = "00000000-0000-0000-0000-0000000000a1";
const string steerAxis = "axis 0 dev={" + wheel + "} prod={0006346e-0000-0000-0000-504944564944} range=0..65535 rest=32768 travel=+1";
byte[] Section(string body) => System.Text.Encoding.UTF8.GetBytes("[Controls]\r\nschema=1\r\n" + body);
string Report(byte[] bytes, string ok = "1") => "ok=" + ok + "\nprofile=Rig\ntransmission=Sequential\nsha256=" +
    Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes)).ToLowerInvariant() + "\n";
var accepted = Section("steer=" + steerAxis + "\r\nthrottle=axis 2 dev={" + wheel + "} prod={0006346e-0000-0000-0000-504944564944} range=0..65535 rest=0 travel=+1\r\n");
Check(ShimSteering.FromAccepted(accepted, Report(accepted), out var steeringWhy)?.Device == Guid.Parse(wheel) && steeringWhy == "",
    "Core-accepted exact bytes give the steer device");
var named = Section("steer=" + steerAxis + " name=\"MOZA R12 \\\"Base\\\"\"\r\n");
var steerBinding = ShimSteering.FromAccepted(named, Report(named), out _);
Check(steerBinding != null && steerBinding.Device == Guid.Parse(wheel) && steerBinding.Product == Guid.Parse("0006346e-0000-0000-0000-504944564944") &&
    steerBinding.Name == "MOZA R12 \"Base\"" && steerBinding.Kind == ControlKind.Axis && steerBinding.Index == 0,
    "The parsed steer binding: device, product and display name as ControlBinding reads them");
var noProduct = Section("steer=axis 0 dev={" + wheel + "} prod={00000000-0000-0000-0000-000000000000} range=0..65535 rest=32768 travel=+1\r\n");
var emptyProduct = ShimSteering.FromAccepted(noProduct, Report(noProduct), out var noProductWhy);
Check(emptyProduct != null && emptyProduct.Device == Guid.Parse(wheel) && emptyProduct.Product == Guid.Empty && noProductWhy == "",
    "An empty product GUID is returned as written; product policy belongs to the consumer");
foreach (var (name, report) in new[] {
    ("hash of other bytes", Report(Section("steer=" + steerAxis + "\r\n"))), ("not accepted", Report(accepted, "0")),
    ("no hash", "ok=1\n"), ("repeated field", Report(accepted) + "ok=1\n") })
{
    bool refused = false;
    try { ShimSteering.FromAccepted(accepted, report, out _); } catch (InvalidDataException) { refused = true; }
    Check(refused, "Core acceptance required: " + name);
}
foreach (var (name, body) in new[] {
    ("no steer", "throttle=" + steerAxis + "\r\n"), ("two steer lines", "steer=" + steerAxis + "\r\nsteer=" + steerAxis + "\r\n"),
    ("steer on a button", "steer=button 1 dev={" + wheel + "} prod={0006346e-0000-0000-0000-504944564944}\r\n"),
    ("unparsable steer", "steer=axis nine\r\n"), ("empty steer", "steer=\r\n") })
{
    var bytes = Section(body);
    Check(ShimSteering.FromAccepted(bytes, Report(bytes), out var why) == null && why == ShimSteering.NoSteering, "No steering identity: " + name);
}
var commented = Section("; steer=button 1 dev={" + wheel + "} prod={0006346e-0000-0000-0000-504944564944}\r\nsteer=" + steerAxis + "\r\n");
Check(ShimSteering.FromAccepted(commented, Report(commented), out _)?.Device == Guid.Parse(wheel), "Comment lines are not steer lines");
var notUtf8 = new byte[] { 0x5b, 0x43, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x73, 0x5d, 0x0a, 0xff, 0xfe, 0x0a };
Check(ShimSteering.FromAccepted(notUtf8, Report(notUtf8), out var utf8Why) == null && utf8Why == ShimSteering.NoSteering,
    "Accepted bytes that are not UTF-8 give no identity, never an exception");
Console.WriteLine($"PASS {vectors} shared controls vectors; {checks} managed checks (no device or game).");
