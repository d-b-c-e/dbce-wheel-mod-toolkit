using System;
using System.Collections.Generic;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>Where a position falls on a <see cref="CoursePath"/>.</summary>
    public struct CourseProjection
    {
        /// <summary>Metres along the path from its first point: [0, LengthM) on a loop, [0, LengthM] otherwise.</summary>
        public double DistanceM;

        /// <summary>Straight-line metres from the position to the closest point on the path.</summary>
        public double OffsetM;
    }

    /// <summary>
    /// A course reference line as a polyline in driving order: the game's own route (stage
    /// waypoints, track path nodes, progress spline), never a guessed or driven line.
    /// </summary>
    /// <remarks>
    /// <para>
    /// Lengths and projections are 3D, so a bridge over another part of the course is not
    /// confused with the road beneath it the way a plan-view projection would be.
    /// </para>
    /// <para>
    /// A loop's closing segment (last point back to the first) is implicit. A point-to-point
    /// path ends at its last point.
    /// </para>
    /// </remarks>
    public sealed class CoursePath
    {
        /// <summary>Shorter paths are rejected as a wrong unit or an empty route.</summary>
        public const double MinLengthM = 10.0;

        /// <summary>Consecutive points closer than this are treated as one.</summary>
        public const double DuplicateM = 0.01;

        private readonly CourseVector[] _points;
        private readonly double[] _cumulative; // metres at the start of each segment, plus the total

        public bool IsLoop { get; }
        public double LengthM { get; }
        public int PointCount => _points.Length;
        public CourseVector this[int index] => _points[index];

        private CoursePath(CourseVector[] points, bool isLoop)
        {
            _points = points;
            IsLoop = isLoop;
            int segments = isLoop ? points.Length : points.Length - 1;
            _cumulative = new double[segments + 1];
            for (int i = 0; i < segments; i++)
                _cumulative[i + 1] = _cumulative[i] + CourseVector.Distance(points[i], points[(i + 1) % points.Length]);
            LengthM = _cumulative[segments];
        }

        public static bool TryCreate(IList<CourseVector> points, bool isLoop, out CoursePath path, out string error)
        {
            path = null;
            if (points == null || points.Count < 2)
            {
                error = "a course needs at least 2 points";
                return false;
            }
            var kept = new List<CourseVector>(points.Count);
            for (int i = 0; i < points.Count; i++)
            {
                if (!points[i].IsFinite)
                {
                    error = "point " + i + " is not finite";
                    return false;
                }
                if (kept.Count == 0 || CourseVector.Distance(kept[kept.Count - 1], points[i]) >= DuplicateM)
                    kept.Add(points[i]);
            }
            // A loop given with its first point repeated at the end closes itself.
            if (isLoop && kept.Count > 1 && CourseVector.Distance(kept[0], kept[kept.Count - 1]) < DuplicateM)
                kept.RemoveAt(kept.Count - 1);
            if (kept.Count < (isLoop ? 3 : 2))
            {
                error = isLoop ? "a loop needs at least 3 distinct points" : "a course needs at least 2 distinct points";
                return false;
            }
            var candidate = new CoursePath(kept.ToArray(), isLoop);
            if (!(candidate.LengthM >= MinLengthM))
            {
                error = "course is " + candidate.LengthM.ToString("0.###") + " m; at least " + MinLengthM + " m expected";
                return false;
            }
            path = candidate;
            error = null;
            return true;
        }

        public static CoursePath Create(IList<CourseVector> points, bool isLoop)
        {
            if (!TryCreate(points, isLoop, out var path, out var error)) throw new ArgumentException(error);
            return path;
        }

        /// <summary>
        /// The same road driven the other way. A loop keeps its start point; a point-to-point
        /// course starts at the old finish.
        /// </summary>
        public CoursePath Reversed()
        {
            var reversed = new CourseVector[_points.Length];
            if (IsLoop)
            {
                reversed[0] = _points[0];
                for (int i = 1; i < _points.Length; i++) reversed[i] = _points[_points.Length - i];
            }
            else
            {
                for (int i = 0; i < _points.Length; i++) reversed[i] = _points[_points.Length - 1 - i];
            }
            return new CoursePath(reversed, IsLoop);
        }

        /// <summary>The authored points, in driving order, for hashing or export.</summary>
        public CourseVector[] ToArray() => (CourseVector[])_points.Clone();

        /// <summary>The point at a distance along the path; wraps on a loop, clamps otherwise.</summary>
        public CourseVector PointAt(double distanceM)
        {
            double d = Normalize(distanceM);
            int segment = SegmentAt(d);
            double length = _cumulative[segment + 1] - _cumulative[segment];
            double t = length > 0 ? (d - _cumulative[segment]) / length : 0;
            return CourseVector.Lerp(_points[segment], _points[(segment + 1) % _points.Length], t);
        }

        /// <summary>Closest point over the whole path.</summary>
        public CourseProjection Project(CourseVector position) => ProjectRange(position, 0, SegmentCount - 1);

        /// <summary>
        /// Closest point among the segments within <paramref name="windowM"/> of
        /// <paramref name="nearDistanceM"/>, so a car on one branch of a crossing stays on it.
        /// </summary>
        public CourseProjection Project(CourseVector position, double nearDistanceM, double windowM)
        {
            if (!(windowM > 0) || windowM * 2 >= LengthM) return Project(position);
            double from = nearDistanceM - windowM, to = nearDistanceM + windowM;
            if (!IsLoop)
                return ProjectRange(position, SegmentAt(Math.Max(0, from)), SegmentAt(Math.Min(LengthM, to)));
            int first = SegmentAt(Normalize(from)), last = SegmentAt(Normalize(to));
            if (first <= last) return ProjectRange(position, first, last);
            var a = ProjectRange(position, first, SegmentCount - 1);
            var b = ProjectRange(position, 0, last);
            return a.OffsetM <= b.OffsetM ? a : b;
        }

        /// <summary>Distance into [0, LengthM): wrapped on a loop, clamped on a point-to-point path.</summary>
        public double Normalize(double distanceM)
        {
            if (IsLoop)
            {
                double d = distanceM % LengthM;
                if (d < 0) d += LengthM;
                return d >= LengthM ? 0 : d;
            }
            return Math.Max(0, Math.Min(LengthM, distanceM));
        }

        private int SegmentCount => _cumulative.Length - 1;

        private int SegmentAt(double d)
        {
            int lo = 0, hi = SegmentCount - 1;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) / 2;
                if (_cumulative[mid] <= d) lo = mid; else hi = mid - 1;
            }
            return lo;
        }

        private CourseProjection ProjectRange(CourseVector p, int first, int last)
        {
            var best = new CourseProjection { OffsetM = double.PositiveInfinity };
            for (int i = first; i <= last; i++)
            {
                var a = _points[i];
                var b = _points[(i + 1) % _points.Length];
                double n = b.North - a.North, e = b.East - a.East, u = b.Up - a.Up;
                double lengthSquared = n * n + e * e + u * u;
                double t = lengthSquared > 0
                    ? ((p.North - a.North) * n + (p.East - a.East) * e + (p.Up - a.Up) * u) / lengthSquared
                    : 0;
                t = Math.Max(0, Math.Min(1, t));
                var closest = CourseVector.Lerp(a, b, t);
                double offset = CourseVector.Distance(p, closest);
                if (offset < best.OffsetM)
                {
                    best.OffsetM = offset;
                    best.DistanceM = Normalize(_cumulative[i] + t * (_cumulative[i + 1] - _cumulative[i]));
                }
            }
            return best;
        }
    }
}
