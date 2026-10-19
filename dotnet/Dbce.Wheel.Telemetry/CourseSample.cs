using System;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>
    /// One course snapshot beside <see cref="TelemetryFrame"/>: which course, where on it, and
    /// the session state a course map needs. Never folded into a Forza packet.
    /// </summary>
    /// <remarks>
    /// <para>
    /// The Forza layouts have no course id, length or progression, and SimHub's Forza readers
    /// report none (no map is ever recorded from them). This struct is the neutral seam a
    /// game adapter fills from the game's own route data; an encoder such as
    /// <see cref="SimHubCourseFeed"/> turns it into a wire format.
    /// </para>
    /// <para>
    /// Units: metres, degrees, seconds. Position is on north/east/up axes (see
    /// <see cref="CourseAxes"/>).
    /// </para>
    /// </remarks>
    public struct CourseSample
    {
        public const uint Schema = 1;

        /// <summary>A <see cref="CourseIdentity.Id"/>.</summary>
        public string CourseId;

        /// <summary>Display name; may be localized. Never used for identity.</summary>
        public string CourseName;

        public double LengthM;
        public bool IsLoop;

        /// <summary>From <see cref="CourseProgress.Progress01"/>; exactly 1 only when a point-to-point course is finished.</summary>
        public double Progress01;

        public uint CompletedLaps;
        public bool Finished;
        public bool HasFix;

        public CourseVector Position;

        /// <summary>0 = north, clockwise, [0, 360).</summary>
        public double YawDeg;

        public double LapTimeS;

        /// <summary>Changes on every new session or stage restart.</summary>
        public ulong SessionId;

        public uint DiscontinuityCount;

        public bool IsSpectating;
        public bool IsReplay;
        public bool IsUserInControl;

        /// <summary>Fills the course fields from an identity, a tracker's path and its latest progress.</summary>
        public static CourseSample From(CourseIdentity identity, string courseName, CoursePath path,
            CourseProgress progress, CourseVector position, double yawDeg, double lapTimeS, ulong sessionId)
        {
            if (identity == null) throw new ArgumentNullException(nameof(identity));
            if (path == null) throw new ArgumentNullException(nameof(path));
            return new CourseSample
            {
                CourseId = identity.Id,
                CourseName = courseName,
                LengthM = path.LengthM,
                IsLoop = path.IsLoop,
                Progress01 = progress.Progress01,
                CompletedLaps = progress.CompletedLaps,
                Finished = progress.Finished,
                HasFix = progress.HasFix,
                Position = position,
                YawDeg = yawDeg,
                LapTimeS = lapTimeS,
                SessionId = sessionId,
                DiscontinuityCount = progress.DiscontinuityCount,
                IsUserInControl = true,
            };
        }

        /// <summary>Checks the invariants an encoder relies on. Returns the first broken one.</summary>
        public bool TryValidate(out string error)
        {
            if (!CourseIdentity.TryParse(CourseId, out _)) { error = "CourseId is not a course id"; return false; }
            if (!CourseVector.Finite(LengthM) || LengthM < CoursePath.MinLengthM) { error = "LengthM must be at least " + CoursePath.MinLengthM; return false; }
            if (!CourseVector.Finite(Progress01) || Progress01 < 0 || Progress01 > 1) { error = "Progress01 must be in [0, 1]"; return false; }
            if (IsLoop && (Progress01 >= 1 || Finished)) { error = "a loop never reports progress 1 or finished"; return false; }
            if (!IsLoop && (Progress01 == 1) != Finished) { error = "progress 1 if and only if finished"; return false; }
            if (!HasFix && (Progress01 != 0 || Finished)) { error = "no fix means progress 0"; return false; }
            if (!Position.IsFinite) { error = "Position is not finite"; return false; }
            if (!CourseVector.Finite(YawDeg) || YawDeg < 0 || YawDeg >= 360) { error = "YawDeg must be in [0, 360)"; return false; }
            if (!CourseVector.Finite(LapTimeS) || LapTimeS < 0) { error = "LapTimeS must be finite and not negative"; return false; }
            error = null;
            return true;
        }
    }
}
