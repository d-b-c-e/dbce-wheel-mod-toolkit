// dbce-mod.json (XC-1): the C# reader's dump of each fixture equals schema/fixtures/<id>.dump.txt, the same file the
// PowerShell 5.1 and C++ readers are checked against, so all three agree on every value. Run:
//   dotnet run --project dotnet/Dbce.Wheel.ModFile.Tests
using System.Globalization;
using Dbce.Wheel.Mod;

CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("fr-FR");
string dir = Path.Combine(AppContext.BaseDirectory, "fixtures");
int checks = 0;
void Check(bool ok, string what) { checks++; if (!ok) throw new Exception("FAIL " + what); }
static string[] Lines(string text) => text.Replace("\r\n", "\n").TrimEnd('\n').Split('\n');

var files = Directory.GetFiles(dir, "*.dbce-mod.json").OrderBy(f => f, StringComparer.Ordinal).ToArray();
Check(files.Length == 3, "three fixtures");
foreach (string file in files)
{
    var mod = DbceModFile.Load(file);
    string[] got = Lines(mod.Dump()), want = Lines(File.ReadAllText(file.Replace(".dbce-mod.json", ".dump.txt")));
    for (int i = 0; i < Math.Min(got.Length, want.Length); i++) Check(got[i] == want[i], $"{Path.GetFileName(file)} line {i + 1}: got '{got[i]}', want '{want[i]}'");
    Check(got.Length == want.Length, $"{Path.GetFileName(file)}: {got.Length} lines, want {want.Length}");
}

// Spot checks on the typed model, beyond the dump.
var woden = DbceModFile.Load(Path.Combine(dir, "super-woden-rally-edge.dbce-mod.json"));
var eye = woden.Settings!.Rows.Single(r => r.Role == "screens.eyeDistanceMm");
Check(eye.Store!.Section == "Triple" && eye.Store.Key == "EyeDistanceMm" && eye.Default == 660, "Woden eye distance row");
Check(woden.Package.Loader!.Version == "6.0.0-be.788" && woden.Installed!.Files.Count == 7, "Woden loader and installed files");
var dirt = DbceModFile.Load(Path.Combine(dir, "dirt-4.dbce-mod.json"));
Check(dirt.Package.Loader == null && dirt.Controls!.Store == "game" && dirt.Cameras.Count == 0, "DiRT 4 has no loader, game controls, no camera starts");
var art = DbceModFile.Load(Path.Combine(dir, "art-of-rally.dbce-mod.json"));
Check(art.Settings!.Rows.Single(r => r.Id == "display.eye-distance").Store!.Scale == 10, "art eye distance is stored in mm");

bool Refused(string json) { try { DbceModFile.Parse(json); return false; } catch (FormatException) { return true; } }
Check(Refused("{\"schema\":\"dbce.mod@2\",\"id\":\"x\",\"name\":\"X\",\"version\":\"1.0.0\",\"package\":{\"kind\":\"standalone\"}}"), "refuses another schema");
Check(Refused("{\"schema\":\"dbce.mod@1\",\"id\":\"x\",\"name\":\"X\",\"version\":\"1.0.0\"}"), "refuses a file without a package");
Check(Refused("{\"schema\":\"dbce.mod@1\",\"schema\":\"dbce.mod@1\"}"), "refuses duplicate members");
// The shared store (docs/STORE.md): each fixture's dump equals its golden, and each X.future.json (the same file with
// fields a later version adds) dumps exactly like X.json: readers ignore fields they do not know.
// tools\tests\Test-DbceStore.ps1 -Update rewrites the goldens.
string storeDir = Path.Combine(dir, "store");
string StoreDump(string file) => file.EndsWith(".runtime.json") || file.EndsWith(".runtime.future.json")
    ? DbceRuntimeSettings.Load(file).Dump() : DbceStore.Load(file).Dump();
void Same(string file, string golden)
{
    string[] got = Lines(StoreDump(file)), want = Lines(File.ReadAllText(golden));
    for (int i = 0; i < Math.Min(got.Length, want.Length); i++) Check(got[i] == want[i], $"{Path.GetFileName(file)} line {i + 1}: got '{got[i]}', want '{want[i]}'");
    Check(got.Length == want.Length, $"{Path.GetFileName(file)}: {got.Length} lines, want {want.Length}");
}
var storeFiles = Directory.GetFiles(storeDir, "*.json").Where(f => !f.EndsWith(".future.json")).OrderBy(f => f, StringComparer.Ordinal).ToArray();
Check(storeFiles.Length == 3, "three store fixtures");
foreach (string file in storeFiles)
{
    string golden = file.Replace(".json", ".dump.txt");
    Same(file, golden);
    string future = file.Replace(".json", ".future.json");
    if (File.Exists(future)) Same(future, golden);
}
Check(Directory.GetFiles(storeDir, "*.future.json").Length == 2, "two future fixtures");
var owner = DbceStore.Load(Path.Combine(storeDir, "owner.profiles.json"));
Check(owner.Profiles.Select(p => p.Name).SequenceEqual(new[] { "Moza KS Pro Triple Manual", "Moza KS Pro Triple Auto" }), "the owner's two profiles");
var artChoice = owner.For("art-of-rally");
Check(artChoice.Profile.Transmission == "Automatic" && artChoice.Profile.FfbStrength == 50 && artChoice.Screens == owner.Cockpit && owner.Cockpit!.Count == 3, "art uses Auto with the cockpit");
Check(owner.For("dirt-4").Profile.Transmission == "H-pattern", "DiRT 4 uses Manual");
var wide = DbceStore.Load(Path.Combine(storeDir, "widescreen.profiles.json"));
Check(wide.Cockpit == null && wide.For("anything").ScreensFrom == "profile" && wide.For("slow-roads").ScreensFrom == "none", "a profile's own screens override; no cockpit is none");
Check(wide.For("dirt-4").Profile.Id == "track", "an assignment to a deleted profile falls back to ActiveId");
var rt = DbceRuntimeSettings.Load(Path.Combine(storeDir, "art-of-rally.runtime.json"));
Check(rt.Get("ffb.strength")!.Number == 55 && rt.Get("camera.start")!.Text == "cockpit" && rt.Get("missing") == null, "runtime values");
Check(DbceStore.RuntimePath("art-of-rally").EndsWith(Path.Combine("Wheelkit", "store", "runtime", "art-of-rally.json")), "runtime path");
bool StoreRefused(string json) { try { DbceStore.Parse(json); return false; } catch (FormatException) { return true; } }
Check(StoreRefused("{\"Schema\":3,\"Profiles\":[{\"Id\":\"a\",\"Profile\":{}}]}"), "refuses another profiles schema");
Check(StoreRefused("{\"Schema\":2,\"Profiles\":[]}"), "refuses no profiles");
Check(StoreRefused("{\"Schema\":2,\"Profiles\":[{\"Id\":\"a\",\"Profile\":{}},{\"Id\":\"a\",\"Profile\":{}}]}"), "refuses duplicate profile ids");
// The shim's [Controls] text from the store (DbceStoreControls): DRIVE's assigned profile gives, line for line, the
// dbce-controls.ini Wheelkit's SharedControlsWriter wrote for the same profile (its golden, copied to
// schema/fixtures/controls), except the Revision, which is now the store's own.
string controlsDir = Path.Combine(dir, "controls");
var driveStore = DbceStore.Load(Path.Combine(controlsDir, "drive-rally.profiles.json"));
string[] driveDelivers = { "steer", "throttle", "brake", "handbrake", "shiftUp", "shiftDown", "reverse", "start", "camera", "reset",
    "gear1", "gear2", "gear3", "gear4", "gear5", "gear6", "gear7", "gear8" };
var driveProfile = driveStore.For("drive-rally").Profile;
string? driveText = DbceStoreControls.Text(driveProfile, driveDelivers, out string driveWhy);
Check(driveText != null && driveWhy == "", "DRIVE's profile gives controls: " + driveWhy);
string[] gotIni = Lines(driveText!), wantIni = Lines(File.ReadAllText(Path.Combine(controlsDir, "drive-rally.controls.ini")));
Check(gotIni.Length == wantIni.Length, $"DRIVE controls: {gotIni.Length} lines, want {wantIni.Length}");
for (int i = 0; i < wantIni.Length; i++)
    Check(wantIni[i].StartsWith("Revision = ") ? gotIni[i].StartsWith("Revision = ") && gotIni[i].Length == wantIni[i].Length : gotIni[i] == wantIni[i],
        $"DRIVE controls line {i + 1}: got '{gotIni[i]}', want '{wantIni[i]}'");
Check(DbceStoreControls.Text(driveProfile, driveDelivers, out _) == driveText, "the same profile gives the same revision");
// The exact bytes, Revision included; native/panel/test_store.cpp checks the C++ twin against the same file.
Check(driveText == File.ReadAllText(Path.Combine(controlsDir, "drive-rally.store-controls.txt")), "DRIVE controls equal drive-rally.store-controls.txt byte for byte");
string Transmission(string t) { driveProfile.Transmission = t; return DbceStoreControls.Text(driveProfile, driveDelivers, out _)!; }
string hp = Transmission("H-pattern"), auto = Transmission("Automatic");
driveProfile.Transmission = "Sequential";
Check(hp.Contains("reverse = button 7 ") && hp.Contains("gear7 = ") && !hp.Contains("shiftUp"), "H-pattern writes the gates, not the shifts");
Check(!auto.Contains("shift") && !auto.Contains("gear") && !auto.Contains("reverse") && auto.Contains("Transmission = Automatic"), "Automatic writes no gears");
driveProfile.Bindings.Single(b => b.Action == "shiftDown").Index = 9;
Check(!DbceStoreControls.Text(driveProfile, driveDelivers, out string clashWhy)!.Contains("shift") && clashWhy.Contains("gears"), "two gears on one control write neither");
Check(DbceStoreControls.Text(driveStore.For("unassigned").Profile, driveDelivers, out string noSteer) == null && noSteer.Contains("no steering"), "no steering, no controls");
Check(DbceStoreControls.Text(driveProfile, new[] { "throttle" }, out _) == null, "a build that does not take steering gets no controls");
Console.WriteLine($"PASS {checks} dbce-mod.json and store reader checks");
