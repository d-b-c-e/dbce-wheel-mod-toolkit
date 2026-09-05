using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// Detects a recent arrival at high source-force magnitude. This is a waveform
    /// candidate, not proof of a collision. Feed every sample, including unchanged
    /// values and zero, BEFORE wheel gain. Counterpart: native RiseDetector.
    /// </summary>
    public sealed class ForceRiseDetector
    {
        public float Arrival = 0.80f, Rise = 0.40f;
        public double WindowSeconds = 0.25, CooldownSeconds = 0.25;
        public float LastArrival { get; private set; }
        public float LastRise { get; private set; }
        private struct Sample { public double Time; public float Value; }
        // Reuse storage on the physics path. Grow only if a higher sample rate
        // requires more history than the initial quarter-second at 1 kHz.
        private Sample[] _samples = new Sample[256];
        private int _head, _count;
        private double _lastTime = -1, _lastEvent = -1e30;

        public void Reset()
        {
            _head = _count = 0; _lastTime = -1; _lastEvent = -1e30;
            LastArrival = LastRise = 0;
        }

        public bool Observe(float normalised, double seconds)
        {
            if (float.IsNaN(normalised) || float.IsInfinity(normalised) ||
                double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0)
            { Reset(); return false; }
            if (seconds < _lastTime) Reset();
            if (seconds == _lastTime) return false;
            _lastTime = seconds;
            float level = Math.Min(1f, Math.Abs(normalised));
            while (_count > 0 && _samples[_head].Time < seconds - WindowSeconds)
            { _head = (_head + 1) % _samples.Length; _count--; }
            while (_count > 0 && _samples[(_head + _count - 1) % _samples.Length].Value >= level)
                _count--;
            if (_count == _samples.Length)
            {
                var grown = new Sample[_samples.Length * 2];
                for (int n = 0; n < _count; n++) grown[n] = _samples[(_head + n) % _samples.Length];
                _samples = grown; _head = 0;
            }
            _samples[(_head + _count++) % _samples.Length] = new Sample { Time = seconds, Value = level };
            LastArrival = level;
            LastRise = level - _samples[_head].Value;
            if (level >= Arrival && LastRise >= Rise && seconds - _lastEvent >= CooldownSeconds)
            { _lastEvent = seconds; return true; }
            return false;
        }
    }
}
