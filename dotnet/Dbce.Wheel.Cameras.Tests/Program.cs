// Checks for shared/cameras/CameraSets.cs, the C# twin of native/cameras/camera_sets.hpp.
// PORTED LITERALLY from native/cameras/test.cpp: the same checks, in the same order, with the same inputs and expected
// values (C++ CHECK n = "C++ check n" below). If you change a case, change it in test.cpp too. The last block holds
// extra checks that only the C# side needs (its own number reading and %.17g formatting).
// Run: dotnet run --project dotnet/Dbce.Wheel.Cameras.Tests
using Dbce.Wheel.Cameras;
using static Dbce.Wheel.Cameras.View;

int fails = 0, checks = 0;
void Check(bool c, string what) { checks++; if (!c) { Console.WriteLine("FAIL " + what); fails++; } }

Settings Read(Dictionary<string, string> ini) =>
    CameraSets.ReadSettings((string k, out string v) => ini.TryGetValue(k, out v!));
int I(View v) => (int)v;

Settings s = Read(new());
Check(!s.Realistic[I(Chase)] && !s.Realistic[I(Bonnet)] && s.Realistic[I(Bumper)] && s.Realistic[I(Dash)] && s.Realistic[I(Cockpit)], "C++ check 1: starting choices");
s = Read(new() { { "MatchGameFov", "true" } });
Check(!s.Realistic[I(Bumper)] && !s.Realistic[I(Cockpit)] && !s.Realistic[I(Chase)], "C++ check 2: MatchGameFov true");
s = Read(new() { { "MatchGameFov", "false" }, { "BonnetRealistic", "off" }, { "CockpitGameOffsetDegrees", "-4 ; note" }, { "BumperRealisticOffsetDegrees", "x" } });
Check(s.Realistic[I(Cockpit)] && !s.Realistic[I(Bonnet)] && s.Offset[I(Cockpit)][0] == -4 && s.Offset[I(Bumper)][1] == 0, "C++ check 3: saved keys");
Write w = CameraSets.Toggle(s, Cockpit);
Check(w.Key == "CockpitRealistic" && w.Value == "false" && !s.Realistic[I(Cockpit)], "C++ check 4: toggle");
w = CameraSets.ZoomView(s, Cockpit, Zoom.In);
Check(w.Key == "CockpitGameOffsetDegrees" && s.Offset[I(Cockpit)][0] == -6, "C++ check 5: zoom in");
w = CameraSets.ZoomView(s, Cockpit, Zoom.Reset);
Check(w.Value == "0" && s.Offset[I(Cockpit)][0] == 0, "C++ check 6: zoom reset");
float[] game = { 0.9f, 0.4f }, seat = { 0.6f, 0.2f };
float[] o = CameraSets.Target(s, None, game, seat);
Check(o[0] == game[0] && o[1] == game[1], "C++ check 7: target for no view");
o = CameraSets.Target(s, Bumper, game, seat);
Check(o[0] == seat[0] && o[1] == seat[1], "C++ check 8: target for bumper uses the seat lens");
s.Offset[I(Bonnet)][0] = 10;
o = CameraSets.Target(s, Bonnet, game, seat);
double deg = 2 * Math.Atan((double)o[1]) * 57.29577951308232, bas = 2 * Math.Atan(0.4) * 57.29577951308232;
Check(Math.Abs(deg - bas - 10) < 1e-3 && Math.Abs(o[0] / o[1] - 0.9 / 0.4) < 1e-4, "C++ check 9: target with a 10 degree offset");
string b = CameraSets.Label(s, None, "DiRT 4");
Check(b == "This camera keeps DiRT 4's own FOV (not identified yet)", "C++ check 10: label for no view");
b = CameraSets.Name(s, Bonnet);
Check(b == "Bonnet Camera - Game FOV (Custom)", "C++ check 11: name with a custom offset");

// ---- C# only: the pieces the C++ gets from the C library ----
// Expected strings are what MSVC printf("%.17g") gives.
Check(CameraSets.FormatG17(-6) == "-6" && CameraSets.FormatG17(100) == "100" && CameraSets.FormatG17(0) == "0", "FormatG17 whole numbers");
Check(CameraSets.FormatG17(2.1) == "2.1000000000000001" && CameraSets.FormatG17(0.1) == "0.10000000000000001", "FormatG17 fractions");
Check(CameraSets.FormatG17(1e20) == "1e+20" && CameraSets.FormatG17(1e-5) == "1.0000000000000001e-05", "FormatG17 exponents");
Check(CameraSets.FormatG17(double.MaxValue) == "1.7976931348623157e+308" && CameraSets.FormatG17(double.Epsilon) == "4.9406564584124654e-324", "FormatG17 extremes");
Check(CameraSets.FormatG17(123456789012345678.0) == "1.2345678901234568e+17" && CameraSets.FormatG17(-0.5) == "-0.5", "FormatG17 rounding and sign");
s = Read(new() { { "ChaseGameOffsetDegrees", "1e1" }, { "BonnetGameOffsetDegrees", " +2.5" }, { "DashGameOffsetDegrees", "3 # c" },
                 { "CockpitGameOffsetDegrees", "inf" }, { "BumperGameOffsetDegrees", "1e999" }, { "ChaseRealisticOffsetDegrees", "1e-999" },
                 { "BonnetRealisticOffsetDegrees", "5x" }, { "DashRealisticOffsetDegrees", "0x10" }, { "CockpitRealisticOffsetDegrees", "" } });
Check(s.Offset[I(Chase)][0] == 10 && s.Offset[I(Bonnet)][0] == 2.5 && s.Offset[I(Dash)][0] == 3, "offsets: exponent, sign, comment");
Check(s.Offset[I(Cockpit)][0] == 0 && s.Offset[I(Bumper)][0] == 0 && s.Offset[I(Chase)][1] == 0 && s.Offset[I(Bonnet)][1] == 0, "offsets: inf, overflow, underflow, junk refused");
Check(s.Offset[I(Dash)][1] == 16 && s.Offset[I(Cockpit)][1] == 0, "offsets: hex float read, empty refused");
// A long value is cut at 63 characters like the C++ buffer: refused unless a comment follows the cut.
Check(Read(new() { { "ChaseGameOffsetDegrees", "1" + new string('0', 70) } }).Offset[I(Chase)][0] == 0, "offsets: truncated value refused");
Check(Read(new() { { "ChaseGameOffsetDegrees", "7" + new string(' ', 70) + ";" } }).Offset[I(Chase)][0] == 0, "offsets: cut inside blanks refused");
// Zoom steps: out adds 2, a huge value steps to the next double instead of getting stuck.
s = Read(new() { { "ChaseGameOffsetDegrees", "0.1" } });
w = CameraSets.ZoomView(s, Chase, Zoom.Out);
Check(w.Key == "ChaseGameOffsetDegrees" && w.Value == "2.1000000000000001", "zoom out value text");
s.Offset[I(Chase)][0] = 1e300;
w = CameraSets.ZoomView(s, Chase, Zoom.Out);
Check(s.Offset[I(Chase)][0] > 1e300 && !w.IsEmpty, "zoom out at a huge value moves to the next double");
s.Offset[I(Chase)][0] = double.MaxValue;
w = CameraSets.ZoomView(s, Chase, Zoom.Out);
Check(w.IsEmpty && s.Offset[I(Chase)][0] == double.MaxValue, "zoom out at the edge refuses");
// Watch: a view counts after 5 steady frames.
var watch = new Watch();
bool changed = false;
for (int i = 0; i < 4; i++) changed |= watch.Step(Cockpit);
Check(!changed && watch.Shown == None, "watch: not yet after 4 frames");
Check(watch.Step(Cockpit) && watch.Shown == Cockpit, "watch: shown on frame 5");
Check(!watch.Step(Cockpit) && !watch.Step(Chase) && watch.Shown == Cockpit, "watch: a short cut does not flicker");
// Label with the extra text and the key reminder.
s = Read(new());
Check(CameraSets.Label(s, Cockpit, "G", "Extra") == "Cockpit Camera - Realistic FOV  ·  Extra  ·  Numpad + / - zoom, 5 reset  ·  F7 Game FOV", "label with extra");

Console.WriteLine(fails == 0 ? $"PASS camera_sets ({checks} checks)" : $"FAIL {fails} of {checks}");
return fails == 0 ? 0 : 1;
