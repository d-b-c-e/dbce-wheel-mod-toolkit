using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>Opt-in interval-integrated impact mixing with a final command budget.
    /// Pure arithmetic: no collision detection, device access or physical-output claim.</summary>
    public sealed class CadenceImpactMixer
    {
        public const double MaximumIntervalSeconds = .050;
        public const int RequiredHoldTimeoutMs = 150;
        public float PeakLimit { get; }
        public float SlewPerSecond { get; }
        public float Headroom { get; }
        public ulong RejectedEvents { get; private set; }
        public bool LastInputRejected { get; private set; }
        public string Status { get; private set; } = "reset";
        public float LastIntervalPulse { get; private set; }
        public float LastTarget { get; private set; }
        public float LastCommand { get; private set; }
        public bool LastHadImpact { get; private set; }
        private struct Event { public double Time; public float Amplitude; }
        private readonly Event[] _events = new Event[8];
        private int _count;
        private double _lastEnd = -1, _lastEventTime = -1;
        private static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
        private static float Bound(float v, float lo, float hi) => Math.Max(lo, Math.Min(v, hi));
        public CadenceImpactMixer(float peakLimit = .25f, float slewPerSecond = 1.5f, float headroom = .25f)
        {
            if (!Finite(peakLimit) || peakLimit < 0 || peakLimit > 1 || !Finite(slewPerSecond) || slewPerSecond <= 0 ||
                !Finite(headroom) || headroom < 0 || headroom > .5f) throw new ArgumentOutOfRangeException("Invalid force budget");
            PeakLimit = peakLimit; SlewPerSecond = slewPerSecond; Headroom = headroom;
        }
        public void Reset()
        {
            _count = 0; _lastEnd = _lastEventTime = -1; LastCommand = LastTarget = LastIntervalPulse = 0;
            LastHadImpact = LastInputRejected = false; Status = "reset";
        }
        private float Reject(string reason) { Reset(); LastInputRejected = true; Status = reason; return 0; }
        /// <summary>Queue only an explicitly verified, signed event timestamped in the monotonic output clock.
        /// Unverified input clears pending events; it never derives an impact from structural force.</summary>
        public bool Trigger(float amplitude, float direction, double seconds, bool inputVerified = false)
        {
            if (!inputVerified || !Finite(amplitude) || amplitude <= 0 || amplitude > 1 ||
                (direction != -1 && direction != 1) || !Finite(seconds) || seconds < 0 ||
                seconds < _lastEnd || seconds < _lastEventTime)
            {
                RejectedEvents++; _count = 0; Status = inputVerified ? "invalid-event" : "unverified-event"; return false;
            }
            if (_count == _events.Length) { RejectedEvents++; Status = "event-capacity"; return false; }
            _events[_count++] = new Event { Time = seconds, Amplitude = amplitude * direction };
            _lastEventTime = seconds; return true;
        }
        /// <summary>Mix an interval-average event demand with an already normalized structural command.
        /// Use adjacent monotonic OUTPUT-clock intervals, positive and at most 50 ms, not simulation catch-up time. Safety zero/reset bypasses slew.
        /// The caller must submit through its existing guarded, 150 ms hold-watchdog force route.</summary>
        public float Mix(float structural, double start, double end, bool active, bool impactInputsVerified = false)
        {
            if (!active) { Reset(); Status = "inactive-zero"; return 0; }
            double dt = end - start;
            if (!Finite(structural) || !Finite(start) || !Finite(end) || start < 0 || dt <= 0 ||
                dt > MaximumIntervalSeconds + 1e-9 || (_lastEnd >= 0 && Math.Abs(start - _lastEnd) > 1e-9))
                return Reject("invalid-or-stale-interval");
            if (!impactInputsVerified) _count = 0;
            double pulse = 0; LastHadImpact = false;
            for (int i = 0; i < _count; i++)
            {
                var e = _events[i];
                if (e.Time > end || (_lastEnd < 0 && e.Time < start)) return Reject("event-outside-initial-interval");
                if (e.Time < end && e.Time + .100 > start)
                {
                    LastHadImpact = true;
                    pulse += e.Amplitude * (ImpactMixer.EnvelopeIntegral(end - e.Time) - ImpactMixer.EnvelopeIntegral(start - e.Time)) / dt;
                }
            }
            LastIntervalPulse = Bound((float)pulse, -1, 1);
            float structuralBounded = Bound(structural, -PeakLimit, PeakLimit);
            LastTarget = LastHadImpact ? structuralBounded * (1 - Headroom) + PeakLimit * Headroom * LastIntervalPulse : structuralBounded;
            LastTarget = Bound(LastTarget, -PeakLimit, PeakLimit);
            float maxStep = SlewPerSecond * (float)dt;
            LastCommand = Bound(Bound(LastTarget, LastCommand - maxStep, LastCommand + maxStep), -PeakLimit, PeakLimit);
            int keep = 0;
            for (int i = 0; i < _count; i++) if (_events[i].Time + .100 > end) _events[keep++] = _events[i];
            _count = keep; _lastEnd = end; LastInputRejected = false;
            Status = impactInputsVerified ? "bounded" : "structural-only-unverified-impacts";
            return LastCommand;
        }
    }
}
