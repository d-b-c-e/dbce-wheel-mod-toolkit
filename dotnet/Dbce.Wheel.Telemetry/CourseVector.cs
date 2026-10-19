using System;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>
    /// A world position in metres on the axes SimHub's external-sim feed uses: north, east, up.
    /// </summary>
    /// <remarks>
    /// Course code works on these named axes rather than on a game's x/y/z so a game's
    /// handedness is decided once, in <see cref="CourseAxes"/>, and never again in the
    /// projection or encoding code.
    /// </remarks>
    public struct CourseVector
    {
        public double North;
        public double East;
        public double Up;

        public CourseVector(double north, double east, double up)
        {
            North = north;
            East  = east;
            Up    = up;
        }

        public bool IsFinite => Finite(North) && Finite(East) && Finite(Up);

        public static double Distance(CourseVector a, CourseVector b)
        {
            double n = a.North - b.North, e = a.East - b.East, u = a.Up - b.Up;
            return Math.Sqrt(n * n + e * e + u * u);
        }

        public static CourseVector Lerp(CourseVector a, CourseVector b, double t) => new CourseVector(
            a.North + (b.North - a.North) * t,
            a.East  + (b.East  - a.East)  * t,
            a.Up    + (b.Up    - a.Up)    * t);

        internal static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
    }

    /// <summary>Axis conventions between game worlds, <see cref="CourseVector"/> and SimHub.</summary>
    public static class CourseAxes
    {
        /// <summary>
        /// Unity world position to north/east/up: north = +Z, east = +X, up = +Y.
        /// </summary>
        /// <remarks>
        /// SimHub defines yaw as "0 = north, positive clockwise (90 = east)" and requires it to
        /// agree with the north/east position fields. Unity's yaw (<c>eulerAngles.y</c>) turns
        /// +Z toward +X, clockwise seen from above, so +Z north and +X east is the only mapping
        /// under which Unity's own yaw already satisfies SimHub's definition.
        /// </remarks>
        public static CourseVector FromUnity(double x, double y, double z) => new CourseVector(z, x, y);

        /// <summary>Unity <c>eulerAngles.y</c> in degrees to SimHub yaw, normalized to [0, 360).</summary>
        public static double YawFromUnity(double eulerYDegrees)
        {
            double yaw = eulerYDegrees % 360.0;
            if (yaw < 0) yaw += 360.0;
            return yaw >= 360.0 ? 0.0 : yaw;
        }

        /// <summary>
        /// The <c>CarCoordinates</c> array SimHub builds from the three position fields:
        /// [-north, up, east] (SimHub 9.13.2 <c>ExternalSimManager.GD_CarCoordinates</c>).
        /// </summary>
        /// <remarks>
        /// Shown so tests and reviews can see what SimHub stores; the feed itself sends the
        /// named fields, never this array. Whether SimHub then draws the map mirrored is an
        /// on-screen check, not something this code can decide.
        /// </remarks>
        public static double[] ToSimHubCarCoordinates(CourseVector p) => new[] { -p.North, p.Up, p.East };
    }
}
