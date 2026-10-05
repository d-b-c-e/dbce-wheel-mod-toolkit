using System;
using System.Collections.Generic;
using System.Linq;

namespace Dbce.Wheel.Playback
{
    /// <summary>Offline, per-sample descriptive statistics. Units remain adapter-owned.
    /// No gain recommendation or claim of measured hardware output is made.</summary>
    public sealed class SignalStatistics
    {
        public bool Available => Count > 0;
        public int Count { get; private set; }
        public string Unit { get; private set; }
        public double? Minimum { get; private set; }
        public double? Maximum { get; private set; }
        public double? Rms { get; private set; }
        public double? AbsoluteP50 { get; private set; }
        public double? AbsoluteP95 { get; private set; }
        public double? AbsoluteP99 { get; private set; }
        public double? FractionAtUnitLimit { get; private set; }
        public static SignalStatistics Summarize(IEnumerable<double> values, string unit, bool normalized = false)
        {
            if (string.IsNullOrWhiteSpace(unit)) throw new ArgumentException("An explicit unit is required", nameof(unit));
            var data = values.ToArray();
            if (data.Any(v => double.IsNaN(v) || double.IsInfinity(v) || (normalized && Math.Abs(v) > 1)))
                throw new ArgumentException("Nonfinite or out-of-contract signal");
            var result = new SignalStatistics { Count = data.Length, Unit = unit };
            if (data.Length == 0) return result;
            var absolute = data.Select(Math.Abs).OrderBy(v => v).ToArray();
            double P(double p) => absolute[(int)Math.Ceiling(p * data.Length) - 1];
            result.Minimum = data.Min(); result.Maximum = data.Max();
            // Scale first so squaring a finite large input does not overflow.
            double scale = absolute[absolute.Length - 1];
            result.Rms = scale == 0 ? 0 : scale * Math.Sqrt(data.Average(v => (v / scale) * (v / scale)));
            result.AbsoluteP50 = P(.5); result.AbsoluteP95 = P(.95); result.AbsoluteP99 = P(.99);
            if (normalized) result.FractionAtUnitLimit = data.Count(v => Math.Abs(v) >= .9999) / (double)data.Length;
            return result;
        }
    }
}
