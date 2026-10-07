using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// Crash detection from a body contact, ported from art of rally's <c>CrashSignal</c> (STD-025): a
    /// contact with something other than the road whose normal is mostly horizontal, at more than
    /// <see cref="MinSpeed"/> m/s along the normal, gives an intensity rising to 1 at <see cref="FullSpeed"/>.
    /// </summary>
    /// <remarks>
    /// A game adapter supplies, per contact: the time, the relative velocity along the contact normal, how
    /// vertical the normal is, and whether the other collider is road. Braking and acceleration spikes
    /// alone never trigger it. One hit produces several manifold contacts: within <see cref="MergeSeconds"/>
    /// of a cue only a stronger contact, arriving within <see cref="ReplaceSeconds"/>, replaces it.
    /// </remarks>
    public sealed class CrashDetector
    {
        public const float MinSpeed = 3f, FullSpeed = 20f, MaxVerticalShare = .65f;
        public const double MergeSeconds = .35, ReplaceSeconds = .12, SettleSeconds = .1;

        private double _since = -1, _lastCrash = -1, _lastTime = -1;
        private float _lastIntensity;

        /// <summary>Normal speed of the last accepted contact, m/s.</summary>
        public float LastNormalSpeed { get; private set; }

        public void Reset() { _since = _lastCrash = _lastTime = -1; _lastIntensity = LastNormalSpeed = 0; }

        /// <summary>Call every physics tick while the player's car is live; contacts right after a (re)spawn are ignored.</summary>
        public void Track(double time)
        {
            if (!Finite(time) || time < 0 || (_lastTime >= 0 && (time < _lastTime || time - _lastTime > .25))) { Reset(); }
            if (!Finite(time) || time < 0) return;
            if (_since < 0) _since = time;
            _lastTime = time;
        }

        /// <param name="time">Physics time of the contact, seconds.</param>
        /// <param name="normalSpeed">Relative speed along the contact normal, m/s (sign ignored).</param>
        /// <param name="verticalShare">|normal.y| / |normal|: 1 for ground under the car, 0 for a wall.</param>
        /// <param name="road">The other collider is the driving surface.</param>
        /// <returns>Intensity 0..1 for a new or stronger cue; 0 when nothing should play.</returns>
        public float Observe(double time, float normalSpeed, float verticalShare, bool road)
        {
            if (road || _since < 0 || !Finite(time) || !Finite(normalSpeed) || !Finite(verticalShare) ||
                time < _lastTime || time - _since < SettleSeconds || Math.Abs(verticalShare) > MaxVerticalShare) return 0;
            float speed = Math.Abs(normalSpeed);
            if (speed <= MinSpeed) return 0;
            float intensity = Math.Min(1f, (speed - MinSpeed) / (FullSpeed - MinSpeed));
            if (_lastCrash >= 0 && time - _lastCrash < MergeSeconds)
            {
                if (intensity <= _lastIntensity || time - _lastCrash >= ReplaceSeconds) return 0;
            }
            else _lastCrash = time;
            _lastIntensity = intensity; LastNormalSpeed = speed;
            return intensity;
        }

        private static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
    }

    /// <summary>
    /// art of rally's crash cue, the shape the owner picked on the wheel (CrashFeel, 2026-10-05): a
    /// <see cref="PushMs"/> ms push, plus a <see cref="RattleHz"/> Hz rattle at <see cref="RattleShare"/> of its
    /// magnitude lasting <see cref="RattleMs"/> ms and fading out over the last <see cref="RattleFadeMs"/> ms.
    /// </summary>
    /// <remarks>
    /// Played as an additive term on the steering constant force, so it needs no native impact or periodic
    /// effect: <c>command = clamp(steering + Sample(now))</c>. The rattle is a cosine, so a 50 Hz physics
    /// update still renders 25 Hz as alternating full swings instead of sampling the zero crossings.
    /// The push direction is fixed (+1), as in art of rally; the mod's Invert setting applies on top.
    /// </remarks>
    public sealed class CrashCue
    {
        public const int PushMs = 120, RattleHz = 25, RattleMs = 250, RattleFadeMs = 150;
        public const float RattleShare = .5f;

        private double _start = -1;
        private float _magnitude;

        /// <summary>Cues accepted since construction.</summary>
        public int Played { get; private set; }

        /// <summary>Starts (or restarts, if stronger) a cue. Magnitude 0..1 of full force, e.g. intensity x CrashStrength / 100.</summary>
        public bool Trigger(float magnitude, double now)
        {
            if (!Finite(magnitude) || !Finite(now) || now < 0 || magnitude <= 0) return false;
            magnitude = Math.Min(1f, magnitude);
            if (Active(now) && magnitude <= _magnitude) return false;
            _start = now; _magnitude = magnitude; Played++;
            return true;
        }

        public bool Active(double now) => _start >= 0 && Finite(now) && now >= _start && now - _start < RattleMs / 1000.0;

        /// <summary>The cue's force at <paramref name="now"/>, -1..1 (0 when idle).</summary>
        public float Sample(double now)
        {
            if (!Active(now)) return 0;
            double ms = (now - _start) * 1000.0;
            double push = ms < PushMs ? 1 : 0;
            double fadeFrom = RattleMs - RattleFadeMs;
            double rattleGain = ms < fadeFrom ? 1 : Math.Max(0, (RattleMs - ms) / RattleFadeMs);
            double rattle = RattleShare * rattleGain * Math.Cos(2 * Math.PI * RattleHz * ms / 1000.0);
            return (float)Math.Max(-1, Math.Min(1, _magnitude * (push + rattle)));
        }

        public void Reset() { _start = -1; _magnitude = 0; }

        private static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
    }
}
