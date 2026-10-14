using System.Globalization;
using Dbce.Wheel.Input;

int checks = 0;
void Check(bool ok, string description) { checks++; if (!ok) throw new Exception(description); }
double D(string n) => double.Parse(n, CultureInfo.InvariantCulture);
int I(string n) => int.Parse(n, CultureInfo.InvariantCulture);
ControlBinding Parse(string text, int schema = 1)
{
    if (!ControlBinding.TryParse(text, out var b, out var why, schema)) throw new Exception(why + ": " + text);
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
    // "parse2", "norm2", ...: the same operation reading the binding as schema 2.
    int schema = p[0].Length > 1 && p[0].EndsWith("2", StringComparison.Ordinal) ? 2 : 1;
    string op = schema == 2 ? p[0].Substring(0, p[0].Length - 1) : p[0];
    switch (op)
    {
        case "parse": Check(Parse(p[1], schema).ToString() == p[2] && Parse(Parse(p[1], schema).ToString(), schema).ToString() == p[2], description); break;
        case "reject": Check(!ControlBinding.TryParse(p[1], out var b, out var why, schema) && b == null && why == p[2], description); break;
        case "fits":
            Check(Parse(p[2], schema).Fits(p[1]) == (p[3] == "ok" ? "" : p[3]), description);
            Check(Parse(p[2], schema).Fits(ControlBinding.ShapeOf(p[1])) == (p[3] == "ok" ? "" : p[3]), description + " by shape"); break;
        case "norm":
            Check(Math.Abs(Parse(p[2], schema).Normalize(p[1], I(p[3])) - D(p[4])) <= 1e-6, description);
            Check(Math.Abs(Parse(p[2], schema).Normalize(ControlBinding.ShapeOf(p[1]), I(p[3])) - D(p[4])) <= 1e-6, description + " by shape"); break;
        case "denorm":
            Check(Parse(p[2], schema).Denormalize(p[1], D(p[3])) == I(p[4]), description);
            Check(Parse(p[2], schema).Denormalize(ControlBinding.ShapeOf(p[1]), D(p[3])) == I(p[4]), description + " by shape"); break;
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
foreach (var shape in new[] { ControlShape.Unknown, (ControlShape)99, ControlShape.Digital })
{
    bool refusedForward = false, refusedInverse = false;
    try { full.Normalize(shape, 0); } catch (ArgumentException e) { refusedForward = e.ParamName == "shape"; }
    try { full.Denormalize(shape, 0); } catch (ArgumentException e) { refusedInverse = e.ParamName == "shape"; }
    Check(refusedForward && refusedInverse, "Unusable declared shape refuses both directions: " + shape);
}
foreach (var action in new[] { "unknown", "confirm" })
{
    bool refused = false;
    try { full.Normalize(action, 0); } catch (ArgumentException e) { refused = e.ParamName == "action"; }
    Check(refused, "Existing action exception contract retained");
}
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
    refused = false;
    try { full.Denormalize(ControlShape.Centred, bad); } catch (ArgumentOutOfRangeException e) { refused = e.ParamName == "n"; }
    Check(refused, "Nonfinite shape inverse refused");
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

// Schema 2 `cal`: the inverse is exact inside the measured endpoints; beyond them the value saturates at the endpoint.
var calSteer = Parse("axis 0" + ids + " range=0..65535 rest=33265 travel=+1 cal=1000..33265..64000", 2);
var calPedal = Parse("axis 2" + ids + " range=0..65535 rest=1200 travel=+1 cal=1200..60000 inverted", 2);
var calReversed = Parse("axis 5" + ids + " range=-32768..32767 rest=30000 travel=-1 cal=30000..-31000", 2);
bool exact = true;
for (int v = 1000; v <= 64000; v++) exact &= calSteer.Denormalize("steer", calSteer.Normalize("steer", v)) == v;
for (int v = 1200; v <= 60000; v++) exact &= calPedal.Denormalize("throttle", calPedal.Normalize("throttle", v)) == v;
for (int v = -31000; v <= 30000; v++) exact &= calReversed.Denormalize("brake", calReversed.Normalize("brake", v)) == v;
Check(exact, "Calibrated inverse exact inside the measured endpoints");
Check(calSteer.Normalize("steer", 999) == -1 && calSteer.Normalize("steer", 0) == -1 && calSteer.Denormalize("steer", -1) == 1000 &&
      calSteer.Normalize("steer", 64001) == 1 && calSteer.Denormalize("steer", 1) == 64000 && calPedal.Normalize("throttle", 65535) == 0,
      "Calibrated saturation beyond the measured endpoints");
Check(Parse("axis 0" + ids + " range=0..65535 rest=33265 travel=+1 cal=1000..33265..64000").Cal == null, "Schema 1 ignores cal");
foreach (var (action, text) in new[] { ("steer", "axis 0" + ids + " range=0..65535 rest=33265 travel=+1 cal=1200..60000"),
                                       ("throttle", "axis 2" + ids + " range=0..65535 rest=0 travel=+1 cal=1000..33265..64000") })
{
    var unfitted = Parse(text, 2);
    bool threw = false, threwBack = false;
    try { unfitted.Normalize(action, 30000); } catch (ArgumentException) { threw = true; }
    try { unfitted.Denormalize(action, 0.5); } catch (ArgumentException) { threwBack = true; }
    Check(unfitted.Fits(action) == "bad-calibration" && threw && threwBack, "Unfitted cal refused explicitly: " + action);
}
Check(!ControlBinding.HatPressed(0,-1) && !ControlBinding.HatPressed(0,36000),"Invalid target angle");

// ShimSteering: the force wheel identity only from bytes Core accepted exactly; never a second grammar.
const string wheel = "00000000-0000-0000-0000-0000000000a1";
const string steerAxis = "axis 0 dev={" + wheel + "} prod={0006346e-0000-0000-0000-504944564944} range=0..65535 rest=32768 travel=+1";
byte[] Section(string body) => System.Text.Encoding.UTF8.GetBytes("[Controls]\r\nSchema = 1\r\n" + body);
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
// The schema is the helper's own check: Core at schema 1 reports ok=1 for a Schema 2 section it refuses (native
// test_shim.cpp asserts it), which is exactly Report() below. By default (reads 1) that gives no force target.
const string calSteerAxis = steerAxis + " cal=1000..32768..64000";
byte[] SchemaSection(string schema, string body) => System.Text.Encoding.UTF8.GetBytes("[Controls]\r\nSchema = " + schema + "\r\n" + body);
var schema2 = SchemaSection("2", "steer=" + calSteerAxis + "\r\n");
// Core's exact report for that section (dbce_shim.cpp: ok, error = the structural error only, profile, revision,
// transmission, sha256): a schema refusal is not in it.
string coreRefusal = "ok=1\nerror=\nprofile=\nrevision=\ntransmission=\nsha256=" +
    Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(schema2)).ToLowerInvariant() + "\n";
Check(ShimSteering.FromAccepted(schema2, coreRefusal, out var refusedWhy) == null && refusedWhy == ShimSteering.UnsupportedSchema,
      "Schema 2 with the default reads 1 (Core's real ok=1 report for the section it refused): no target");
var calTarget = ShimSteering.FromAccepted(schema2, Report(schema2), out var calWhy, reads: 2);
Check(calTarget != null && calTarget.Device == Guid.Parse(wheel) && calTarget.Cal != null && calTarget.Cal.Count == 3 && calTarget.Cal[1] == 32768 &&
      Math.Abs(calTarget.Normalize("steer", 16884) + 0.5) < 1e-12 && calWhy == "", "Schema 2 with reads 2: the force target and its cal");
var schema1 = SchemaSection("1", "steer=" + calSteerAxis + "\r\n");
foreach (int reads in new[] { 1, 2 })
{
    var plainTarget = ShimSteering.FromAccepted(schema1, Report(schema1), out var plainWhy, reads);
    Check(plainTarget != null && plainTarget.Device == Guid.Parse(wheel) && plainTarget.Cal == null && plainWhy == "",
          "Schema 1 (reads " + reads + "): the force target, cal ignored");
}
var badCal = SchemaSection("2", "steer=" + steerAxis + " cal=0..300..65535\r\n");
Check(ShimSteering.FromAccepted(badCal, Report(badCal), out var badWhy, reads: 2) == null && badWhy == ShimSteering.NoSteering,
      "Schema 2 with an invalid calibration: no identity (Core reports it invalid too)");
foreach (var (name, head, reads) in new[] {
    ("no Schema line", "[Controls]\r\n", 2), ("lowercase key", "[Controls]\r\nschema=1\r\n", 2), ("malformed", "[Controls]\r\nSchema = two\r\n", 2),
    ("signed", "[Controls]\r\nSchema = +1\r\n", 2), ("zero", "[Controls]\r\nSchema = 0\r\n", 2), ("above reads 2", "[Controls]\r\nSchema = 3\r\n", 2),
    ("last line wins: 2 at reads 1", "[Controls]\r\nSchema = 1\r\nSchema = 2\r\n", 1) })
{
    var bytes = System.Text.Encoding.UTF8.GetBytes(head + "steer=" + steerAxis + "\r\n");
    Check(ShimSteering.FromAccepted(bytes, Report(bytes), out var why, reads) == null && why == ShimSteering.UnsupportedSchema, "Unsupported schema: " + name);
}
var lastWins = SchemaSection("2", "Schema = 1\r\nsteer=" + steerAxis + "\r\n");
Check(ShimSteering.FromAccepted(lastWins, Report(lastWins), out _)?.Device == Guid.Parse(wheel), "The last Schema line wins, as parseSection (Core itself refuses a repeated key)");
var notUtf8 = new byte[] { 0x5b, 0x43, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x73, 0x5d, 0x0a, 0xff, 0xfe, 0x0a };
Check(ShimSteering.FromAccepted(notUtf8, Report(notUtf8), out var utf8Why) == null && utf8Why == ShimSteering.NoSteering,
    "Accepted bytes that are not UTF-8 give no identity, never an exception");
Console.WriteLine($"PASS {vectors} shared controls vectors; {checks} managed checks (no device or game).");
