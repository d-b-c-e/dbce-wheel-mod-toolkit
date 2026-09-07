using System;
using System.Globalization;
using System.Text;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// The two diagnostics every FFB bug this month was solved with: a windowed
    /// peak of the raw signal (to set the reference), and a low-rate trace line
    /// of the quantities the force is built from (to see sign and liveness).
    /// Pure formatting; the caller decides where lines go.
    /// </summary>
    public sealed class FfbDiagnostics
    {
        private float _peakRaw, _peakSpeedKmh, _nextPeak, _nextTrace;
        private long _sent, _neg, _pos, _zero;
        private float _min = float.MaxValue, _max = float.MinValue;

        /// <summary>Calls actually attempted at the output API, independent of computed samples.</summary>
        public long DeliveryAttempts { get; private set; }
        public long DeliveryFailures { get; private set; }
        /// <summary>Null until the consumer reports a delivery. Acceptance is not measured wheel movement.</summary>
        public bool? LastDeliverySucceeded { get; private set; }
        public string LastDeliveryError { get; private set; }
        public void RecordDelivery(bool accepted, string error = null)
        {
            DeliveryAttempts++;
            if (!accepted) DeliveryFailures++;
            LastDeliverySucceeded = accepted;
            LastDeliveryError = accepted ? null : error;
        }

        /// <summary>Seconds between peak reports (default 5).</summary>
        public float PeakWindowSeconds = 5f;
        /// <summary>Trace lines per second (default 5).</summary>
        public float TraceHz = 5f;

        public void Reset() { DeliveryAttempts = DeliveryFailures = 0; LastDeliverySucceeded = null; LastDeliveryError = null; _peakRaw = _peakSpeedKmh = 0f; _nextPeak = _nextTrace = 0f; _sent = _neg = _pos = _zero = 0; _min = float.MaxValue; _max = float.MinValue; }

        /// <summary>Record one output; returns a peak-report line when the window elapsed, else null.</summary>
        public string RecordOutput(float rawSignal, float speedKmh, float shapedOutput, float reference, int strength, float now)
        {
            _sent++;
            if (shapedOutput < 0f) _neg++; else if (shapedOutput > 0f) _pos++; else _zero++;
            _min = Math.Min(_min, shapedOutput); _max = Math.Max(_max, shapedOutput);
            float abs = Math.Abs(rawSignal);
            if (abs > _peakRaw) _peakRaw = abs;
            if (speedKmh > _peakSpeedKmh) _peakSpeedKmh = speedKmh;
            if (now < _nextPeak) return null;
            _nextPeak = now + PeakWindowSeconds;
            // Speed matters as much as the peak: a near-zero peak while parked is
            // expected; the same figure at 90 km/h means something is wrong.
            string line = string.Format(CultureInfo.InvariantCulture,
                "FFB peak |signal| {0:F0} over {1:F0}s at up to {2:F0} km/h (reference={3:F0}, strength={4}, output={5:F2})",
                _peakRaw, PeakWindowSeconds, _peakSpeedKmh, reference, strength, shapedOutput);
            _peakRaw = 0f; _peakSpeedKmh = 0f;
            return line;
        }

        /// <summary>A trace line at <see cref="TraceHz"/>, or null between ticks.</summary>
        public string Trace(float now, in ForceInputs i, float structural, float output)
        {
            if (now < _nextTrace) return null;
            _nextTrace = now + 1f / Math.Max(0.5f, TraceHz);
            var sb = new StringBuilder("FFB trace ");
            sb.Append("v=").Append((i.SpeedMps * 3.6f).ToString("F1", CultureInfo.InvariantCulture));
            if (i.HasSteer) sb.Append(" steer=").Append(i.Steer.ToString("F2", CultureInfo.InvariantCulture));
            if (i.HasSlip) sb.Append(" slip=").Append(i.FrontSlipAngleDeg.ToString("F1", CultureInfo.InvariantCulture)).Append(" ideal=").Append(i.IdealSlipAngleDeg.ToString("F1", CultureInfo.InvariantCulture));
            if (i.HasFrontLateralForce) sb.Append(" Fy=").Append(i.FrontLateralForce.ToString("F0", CultureInfo.InvariantCulture));
            if (i.HasLateralG) sb.Append(" latG=").Append(i.LateralG.ToString("F2", CultureInfo.InvariantCulture));
            sb.Append(" struct=").Append(structural.ToString("F3", CultureInfo.InvariantCulture));
            sb.Append(" out=").Append(output.ToString("F3", CultureInfo.InvariantCulture));
            return sb.ToString();
        }

        /// <summary>Support-bundle paragraph: counts, range, sign balance, with a reading.</summary>
        public string Summary(int fullScale = WheelFfbNative.ForceMax)
        {
            var sb = new StringBuilder();
            sb.Append("force samples computed : ").Append(_sent).Append('\n');
            if (DeliveryAttempts == 0) sb.Append("  output acceptance: not reported\n");
            else {
                sb.Append("  output acceptance: ").Append(DeliveryAttempts - DeliveryFailures).Append(" accepted, ").Append(DeliveryFailures).Append(" failed\n");
                if (LastDeliverySucceeded == false) sb.Append("  latest output failed: ").Append(LastDeliveryError ?? "No error detail supplied").Append('\n');
            }
            if (_sent == 0) { sb.Append("  READING: no force samples computed. This can be expected outside driving; check the game hook during a drive."); return sb.ToString(); }

            sb.Append("  range            : ").Append((_min * fullScale).ToString("F0", CultureInfo.InvariantCulture)).Append(" .. ").Append((_max * fullScale).ToString("F0", CultureInfo.InvariantCulture)).Append("   (full scale is +/-").Append(fullScale).Append(")\n");
            sb.Append("  peak             : ").Append((Math.Max(Math.Abs(_min), Math.Abs(_max)) * 100f).ToString("F2", CultureInfo.InvariantCulture)).Append("% of full scale\n");
            sb.Append("  sign balance     : ").Append(_neg).Append(" negative, ").Append(_pos).Append(" positive, ").Append(_zero).Append(" zero\n");
            float peak = Math.Max(Math.Abs(_min), Math.Abs(_max));
            if (peak < 0.05f) sb.Append("  READING: computed forces are tiny. This is expected while parked or faded out; during driving, check signal validity and the reference before changing Strength.");
            else if (_neg == 0 || _pos == 0) sb.Append("  READING: forces only ever point one way. Suspect the sign encoding (double-signed magnitude) or a one-sided input.");
            else sb.Append("  READING: computed forces vary in both directions. Output acceptance and physical wheel response are separate checks.");
            return sb.ToString();
        }
    }
}
