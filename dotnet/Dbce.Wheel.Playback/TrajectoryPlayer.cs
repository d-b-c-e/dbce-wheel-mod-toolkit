using System;
using System.IO;

namespace Dbce.Wheel.Playback
{
    public interface ITrajectoryTarget
    {
        // Acquire suppresses competing physics/reset/input paths, and captures original body flags.
        // Release restores them even after failure. Physical output suppression is process-scoped.
        void Acquire();
        void Apply(TrajectoryFrame frame);
        TrajectoryFrame Read();
        void Release();
    }

    /// <summary>One recorded frame per fixed step; verifies the prior command before advancing.</summary>
    public sealed class TrajectoryPlayer : IDisposable
    {
        private readonly TrajectoryTape tape;
        private readonly ITrajectoryTarget target;
        private bool acquired, closed;
        private int requestedEnd;
        public int Applied { get; private set; }
        /// <summary>Number of applied poses subsequently verified by Read, not merely written.</summary>
        public int Verified { get; private set; }
        public double MaximumPositionError { get; private set; }
        public double MaximumRotationErrorDegrees { get; private set; }
        public bool Passed { get; private set; }
        public TrajectoryPlayer(TrajectoryTape tape, ITrajectoryTarget target) { this.tape = tape; this.target = target; }
        public bool Step() => StepUntil(tape.Frames.Length);

        /// <summary>
        /// Applies at most one frame before exclusiveEnd, verifying the previous command first.
        /// False at an interior boundary means its last pose has been verified; ownership remains held.
        /// Resume with a larger end. Calls at the same boundary recheck the held pose without advancing.
        /// Only the complete tape's verified final pose sets Passed and releases ownership.
        /// Ends must be positive, within the tape and nondecreasing. Failure closes the player.
        /// The host calls this on subsequent physics callbacks, never immediately after Apply to declare success.
        /// </summary>
        public bool StepUntil(int exclusiveEnd)
        {
            if (closed) throw new InvalidOperationException("Playback is closed");
            try
            {
                if (exclusiveEnd < 1 || exclusiveEnd > tape.Frames.Length)
                    throw new ArgumentOutOfRangeException(nameof(exclusiveEnd));
                if (exclusiveEnd < requestedEnd)
                    throw new ArgumentException("Trajectory interval ends cannot move backwards", nameof(exclusiveEnd));
                requestedEnd = exclusiveEnd;
                if (!acquired) { acquired = true; target.Acquire(); }
                if (Applied > 0)
                {
                    var current = target.Read(); current.Validate(); var expected = tape.Frames[Applied - 1];
                    double sum = 0; for (int i = 0; i < 3; i++) { double d = current.Values[i] - expected.Values[i]; sum += d * d; }
                    double error = Math.Sqrt(sum), dot = 0, a = 0, b = 0;
                    for (int i = 3; i < 7; i++) { dot += current.Values[i] * (double)expected.Values[i]; a += current.Values[i] * (double)current.Values[i]; b += expected.Values[i] * (double)expected.Values[i]; }
                    double angle = 2 * Math.Acos(Math.Min(1, Math.Abs(dot) / Math.Sqrt(a * b))) * 180 / Math.PI;
                    MaximumPositionError = Math.Max(MaximumPositionError, error);
                    MaximumRotationErrorDegrees = Math.Max(MaximumRotationErrorDegrees, angle);
                    if (error > .02 || angle > .2) throw new InvalidDataException("Trajectory ownership lost: " + error + " m / " + angle + " deg");
                    Verified = Applied;
                }
                // Verify the final pose on its next tick too; writing it alone is not success.
                if (Applied == tape.Frames.Length) { Passed = true; Dispose(); return false; }
                if (Applied == exclusiveEnd) return false;
                target.Apply(tape.Frames[Applied++]); return true;
            }
            catch { Dispose(); throw; }
        }
        public void Dispose()
        {
            if (closed) return; closed = true;
            if (acquired) target.Release();
        }
    }
}
