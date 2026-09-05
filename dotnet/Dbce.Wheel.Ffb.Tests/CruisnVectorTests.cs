using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    /// <summary>
    /// Holds <c>cruisn-vunit@1</c> to the conditioning it replaces.
    /// </summary>
    /// <remarks>
    /// <para>
    /// Cruis'n's force feedback was hand-written inside <c>vunit.exe</c>
    /// (<c>mvffb</c> in <c>midvunit_v.cpp</c>). Moving it onto this toolkit is
    /// only worth doing if the feel survives, and "it feels the same" is not a
    /// claim anyone can hold a build to. So the old algorithm is transcribed here
    /// verbatim as <see cref="MvffbReference"/>, and both are driven with the
    /// same 62 seconds of real motor bytes captured on the rig.
    /// </para>
    /// <para>
    /// The vector is the INPUT side - the signed byte the arcade code wrote - so
    /// this compares two conditioning chains rather than re-checking a recording
    /// of one of them. Same technique that caught <c>simlite@1</c>.
    /// </para>
    /// <para>
    /// Both are driven at the FFB worker's real 4 ms cadence with the last
    /// written byte held in between, because that is what the worker does. Driving
    /// them at the trace's own row spacing instead makes the comparison meaningless:
    /// the gaps between writes run past <see cref="ForceShaper.Shape"/>'s 100 ms
    /// dt clamp, and the two filters then advance by different amounts for reasons
    /// that have nothing to do with either algorithm.
    /// </para>
    /// </remarks>
    public class CruisnVectorTests
    {
        private const int StrengthPct = 50;   // [collection] ffb = 50, the rig's setting
        private const int SmoothMs = 50;      // [collection] ffb_smooth = 50
        private const int TickMs = 4;         // the mvffb worker's wait when smoothing

        private static string Find(string rel, string leaf)
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null && !File.Exists(Path.Combine(dir.FullName, rel, leaf)))
                dir = dir.Parent;
            Assert.True(dir != null, rel + "/" + leaf + " not found");
            return Path.Combine(dir.FullName, rel, leaf);
        }

        /// <summary>The captured bytes expanded onto the worker's 4 ms cadence.</summary>
        private static List<int> Ticks()
        {
            var raw = new List<KeyValuePair<int, int>>();
            foreach (var line in File.ReadAllLines(Find("data", "cruisn-usa-mvffb-0.3.7-wheel-trace.csv")))
            {
                if (line.Length == 0 || line[0] == '#' || line.StartsWith("ms,")) continue;
                var f = line.Split(',');
                if (f.Length != 3) continue;
                raw.Add(new KeyValuePair<int, int>(
                    int.Parse(f[0], CultureInfo.InvariantCulture),
                    int.Parse(f[2], CultureInfo.InvariantCulture)));
            }
            Assert.True(raw.Count > 1000, "expected the full capture, got " + raw.Count);

            var ticks = new List<int>();
            int i = 0, cur = 0;
            for (int ms = raw[0].Key; ms <= raw[raw.Count - 1].Key; ms += TickMs)
            {
                while (i < raw.Count && raw[i].Key <= ms) { cur = raw[i].Value; i++; }
                ticks.Add(cur);
            }
            return ticks;
        }

        /// <summary>
        /// mvffb v0.3.7's conditioning, transcribed from <c>midvunit_v.cpp</c>:
        /// the gain from <c>midv_ffb_write</c>, then the worker's impulse bypass,
        /// dt-aware low-pass, settle-to-zero and re-level threshold. Returns the
        /// level actually handed to SDL on each tick.
        /// </summary>
        private static int[] MvffbReference(List<int> ticks)
        {
            double full = 32767.0 * StrengthPct / 100.0;
            double filt = 0.0;
            int prevWant = 0, applied = 0;
            var outp = new int[ticks.Count];

            for (int i = 0; i < ticks.Count; i++)
            {
                int f = ticks[i];
                int level = 0;
                if (f != 0 && f != -128)
                {
                    double p = Math.Min(1.0, Math.Abs((double)f) / 126.0);
                    level = (int)(p * 32767.0 * StrengthPct / 100.0 + 0.5);
                    if (f > 0) level = -level;      // invert = false
                }
                int want = level;

                if (want != prevWant)
                {
                    if (Math.Abs(want - prevWant) >= full * 0.6) filt = want;
                    prevWant = want;
                }

                double a = 1.0 - Math.Exp(-(double)TickMs / SmoothMs);
                filt += (want - filt) * a;
                int outv = (int)Math.Round(filt);
                if (want == 0 && Math.Abs(outv) < 160) { filt = 0.0; outv = 0; }

                if (Math.Abs(outv - applied) >= 33 || (outv == 0) != (applied == 0))
                    applied = outv;
                outp[i] = applied;
            }
            return outp;
        }

        private static ForceShaper Shaper(Action<ForceShaper> tweak = null)
        {
            var p = ForceProfile.Load(Find("profiles", "force-profiles.ini"), "cruisn-vunit@1");
            var sh = p.Shaper;
            // Their FFB STRENGTH is a plain percent; Strength here is 50-is-unity.
            sh.Strength = StrengthPct / 2;
            if (tweak != null) tweak(sh);
            return sh;
        }

        private static int[] ToolkitPath(List<int> ticks, Action<ForceShaper> tweak = null)
        {
            var sh = Shaper(tweak);
            var outp = new int[ticks.Count];
            for (int i = 0; i < ticks.Count; i++)
            {
                int f = ticks[i];
                double p01 = (f == 0 || f == -128) ? 0.0 : Math.Min(1.0, Math.Abs((double)f) / 126.0);
                float v = (float)(f > 0 ? -p01 : p01);
                outp[i] = (int)Math.Round(sh.Shape(v, 0f, TickMs / 1000f) * 32767f);
            }
            return outp;
        }

        private static int Worst(int[] a, int[] b)
        {
            int w = 0;
            for (int i = 0; i < a.Length; i++) w = Math.Max(w, Math.Abs(a[i] - b[i]));
            return w;
        }

        [Fact]
        public void TheTraceIsRepresentative()
        {
            var t = Ticks();
            Assert.True(t.Count > 10000, "expected ~15.5k ticks of 4 ms, got " + t.Count);
            Assert.Contains(t, b => b > 0);
            Assert.Contains(t, b => b < 0);              // both directions
            Assert.Contains(t, b => Math.Abs(b) > 100);  // near-full kicks
            Assert.Contains(t, b => b == 0);             // and releases
        }

        /// <summary>The whole point: the converted chain matches the one it replaces.</summary>
        [Fact]
        public void ToolkitReproducesMvffbOverTheRealTrace()
        {
            var ticks = Ticks();
            int[] want = MvffbReference(ticks);
            int[] got = ToolkitPath(ticks);

            long sum = 0;
            for (int i = 0; i < ticks.Count; i++) sum += Math.Abs(want[i] - got[i]);
            double mean = (double)sum / ticks.Count;
            int worst = Worst(want, got);

            // 33 of 32767 is one re-level step, which is the closest two
            // implementations can be: they differ only in where rounding happens.
            Assert.True(worst <= 40, $"worst difference {worst} of 32767");
            Assert.True(mean <= 3.0, $"mean difference {mean:F3} of 32767");
        }

        /// <summary>
        /// The three behaviours that change the FORCE. Remove any and the trace
        /// stops matching, by a lot - so the match above is not a coincidence.
        /// </summary>
        [Fact]
        public void TheBorrowedBehavioursAreLoadBearing()
        {
            var ticks = Ticks();
            int[] want = MvffbReference(ticks);

            Assert.True(Worst(want, ToolkitPath(ticks, s => s.SmoothingMs = 0f)) > 1000,
                "the ms time constant is not being used");
            Assert.True(Worst(want, ToolkitPath(ticks, s => s.ImpulseBypass = 0f)) > 1000,
                "the impulse bypass is not being used");
            Assert.True(Worst(want, ToolkitPath(ticks, s => s.SettleBelow = 0f)) > 100,
                "settle-to-zero is not being used");
        }

        /// <summary>
        /// The fourth one is measured differently on purpose. Re-level hysteresis
        /// cannot move the output by more than its own threshold - that is what it
        /// is - so judging it by force would always look like nothing. What it buys
        /// is device traffic, and that is what is asserted.
        /// </summary>
        [Fact]
        public void RelevelHysteresisCutsDeviceTrafficWithoutMovingTheForce()
        {
            var ticks = Ticks();
            Func<Action<ForceShaper>, int> updates = tweak =>
            {
                int[] o = ToolkitPath(ticks, tweak);
                int n = 0;
                for (int i = 1; i < o.Length; i++) if (o[i] != o[i - 1]) n++;
                return n;
            };

            int on = updates(null);
            int off = updates(s => s.RelevelHysteresis = 0f);
            // Measured on this trace: 5,872 updates with it against 9,437 without,
            // out of 15,552 ticks - a bit over a third of the traffic removed.
            Assert.True(off > on * 1.4,
                $"expected hysteresis to cut device updates substantially; on={on} off={off} of {ticks.Count}");

            // and it stays within its own threshold, as its definition requires
            Assert.True(Worst(ToolkitPath(ticks), ToolkitPath(ticks, s => s.RelevelHysteresis = 0f)) <= 40,
                "hysteresis moved the force by more than one step");
        }
    }
}
