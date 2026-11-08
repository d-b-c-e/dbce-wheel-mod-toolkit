using System.Globalization;
using Dbce.Wheel.Display;

// TriplePanels against the triple toolkit's shared geometry vectors (native/geometry/geometry-vectors.txt, STD-047):
// every rig TripleRig can describe (one side angle for both sides, eye level with the center panel's middle) gives
// the vectors' yaw and unit-distance frustum for an eye at the origin, and every fov line gives its eye distance.
internal static class GeometryVectorChecks
{
    internal static int Run()
    {
        int checks = 0, rigs = 0;
        void Check(bool ok, string why) { if (!ok) throw new Exception(why); checks++; }
        static double D(string text) => double.Parse(text, NumberStyles.Float, CultureInfo.InvariantCulture);
        static bool Near(double actual, double expected) => Math.Abs(actual - expected) <= 1e-9 * Math.Max(1, Math.Abs(expected));

        string path = Path.Combine(AppContext.BaseDirectory, "geometry-vectors.txt");
        Check(File.Exists(path), "geometry-vectors.txt was not copied beside the test");
        string? name = null;
        TripleRig? rig = null;
        PanelFrustum[]? views = null;
        foreach (string raw in File.ReadLines(path))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line[0] == '#') continue;
            string[] f = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            int panel = f.Length > 1 ? Array.IndexOf(new[] { "left", "center", "right" }, f[1]) : -1;
            switch (f[0])
            {
                case "rig":
                    name = f[1]; rig = null; views = null;
                    // rig NAME width height eye left right eye_height bezel curve_radius
                    if (f[5] == f[6] && D(f[7]) == 0)
                    {
                        rig = new TripleRig(D(f[2]), D(f[3]), D(f[4]), D(f[5]), D(f[8]), D(f[9]));
                        views = TriplePanels.Compute(rig.Value);
                        rigs++;
                    }
                    break;
                case "edge" when rig is not null:
                    Check(Near(rig.Value.EyeDistanceMm, D(f[1])), name + ": the measured eye is not the panel-edge distance");
                    break;
                case "panel" when views is not null:
                    Check(Near(views[panel].YawDegrees, D(f[2])), name + " " + f[1] + ": yaw differs from the vectors");
                    break;
                case "view" when views is not null && D(f[2]) == 0 && D(f[3]) == 0 && D(f[4]) == 0:
                    var v = views[panel];
                    Check(Near(v.Left, D(f[5])) && Near(v.Right, D(f[6])) && Near(v.Bottom, D(f[7])) && Near(v.Top, D(f[8])),
                        name + " " + f[1] + ": frustum differs from the vectors");
                    break;
                case "fov":
                    // fov height degrees eye: a flat center panel of that height framed for that field of view.
                    var flat = new TripleRig(708.4, D(f[1]), 660, 0, 0);
                    var center = TriplePanels.Compute(flat, D(f[2]))[1];
                    Check(Near(center.Top, D(f[1]) / 2 / D(f[3])) && Math.Abs(TriplePanels.VerticalFov(center) - D(f[2])) < 1e-9,
                        "fov " + f[2] + ": framing eye differs from the vectors");
                    break;
            }
        }
        Check(rigs >= 4, "Fewer than four symmetric eye-level rigs in the vectors");
        return checks;
    }
}
