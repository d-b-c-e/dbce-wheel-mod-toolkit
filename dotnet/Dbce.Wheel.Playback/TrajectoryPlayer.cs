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
        public int Applied { get; private set; }
        public double MaximumPositionError { get; private set; }
        public double MaximumRotationErrorDegrees { get; private set; }
        public bool Passed { get; private set; }
        public TrajectoryPlayer(TrajectoryTape tape, ITrajectoryTarget target) { this.tape = tape; this.target = target; }
        public bool Step()
        {
            if (closed) throw new InvalidOperationException("Playback is closed");
            try
            {
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
                }
                // Verify the final pose on its next tick too; writing it alone is not success.
                if (Applied == tape.Frames.Length) { Passed = true; Dispose(); return false; }
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
