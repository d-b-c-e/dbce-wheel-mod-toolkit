using System;
using System.Collections.Generic;
using Dbce.Wheel.Telemetry;

namespace Dbce.Wheel.Telemetry.Tests
{
    /// <summary>
    /// Deterministic synthetic courses and drives. No owner capture is used or needed: every
    /// expected value below follows from the geometry, so a failure is the code's fault.
    /// </summary>
    internal static class SyntheticCourses
    {
        /// <summary>A 2 km point-to-point stage: S-bends east-west, 20 m of climb and fall.</summary>
        public static List<CourseVector> Stage()
        {
            var points = new List<CourseVector>();
            for (double s = 0; s <= 2000; s += 5)
                points.Add(new CourseVector(s, 80 * Math.Sin(s / 200), 20 * Math.Sin(s / 500)));
            return points;
        }

        /// <summary>A flat 300 x 200 m elliptical circuit, about 1586 m, starting due east.</summary>
        public static List<CourseVector> Circuit()
        {
            var points = new List<CourseVector>();
            for (int i = 0; i < 400; i++)
            {
                double a = 2 * Math.PI * i / 400;
                points.Add(new CourseVector(200 * Math.Sin(a), 300 * Math.Cos(a), 0));
            }
            return points;
        }

        /// <summary>A flat figure-eight whose two lobes cross at the origin at the same height.</summary>
        public static List<CourseVector> FigureEight()
        {
            var points = new List<CourseVector>();
            for (int i = 0; i < 600; i++)
            {
                double t = 2 * Math.PI * i / 600;
                points.Add(new CourseVector(300 * Math.Sin(t) * Math.Cos(t), 300 * Math.Sin(t), 0));
            }
            return points;
        }

        /// <summary>
        /// Positions at <paramref name="hz"/> driving from <paramref name="fromM"/> to
        /// <paramref name="toM"/> along the path (wrapping on a loop), weaving up to
        /// <paramref name="weaveM"/> either side of the line, as a real car would.
        /// </summary>
        public static List<(double TrueDistanceM, CourseVector Position)> Drive(CoursePath path, double fromM,
            double toM, double speedMps = 30, double hz = 60, double weaveM = 1.5)
        {
            var samples = new List<(double, CourseVector)>();
            double step = Math.Sign(toM - fromM) * speedMps / hz;
            int count = (int)Math.Floor(Math.Abs(toM - fromM) / Math.Abs(step));
            for (int i = 0; i <= count; i++)
            {
                double d = fromM + i * step;
                samples.Add((d, Beside(path, d, weaveM * Math.Sin(i * 0.05))));
            }
            return samples;
        }

        /// <summary>A point <paramref name="offsetM"/> to one side of the line (plan view) at a distance.</summary>
        public static CourseVector Beside(CoursePath path, double distanceM, double offsetM)
        {
            var here = path.PointAt(distanceM);
            var ahead = path.PointAt(path.IsLoop ? distanceM + 0.5 : Math.Min(distanceM + 0.5, path.LengthM));
            if (CourseVector.Distance(here, ahead) < 1e-6) ahead = here;
            double n = ahead.North - here.North, e = ahead.East - here.East;
            double len = Math.Sqrt(n * n + e * e);
            if (len < 1e-9)
            {
                var behind = path.PointAt(distanceM - 0.5);
                n = here.North - behind.North;
                e = here.East - behind.East;
                len = Math.Sqrt(n * n + e * e);
            }
            // Perpendicular to the heading in plan view; which side does not matter, only that it is consistent.
            return new CourseVector(here.North - e / len * offsetM, here.East + n / len * offsetM, here.Up);
        }
    }
}
