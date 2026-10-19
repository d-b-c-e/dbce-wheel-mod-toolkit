using System;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>What happened on one <see cref="CourseTracker.Update"/>.</summary>
    [Flags]
    public enum CourseFlags
    {
        None          = 0,
        /// <summary>The position was NaN or infinite; nothing changed.</summary>
        InvalidInput  = 1,
        /// <summary>Too far from the course; progress is held at the last good value.</summary>
        OffCourse     = 2,
        /// <summary>The car jumped along the course (reset, teleport, restart); no lap was counted.</summary>
        Discontinuity = 4,
        /// <summary>The car has been moving backwards along the course for a while.</summary>
        WrongWay      = 8,
        /// <summary>A loop's start line was crossed forwards on this update.</summary>
        LapCompleted  = 16,
        /// <summary>A point-to-point course has been completed; progress is exactly 1.</summary>
        Finished      = 32,
        /// <summary>No position has been close enough to the course yet.</summary>
        NoFix         = 64,
    }

    /// <summary>Progress on a course after one update.</summary>
    public struct CourseProgress
    {
        /// <summary>
        /// 0..1 along the course. A loop wraps at the start line and never reports 1; a
        /// point-to-point course reports exactly 1 only once finished, as SimHub requires to
        /// close a non-looping map.
        /// </summary>
        public double Progress01;

        public double DistanceM;
        public double OffsetM;
        public uint CompletedLaps;
        public bool Finished;

        /// <summary>Monotonic; feeds SimHub's <c>PhysicsDiscontinuityCounter</c>.</summary>
        public uint DiscontinuityCount;

        public CourseFlags Flags;

        public bool HasFix => (Flags & CourseFlags.NoFix) == 0;
    }

    /// <summary>Tolerances for <see cref="CourseTracker"/>, in metres.</summary>
    public sealed class CourseTrackerOptions
    {
        /// <summary>Beyond this distance from the course, progress is held.</summary>
        public double MaxOffsetM = 30;

        /// <summary>How far along the course to search around the last position.</summary>
        public double SearchWindowM = 150;

        /// <summary>A larger along-course move in one update is a jump, not driving.</summary>
        public double MaxStepM = 60;

        /// <summary>Continuous backwards travel before <see cref="CourseFlags.WrongWay"/>.</summary>
        public double WrongWayM = 20;

        /// <summary>A point-to-point course is finished this close to its end.</summary>
        public double FinishToleranceM = 3;
    }

    /// <summary>
    /// Turns car positions into course progress, laps and completion, against the game's own
    /// course line.
    /// </summary>
    /// <remarks>
    /// <para>
    /// Projection rather than distance travelled: travelled distance keeps counting through
    /// resets, spins and off-road loops, while the projection is where the car is.
    /// </para>
    /// <para>
    /// Rules, each covered by a test: the search stays near the last position so crossings and
    /// hairpins cannot snap progress across; a move larger than <see cref="CourseTrackerOptions.MaxStepM"/>
    /// is a discontinuity that never counts a lap; a lap counts only when driving enters a lap
    /// the car has never been in, so reversing over the line and driving on, or a reset back
    /// over it, cannot count a lap twice; a point-to-point course
    /// reports 1 only when finished and then holds it until <see cref="Restart"/>.
    /// </para>
    /// </remarks>
    public sealed class CourseTracker
    {
        private readonly CoursePath _path;
        private readonly CourseTrackerOptions _options;

        private bool _hasFix;
        private double _distance;     // along the course, normalized
        private double _unwrapped;    // loops: lap index * length + distance; follows the car through jumps too
        private long _highestLap;     // highest lap index entered by driving; a lap counts only past it
        private double _backwards;
        private uint _laps;
        private bool _finished;
        private uint _discontinuities;
        private CourseProgress _last;

        public CourseTracker(CoursePath path, CourseTrackerOptions options = null)
        {
            _path = path ?? throw new ArgumentNullException(nameof(path));
            _options = options ?? new CourseTrackerOptions();
            _last = new CourseProgress { Flags = CourseFlags.NoFix };
        }

        public CoursePath Path => _path;

        /// <summary>
        /// Starts a new attempt (stage restart, new session) on the same course. Counts as a
        /// discontinuity so consumers reset motion smoothing and map recording.
        /// </summary>
        public void Restart()
        {
            _hasFix = false;
            _laps = 0;
            _finished = false;
            _backwards = 0;
            _discontinuities++;
            _last = new CourseProgress { Flags = CourseFlags.NoFix, DiscontinuityCount = _discontinuities };
        }

        public CourseProgress Update(CourseVector position)
        {
            if (!position.IsFinite)
                return Report(_last.OffsetM, CourseFlags.InvalidInput | (_hasFix ? CourseFlags.None : CourseFlags.NoFix));

            if (!_hasFix)
            {
                var first = _path.Project(position);
                if (first.OffsetM > _options.MaxOffsetM)
                    return Report(first.OffsetM, CourseFlags.OffCourse | CourseFlags.NoFix);
                _hasFix = true;
                _distance = first.DistanceM;
                // On a loop, a grid behind the start line is the end of lap -1, not lap 0 nearly done,
                // and crossing the line from there starts lap 0 rather than completing one.
                _unwrapped = _path.IsLoop && _distance > _path.LengthM / 2 ? _distance - _path.LengthM : _distance;
                _highestLap = Math.Max(0, LapIndex(_unwrapped));
                return Report(first.OffsetM, CourseFlags.None);
            }

            var near = _path.Project(position, _distance, _options.SearchWindowM);
            var flags = CourseFlags.None;
            double step;
            if (near.OffsetM <= _options.MaxOffsetM)
            {
                step = Delta(_distance, near.DistanceM);
            }
            else
            {
                var anywhere = _path.Project(position);
                if (anywhere.OffsetM > _options.MaxOffsetM)
                    return Report(anywhere.OffsetM, CourseFlags.OffCourse);
                near = anywhere;
                step = Delta(_distance, near.DistanceM);
                flags |= CourseFlags.Discontinuity; // back on the course somewhere the window could not see
            }

            if ((flags & CourseFlags.Discontinuity) != 0 || Math.Abs(step) > _options.MaxStepM)
            {
                flags |= CourseFlags.Discontinuity;
                _discontinuities++;
                _backwards = 0;
                _distance = near.DistanceM;
                // Follow the car into whichever lap it landed in, but credit no lap for the jump: a
                // forward jump over the line is not a lap (the lap it lands in counts as entered),
                // and after a backward jump over it the next crossing re-enters a lap already counted.
                _unwrapped += step;
                _highestLap = Math.Max(_highestLap, LapIndex(_unwrapped));
                return Report(near.OffsetM, flags);
            }

            _distance = near.DistanceM;
            _unwrapped += step;
            _backwards = step < 0 ? _backwards - step : 0;
            if (_backwards > _options.WrongWayM) flags |= CourseFlags.WrongWay;

            if (_path.IsLoop)
            {
                long lap = LapIndex(_unwrapped);
                if (lap > _highestLap)
                {
                    _highestLap = lap;
                    _laps++;
                    flags |= CourseFlags.LapCompleted;
                }
            }
            else if (!_finished && step > 0 && _distance >= _path.LengthM - _options.FinishToleranceM)
            {
                // Arriving forwards only: reversing into the last metres of a stage is not a finish.
                _finished = true;
            }
            return Report(near.OffsetM, flags);
        }

        private long LapIndex(double unwrapped) => (long)Math.Floor(unwrapped / _path.LengthM);

        // Signed along-course change; on a loop the shorter way round the start line.
        private double Delta(double from, double to)
        {
            double d = to - from;
            if (_path.IsLoop)
            {
                if (d > _path.LengthM / 2) d -= _path.LengthM;
                else if (d < -_path.LengthM / 2) d += _path.LengthM;
            }
            return d;
        }

        private CourseProgress Report(double offset, CourseFlags flags)
        {
            if (_finished) flags |= CourseFlags.Finished;
            double progress;
            if (!_hasFix) progress = 0;
            else if (_path.IsLoop) progress = _distance / _path.LengthM;
            else if (_finished) progress = 1.0;
            else progress = Math.Min(_distance / _path.LengthM, MaxUnfinished);

            _last = new CourseProgress
            {
                Progress01 = progress,
                DistanceM = _hasFix ? (_finished ? _path.LengthM : _distance) : 0,
                OffsetM = offset,
                CompletedLaps = _laps,
                Finished = _finished,
                DiscontinuityCount = _discontinuities,
                Flags = flags,
            };
            return _last;
        }

        // The largest progress below 1 a float32 can carry, so an unfinished course never reads as 1.
        private const double MaxUnfinished = 0.99999994;
    }
}
