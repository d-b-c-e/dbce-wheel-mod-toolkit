using Dbce.Wheel.Display;

// Racing-row policy checks: pure geometry, no Windows call except the read-only live print at the end.
int checks = 0;
void Check(bool good, string reason) { checks++; if (!good) throw new Exception("FAIL: " + reason); }
MonitorRect M(int x, int y, int w, int h, bool primary = false) => new(x, y, w, h, primary);
MonitorRect Left = M(-2560, 0, 2560, 1440), Center = M(0, 0, 2560, 1440, true), Right = M(2560, 0, 2560, 1440);
MonitorRect Diagnostic = M(314, -1080, 1920, 1080);
SpanTargetChecks.Run(Check);

void Separate(string label, IEnumerable<MonitorRect> monitors)
{
    var row = RacingRowPolicy.Select(monitors);
    Check(row.Kind == RacingRowKind.Separate && row.X == -2560 && row.Y == 0 && row.Width == 7680 && row.Height == 1440, label + ": " + row);
    Check(row.Displays.Count == 3 && row.Displays[0].Equals(Left) && row.Displays[1].Equals(Center) && row.Displays[2].Equals(Right), label + ": left, primary, right");
}
void Refused(string label, IEnumerable<MonitorRect> monitors, string because)
{
    var row = RacingRowPolicy.Select(monitors);
    Check(row.Kind == RacingRowKind.None && row.Width == 0 && row.Displays.Count == 0 && row.Reason.Contains(because, StringComparison.Ordinal),
        label + ": expected no row (" + because + "), got " + row + " / " + row.Reason);
}

// The owner's rig on 2026-10-13: the racing row plus the 1920x1080 diagnostic display above it.
Separate("actual four-display layout", new[] { Left, Center, Right, Diagnostic });
Separate("enumeration order does not matter", new[] { Diagnostic, Right, Center, Left });
Separate("the unchanged three-display row", new[] { Left, Center, Right });
Separate("a display below the row is ignored too", new[] { Left, Center, Right, M(0, 1440, 1920, 1080) });
Separate("a display beyond the row's corner, outside its band", new[] { Left, Center, Right, M(5120, -1080, 1920, 1080) });

// Surround: the primary alone, whatever else is attached.
var surround = RacingRowPolicy.Select(new[] { M(0, 0, 7680, 1440, true) });
Check(surround.Kind == RacingRowKind.Surround && surround.X == 0 && surround.Width == 7680 && surround.Height == 1440, "Surround primary: " + surround);
var surroundPlus = RacingRowPolicy.Select(new[] { M(0, 0, 7680, 1440, true), M(2880, -1080, 1920, 1080) });
Check(surroundPlus.Kind == RacingRowKind.Surround && surroundPlus.Width == 7680, "Surround with a diagnostic display: " + surroundPlus);
var surroundSide = RacingRowPolicy.Select(new[] { M(0, 0, 7680, 1440, true), M(7680, 0, 2560, 1440) });
Check(surroundSide.Kind == RacingRowKind.Surround && surroundSide.Width == 7680, "Surround stays distinct: a side display is never added to it");
Check(RacingRowPolicy.Select(new[] { M(0, 0, 5120, 1440, true) }).Kind == RacingRowKind.Surround, "5120x1440 (3.6:1) is past the 2.9:1 Surround rule");

// No row, never a guess.
Refused("single display", new[] { Center }, "single display row");
Refused("single display plus the diagnostic one", new[] { Center, Diagnostic }, "single display row");
Refused("primary at the left end", new[] { M(0, 0, 2560, 1440, true), M(2560, 0, 2560, 1440), M(5120, 0, 2560, 1440) }, "the left");
Refused("missing right", new[] { Left, Center }, "the right");
Refused("gap on the left", new[] { M(-2570, 0, 2560, 1440), Center, Right }, "the left");
Refused("vertical offset on the right", new[] { Left, Center, M(2560, 10, 2560, 1440) }, "the right");
Refused("different size on the left", new[] { M(-1920, 0, 1920, 1080), Center, Right }, "the left");
Refused("left overlapping the primary", new[] { M(-2500, 0, 2560, 1440), Center, Right }, "the left");
Refused("four panels in one row", new[] { M(-5120, 0, 2560, 1440), Left, Center, Right }, "shares the racing row's band");
Refused("a display overlapping the row", new[] { Left, Center, Right, M(1000, 500, 1920, 1080) }, "shares the racing row's band");
Refused("a mirrored copy of the primary", new[] { Left, Center, Right, M(0, 0, 2560, 1440) }, "shares the racing row's band");
Refused("two candidates for the left", new[] { Left, Left, Center, Right }, "two displays claim the same side");
Refused("no primary", new[] { Left, M(0, 0, 2560, 1440), Right }, "no primary display");
Refused("two primaries", new[] { M(-2560, 0, 2560, 1440, true), Center, Right }, "more than one primary");
Refused("an empty rectangle", new[] { Left, Center, Right, M(0, -10, 0, 0) }, "empty rectangle");
Refused("nothing reported", Array.Empty<MonitorRect>(), "no primary display");

// Read-only: what this machine reports now. Informational; the topology differs between machines.
var live = WindowsMonitors.Read();
Check(live.Count >= 1 && live.Count(m => m.Primary) == 1, "Windows reports at least one display and exactly one primary");
Console.WriteLine("live displays: " + string.Join("; ", live) + " -> " + RacingRowPolicy.Select(live) + " (" + RacingRowPolicy.Select(live).Reason + ")");
Console.WriteLine($"PASS racing row: {checks} checks");
