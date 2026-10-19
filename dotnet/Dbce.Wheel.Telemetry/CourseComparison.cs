using System;
using System.Collections.Generic;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>What <see cref="CourseComparison.Run"/> measured. Pass thresholds belong to the caller.</summary>
    public sealed class CourseComparisonResult
    {
        public int Samples;
        public bool FixOnFirstSample;
        public int Invalid;
        public int OffCourse;
        public int Discontinuities;

        /// <summary>Steps that went backwards by more than 0.5 m outside a wrong-way span.</summary>
        public int UnflaggedBackwardSteps;

        public double OffsetP95M;
        public double OffsetMaxM;

        /// <summary>Forward progress along the course, metres (sum of along-course steps, jumps excluded).</summary>
        public double CourseDistanceM;

        /// <summary>The trajectory's own plan-view path length, metres.</summary>
        public double PlanPathM;

        /// <summary>Median of tracker distance / game progress where both exceed 10 m; null without game progress.</summary>
        public double? GameScale;

        public double? GameDeltaMedianM;
        public double? GameDeltaP95M;
    }

    /// <summary>
    /// Runs a <see cref="CourseTracker"/> over a captured trajectory on a game's own route and measures how well
    /// the two agree, optionally against the game's own progress samples.
    /// </summary>
    /// <remarks>
    /// Used offline on a route capture (for example Woden's <c>dbce.course-route@1</c>) and a recorded
    /// trajectory. The game's progress unit is not assumed: its scale to metres is estimated first and reported,
    /// and deltas are taken after scaling, so a unit mismatch shows as a scale rather than as a large error.
    /// </remarks>
    public static class CourseComparison
    {
        public static CourseComparisonResult Run(CoursePath path, IList<CourseVector> positions,
            IList<double> gameProgress = null, CourseTrackerOptions options = null)
        {
            if (path == null) throw new ArgumentNullException(nameof(path));
            if (positions == null) throw new ArgumentNullException(nameof(positions));
            if (gameProgress != null && gameProgress.Count != positions.Count)
                throw new ArgumentException("game progress needs one value per position", nameof(gameProgress));

            var tracker = new CourseTracker(path, options);
            var result = new CourseComparisonResult { Samples = positions.Count };
            var offsets = new List<double>();
            var pairs = new List<(double Tracker, double Game)>();
            double startUnwrapped = double.NaN, unwrapped = 0, previous = double.NaN;
            CourseVector? lastPosition = null;

            for (int i = 0; i < positions.Count; i++)
            {
                var p = tracker.Update(positions[i]);
                if (i == 0) result.FixOnFirstSample = p.HasFix;
                if ((p.Flags & CourseFlags.InvalidInput) != 0) { result.Invalid++; continue; }
                if (lastPosition is CourseVector last)
                {
                    double n = positions[i].North - last.North, e = positions[i].East - last.East;
                    result.PlanPathM += Math.Sqrt(n * n + e * e);
                }
                lastPosition = positions[i];
                if ((p.Flags & CourseFlags.OffCourse) != 0) { result.OffCourse++; continue; }
                if (!p.HasFix) continue;
                offsets.Add(p.OffsetM);

                if ((p.Flags & CourseFlags.Discontinuity) != 0) result.Discontinuities++;
                else if (!double.IsNaN(previous))
                {
                    double step = p.DistanceM - previous;
                    if (path.IsLoop)
                    {
                        if (step > path.LengthM / 2) step -= path.LengthM;
                        else if (step < -path.LengthM / 2) step += path.LengthM;
                    }
                    unwrapped += step;
                    if (step < -0.5 && (p.Flags & CourseFlags.WrongWay) == 0) result.UnflaggedBackwardSteps++;
                }
                if (double.IsNaN(startUnwrapped)) startUnwrapped = unwrapped;
                previous = p.DistanceM;

                if (gameProgress != null && CourseVector.Finite(gameProgress[i]))
                    pairs.Add((unwrapped - startUnwrapped, gameProgress[i]));
            }

            result.CourseDistanceM = double.IsNaN(startUnwrapped) ? 0 : unwrapped - startUnwrapped;
            if (offsets.Count > 0)
            {
                offsets.Sort();
                result.OffsetP95M = Percentile(offsets, 0.95);
                result.OffsetMaxM = offsets[offsets.Count - 1];
            }
            if (pairs.Count > 0) CompareGame(pairs, result);
            return result;
        }

        private static void CompareGame(List<(double Tracker, double Game)> pairs, CourseComparisonResult result)
        {
            // Both series are taken relative to their first value, so differing origins do not count as error.
            double tracker0 = pairs[0].Tracker, game0 = pairs[0].Game;
            var ratios = new List<double>();
            foreach (var (t, g) in pairs)
                if (t - tracker0 > 10 && g - game0 > 10) ratios.Add((t - tracker0) / (g - game0));
            if (ratios.Count == 0) return;
            ratios.Sort();
            double scale = Percentile(ratios, 0.5);
            var deltas = new List<double>(pairs.Count);
            foreach (var (t, g) in pairs) deltas.Add(Math.Abs((t - tracker0) - (g - game0) * scale));
            deltas.Sort();
            result.GameScale = scale;
            result.GameDeltaMedianM = Percentile(deltas, 0.5);
            result.GameDeltaP95M = Percentile(deltas, 0.95);
        }

        private static double Percentile(List<double> sorted, double q) =>
            sorted[Math.Min(sorted.Count - 1, (int)Math.Floor(q * (sorted.Count - 1) + 0.5))];
    }
}
