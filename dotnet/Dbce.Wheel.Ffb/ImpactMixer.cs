using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>Bounded constant-force mix with explicit signed 100 ms impact envelopes.
    /// Its budget excludes independently downloaded condition effects. No collision detection.</summary>
    public sealed class ImpactMixer
    {
        public float Headroom = .25f;
        public ulong RejectedEvents { get; private set; }
        private struct Event { public double Time; public float Amplitude; }
        private readonly Event[] _events = new Event[8];
        private int _count;
        private double _lastTime = -1;
        private static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
        private static float Bound(float v, float lo, float hi) => Math.Max(lo, Math.Min(v, hi));
        public void Reset() { _count = 0; _lastTime = -1; }
        public bool Trigger(float amplitude, float direction, double seconds)
        {
            if (!Finite(amplitude) || !Finite(direction) || !Finite(seconds) || seconds < 0 || seconds < _lastTime)
            { RejectedEvents++; return false; }
            Expire(seconds);
            if (_count == _events.Length) { RejectedEvents++; return false; }
            _lastTime = seconds;
            _events[_count++] = new Event { Time = seconds, Amplitude = Bound(amplitude, 0, 1) * (direction < 0 ? -1 : 1) };
            return true;
        }
        public static float Envelope(double age)
        {
            if (age < 0 || age >= .100) return 0;
            if (age < .005) return (float)(age / .005);
            if (age < .020) return (float)((.020 - age) / .015);
            if (age < .060) return (float)(-.25 * (age - .020) / .040);
            return (float)(-.25 * (.100 - age) / .040);
        }
        public float Mix(float structural, double seconds, float strength = 1)
        {
            if (!Finite(seconds) || seconds < 0 || seconds < _lastTime || !Finite(structural) || !Finite(strength) || !Finite(Headroom))
            { Reset(); return 0; }
            _lastTime = seconds;
            Expire(seconds);
            float pulse = 0;
            for (int i = 0; i < _count; i++) pulse += _events[i].Amplitude * Envelope(seconds - _events[i].Time);
            float reserve = Bound(Headroom, 0, .5f);
            return Bound(strength, 0, 1) * (Bound(structural, -1, 1) * (1 - reserve) + Bound(pulse, -1, 1) * reserve);
        }
        private void Expire(double seconds)
        {
            int keep = 0;
            for (int i = 0; i < _count; i++) if (seconds - _events[i].Time < .100) _events[keep++] = _events[i];
            _count = keep;
        }
    }
}
