using System;
using System.Globalization;
using System.IO;
using System.Text;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    /// <summary>
    /// Pins the managed force model to <c>native/forcemodel/conformance.expected.txt</c>,
    /// the same golden file <c>conformance.cpp</c> checks itself against. If both
    /// match the golden they match each other, and any change in how a game feels
    /// shows up as a diff in the repo instead of a surprise at the wheel.
    /// </summary>
    /// <remarks>
    /// Regenerate deliberately with <c>native\forcemodel\build_conformance.bat --update</c>
    /// and commit the diff. Do not "fix" a failure by regenerating without reading it.
    /// </remarks>
    public class ForceConformanceTests
    {
        private const int Ticks = 200;
        private const float Dt = 1f / 60f;
        private const float Tolerance = 1e-4f;   // tanh/sin differ in the last bits between runtimes

        private static string RepoFile(string relative)
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null && !File.Exists(Path.Combine(dir.FullName, relative)))
                dir = dir.Parent;
            return dir == null ? null : Path.Combine(dir.FullName, relative);
        }

        // Mirrors tick_inputs() in conformance.cpp exactly.
        private static ForceInputs TickInputs(int n)
        {
            float t = n / 200f;
            return new ForceInputs
            {
                Steer = (float)Math.Sin(t * 6.2831853f),
                HasSteer = true,
                SteerRate = (float)Math.Cos(t * 6.2831853f) * 6.2831853f,
                HasSteerRate = true,
                SpeedMps = 2f + 38f * t,
                LateralG = 1.4f * (float)Math.Sin(t * 3.14159265f),
                HasLateralG = true,
                FrontLateralForce = 9000f * (float)Math.Sin(t * 3.14159265f),
                HasFrontLateralForce = (n % 2) == 0,
                LongitudinalG = -0.8f + 1.6f * t,
                HasLongitudinalG = true,
                FrontSlipAngleDeg = 14f * t,
                IdealSlipAngleDeg = 8f,
                HasSlip = true,
                DriftAmount = t,
                HasDrift = true,
                Texture = (n % 20 < 10) ? 0.7f : 0f,
                Impact = (n == 50) ? 0.9f : 0f,
                ImpactDirection = (n == 50) ? -1f : 0f,
                GearShift = (n == 100),
            };
        }

        private static string F6(float v) => v.ToString("0.000000", CultureInfo.InvariantCulture);

        private static void DumpSettings(StringBuilder sb, ForceProfile p)
        {
            var m = p.Model; var s = p.Shaper;
            var rows = new (string, float)[]
            {
                ("model.spring.strength", m.SpringStrength),
                ("model.spring.fullSpeedMps", m.SpringFullSpeedMps),
                ("model.damper.strength", m.DamperStrength),
                ("model.damper.staticFraction", m.DamperStaticFraction),
                ("model.lateral.weight", m.SteeringWeight),
                ("model.lateral.forceReference", m.LateralForceReference),
                ("model.lateral.gReference", m.LateralGReference),
                ("model.lateral.trailFloor", m.TrailFloor),
                ("model.lateral.trailSlipSpan", m.TrailSlipSpan),
                ("model.lateral.gripLoss", m.GripLoss),
                ("model.load.weightTransfer", m.WeightTransfer),
                ("model.load.transferMin", m.WeightTransferMin),
                ("model.load.transferMax", m.WeightTransferMax),
                ("model.texture.strength", m.TextureStrength),
                ("model.texture.hz", m.TextureHz),
                ("model.impact.strength", m.ImpactStrength),
                ("model.impact.seconds", m.ImpactSeconds),
                ("model.shift.strength", m.ShiftPulseStrength),
                ("model.shift.seconds", m.ShiftPulseSeconds),
                ("shaper.strength", s.Strength),
                ("shaper.invert", s.Invert ? 1f : 0f),
                ("shaper.deadzone", s.Deadzone),
                ("shaper.attackSmoothing", s.AttackSmoothing),
                ("shaper.decaySmoothing", s.DecaySmoothing),
                ("shaper.softSaturation", s.SoftSaturation),
                ("shaper.slewPerSecond", s.SlewPerSecond),
                ("shaper.outputDeadband", s.OutputDeadband),
                ("shaper.fadeStartKmh", s.FadeStartKmh),
                ("shaper.fadeFullKmh", s.FadeFullKmh),
                ("shaper.rampSeconds", s.RampSeconds),
                ("shaper.peakLimit", s.PeakLimit),
            };
            foreach (var (k, v) in rows) sb.Append("S ").Append(k).Append(' ').Append(F6(v)).Append('\n');
        }

        private static string BuildDump(string profileText)
        {
            var sb = new StringBuilder();
            sb.Append("# dbce force-model conformance. Regenerate deliberately: conformance --update\n");
            foreach (var id in ForceProfile.ListIds(profileText))
            {
                var p = ForceProfile.Parse(profileText, id);
                sb.Append("P ").Append(p.Id).Append('\n');
                DumpSettings(sb, p);

                var model = new ForceModel(p.Model);
                var shaper = p.Shaper;
                shaper.Reset();
                for (int n = 0; n < Ticks; n++)
                {
                    var i = TickInputs(n);
                    float f = model.Compute(i, Dt);
                    float g = shaper.Shape(f, i.SpeedMps * 3.6f, Dt, model.LastWasEvent);
                    sb.Append("T ").Append(n.ToString(CultureInfo.InvariantCulture)).Append(' ')
                      .Append(F6(f)).Append(' ').Append(F6(g)).Append('\n');
                }
            }
            return sb.ToString();
        }

        [Fact]
        public void ManagedModelMatchesTheGoldenFileTheNativeOneChecks()
        {
            string profilesPath = RepoFile(Path.Combine("profiles", "force-profiles.ini"));
            string goldenPath = RepoFile(Path.Combine("native", "forcemodel", "conformance.expected.txt"));
            Assert.True(profilesPath != null, "profiles/force-profiles.ini not found");
            Assert.True(goldenPath != null,
                "native/forcemodel/conformance.expected.txt not found - run build_conformance.bat --update");

            string[] got = BuildDump(File.ReadAllText(profilesPath)).Replace("\r\n", "\n").Split('\n');
            string[] want = File.ReadAllText(goldenPath).Replace("\r\n", "\n").Split('\n');

            int n2 = Math.Max(got.Length, want.Length);
            for (int i = 0; i < n2; i++)
            {
                string a = i < got.Length ? got[i] : "(end)";
                string b = i < want.Length ? want[i] : "(end)";
                if (a == b) continue;

                // Numeric lines compare with a tolerance; anything else must match.
                if (a.StartsWith("T ") && b.StartsWith("T "))
                {
                    var pa = a.Split(' '); var pb = b.Split(' ');
                    if (pa.Length == 4 && pb.Length == 4 && pa[1] == pb[1] &&
                        Math.Abs(P(pa[2]) - P(pb[2])) < Tolerance &&
                        Math.Abs(P(pa[3]) - P(pb[3])) < Tolerance)
                        continue;
                }
                Assert.True(false,
                    "line " + (i + 1) + " differs from the golden file.\n  managed: " + a + "\n  golden : " + b +
                    "\nIf the change was intended, run native\\forcemodel\\build_conformance.bat --update and commit the diff.");
            }
        }

        private static float P(string s) => float.Parse(s, CultureInfo.InvariantCulture);

        [Fact]
        public void TheGoldenFileCoversEveryShippedProfile()
        {
            string profilesPath = RepoFile(Path.Combine("profiles", "force-profiles.ini"));
            string goldenPath = RepoFile(Path.Combine("native", "forcemodel", "conformance.expected.txt"));
            string golden = File.ReadAllText(goldenPath);
            foreach (var id in ForceProfile.ListIds(File.ReadAllText(profilesPath)))
                Assert.True(golden.Contains("P " + id + "\n") || golden.Contains("P " + id + "\r\n"),
                    id + " is not in the golden file - regenerate it after adding a profile");
        }
    }
}
