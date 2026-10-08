using System;

namespace Dbce.Wheel.Calibration
{
    /// <summary>
    /// Offline candidate calibration for a load-relative axle-force adapter.
    /// Alters the reference BEFORE the production force law's clamp and filter.
    /// This helper is not part of a shipping runtime or a qualified tune.
    /// </summary>
    public sealed class SpeedReferenceCalibration
    {
        public const string Contract = "dbce.axle-reference-calibration@1";
        public float BaseRatio { get; }
        public float StartKmh { get; }
        public float FullKmh { get; }
        public float HighSpeedGain { get; }

        public SpeedReferenceCalibration(float baseRatio, float startKmh, float fullKmh, float highSpeedGain)
        {
            if (!Finite(baseRatio) || baseRatio < .01f || baseRatio > 10f)
                throw new ArgumentOutOfRangeException(nameof(baseRatio));
            if (!Finite(startKmh) || startKmh < 12f || !Finite(fullKmh) || fullKmh <= startKmh || fullKmh > 1000f)
                throw new ArgumentOutOfRangeException(nameof(fullKmh));
            // Deliberate exploratory bound, not a physical safety certification.
            if (!Finite(highSpeedGain) || highSpeedGain < 1f || highSpeedGain > 1.5f)
                throw new ArgumentOutOfRangeException(nameof(highSpeedGain));
            BaseRatio = baseRatio; StartKmh = startKmh; FullKmh = fullKmh; HighSpeedGain = highSpeedGain;
        }

        public float Gain(float speedMs)
        {
            if (!Finite(speedMs)) throw new ArgumentOutOfRangeException(nameof(speedMs));
            double kmh = Math.Abs((double)speedMs) * 3.6;
            double t = Math.Max(0, Math.Min(1, (kmh - StartKmh) / (FullKmh - StartKmh)));
            // Cubic endpoint slopes are zero. Memoryless: no temporal smoothing.
            double blend = t * t * (3 - 2 * t);
            return (float)(1 + (HighSpeedGain - 1) * blend);
        }

        public float ReferenceRatio(float speedMs) => BaseRatio / Gain(speedMs);
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
