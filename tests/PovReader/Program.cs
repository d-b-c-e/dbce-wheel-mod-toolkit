using Dbce.Wheel.Input;

int checks = 0, calls = 0;
void Check(bool condition) { checks++; if (!condition) throw new Exception("POV check " + checks); }
var axes = new int[8]; var buttons = new byte[128]; var hats = new int[4];
foreach (int outcome in new[] { 1, 0, -1, 2, -2 })
{
    var reader = new PovSnapshotReader((s, a, an, b, bn, h, hn) => {
        calls++; Check(s == 3 && an == 8 && bn == 128 && hn == 4);
        a[2] = 123; b[127] = 1; h[0] = 27000; h[1] = 65535;
        if (outcome == -2) throw new IOException("partial failed read");
        return outcome;
    });
    Check(reader.Read(3, axes, buttons, hats) == (outcome == 1));
    Check(outcome == 1 ? axes[2] == 123 && buttons[127] == 1 && hats[0] == 27000 && hats[1] == -1
        : axes.All(v => v == 0) && buttons.All(v => v == 0) && hats.All(v => v == -1));
}
Check(calls == 5);
var absent = PovSnapshotReader.FromExport(IntPtr.Zero);
Check(!absent.Available && !absent.Read(0, axes, buttons, hats));
Check(hats.All(v => v == -1));
var guarded = new PovSnapshotReader((s,a,an,b,bn,h,hn) => { calls++; return 1; });
Check(!guarded.Read(-1, axes, buttons, hats));
Check(!guarded.Read(0, new int[9], buttons, hats));
Check(!guarded.Read(0, axes, new byte[129], hats));
Check(!guarded.Read(0, axes, buttons, new int[3]));
Check(calls == 5);
Check(guarded.Read(0, axes, null, hats));
Check(calls == 6);
Console.WriteLine($"POV resident-module reader: {checks} checks passed; fake delegate only.");
