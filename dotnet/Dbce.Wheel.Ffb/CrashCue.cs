using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>One physics tick of the player's car: time, position and velocity (any consistent units, metres).</summary>
    public struct MotionSample
    {
        public double Time;
        public float X, Y, Z, Vx, Vy, Vz;
    }

    /// <summary>
    /// Crash detection from a body contact with art of rally's <c>CrashSignal</c> rules (STD-025): the car
    /// must be tracked continuously (no duplicate tick, no gap over <see cref="MaxGapSeconds"/>, no position
    /// jump over 5 m from the last velocity), the contact must be no older than <see cref="MaxContactAgeSeconds"/>
    /// and at least <see cref="SettleSeconds"/> into the tracked epoch, the other collider must not be road,
    /// and the normal mostly horizontal. Intensity rises from 0 at <see cref="MinSpeed"/> m/s along the normal
    /// to 1 at <see cref="FullSpeed"/>. One hit's manifold contacts merge for <see cref="MergeSeconds"/> unless a
    /// stronger one arrives within <see cref="ReplaceSeconds"/>.
    /// </summary>
    /// <remarks>
    /// Revised 2026-10-06 after Codex's review (knowledge/CRASH-PORT-REVIEW-2026-10-06.md): the first port
    /// omitted motion continuity and contact age and produced full cues where art of rally gives none.
    /// When <see cref="Track"/> breaks the epoch it sets <see cref="Discontinuous"/>; the caller must clear any
    /// playing cue then.
    /// </remarks>
    public sealed class CrashDetector
    {
        public const float MinSpeed = 3f, FullSpeed = 20f, MaxVerticalShare = .65f, MaxResidualMetres = 5f;
        public const double MergeSeconds = .35, ReplaceSeconds = .12, SettleSeconds = .1, MaxGapSeconds = .1, MaxContactAgeSeconds = .05;

        private MotionSample _last;
        private bool _hasLast;
        private double _since = -1, _lastCrash = -1;
        private float _lastIntensity;

        /// <summary>Normal speed of the last accepted contact, m/s.</summary>
        public float LastNormalSpeed { get; private set; }
        /// <summary>The last <see cref="Track"/> (or <see cref="Reset"/>) started a new epoch: clear any playing cue.</summary>
        public bool Discontinuous { get; private set; } = true;
        /// <summary>Epochs started so far, for recordings.</summary>
        public int Epoch { get; private set; }

        public void Reset()
        {
            _hasLast = false; _since = _lastCrash = -1;
            _lastIntensity = LastNormalSpeed = 0; Discontinuous = true; Epoch++;
        }

        /// <summary>Every physics tick while the player's car is live.</summary>
        public void Track(MotionSample sample)
        {
            Discontinuous = false;
            if (!Finite(sample.Time) || sample.Time < 0 || !Finite(sample.X) || !Finite(sample.Y) || !Finite(sample.Z) ||
                !Finite(sample.Vx) || !Finite(sample.Vy) || !Finite(sample.Vz)) { Reset(); return; }
            if (_hasLast)
            {
                double dt = sample.Time - _last.Time;
                double x = sample.X - _last.X - _last.Vx * dt, y = sample.Y - _last.Y - _last.Vy * dt, z = sample.Z - _last.Z - _last.Vz * dt;
                if (dt <= 0 || dt > MaxGapSeconds || x * x + y * y + z * z > MaxResidualMetres * MaxResidualMetres) Reset();
            }
            if (!_hasLast) _since = sample.Time;
            _last = sample; _hasLast = true;
        }

        /// <param name="time">Physics time of the contact, seconds.</param>
        /// <param name="normalSpeed">Relative speed along the contact normal, m/s (sign ignored).</param>
        /// <param name="verticalShare">|normal.y| / |normal|: 1 for ground under the car, 0 for a wall.</param>
        /// <param name="road">The other collider is the driving surface.</param>
        /// <returns>Intensity 0..1 for a new or stronger cue; 0 when nothing should play.</returns>
        public float Observe(double time, float normalSpeed, float verticalShare, bool road)
        {
            if (!_hasLast || road || !Finite(time) || !Finite(normalSpeed) || !Finite(verticalShare) ||
                time < _last.Time || time - _last.Time > MaxContactAgeSeconds || time - _since < SettleSeconds ||
                Math.Abs(verticalShare) > MaxVerticalShare) return 0;
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

    /// <summary>The two independent effects of art of rally's crash cue, for a native sink that can play them.</summary>
    public struct CrashEffects
    {
        public float PushMagnitude; public int PushMs;
        public float RattleMagnitude; public int RattleHz, RattleMs, RattleFadeMs;
    }

    /// <summary>
    /// art of rally's crash cue, the shape the owner picked on the wheel (CrashFeel, 2026-10-05): an independent
    /// <see cref="PushMs"/> ms constant push of magnitude M, and an independent <see cref="RattleHz"/> Hz sine of
    /// amplitude <see cref="RattleShare"/> x M starting at phase zero, lasting <see cref="RattleMs"/> ms and fading
    /// linearly over its last <see cref="RattleFadeMs"/> ms. For M = 0.5 that is art of rally's +0.50 push and 0.25 rattle.
    /// </summary>
    /// <remarks>
    /// Art-equivalent delivery is the two native finite effects described by <see cref="Requests"/>, scheduled by
    /// the wheel library. <see cref="Sample"/> is a labelled fallback (<see cref="FallbackModel"/>) for native
    /// libraries without those effects: the push held for 120 ms plus the rattle averaged over each update
    /// interval, so a slow update rate loses the rattle instead of aliasing it into a constant push. Adding it to
    /// the steering command clips differently for each steering sign; the fallback must be qualified separately,
    /// never called art-equivalent.
    /// </remarks>
    public sealed class CrashCue
    {
        public const int PushMs = 120, RattleHz = 25, RattleMs = 250, RattleFadeMs = 150;
        public const float RattleShare = .5f;
        /// <summary>Model label for recordings when the cue is rendered through the constant-force fallback.</summary>
        public const string FallbackModel = "crash-constant-fallback@2";

        private double _start = -1, _lastSample = -1;
        private float _magnitude;

        /// <summary>Cues accepted since construction.</summary>
        public int Played { get; private set; }
        /// <summary>Magnitude 0..1 of the current or last cue.</summary>
        public float Magnitude => _magnitude;

        /// <summary>The independent effect requests for magnitude 0..1 (e.g. intensity x CrashStrength / 100).</summary>
        public static CrashEffects Requests(float magnitude)
        {
            float m = Finite(magnitude) ? Math.Max(0f, Math.Min(1f, magnitude)) : 0f;
            return new CrashEffects { PushMagnitude = m, PushMs = PushMs, RattleMagnitude = RattleShare * m, RattleHz = RattleHz, RattleMs = RattleMs, RattleFadeMs = RattleFadeMs };
        }

        /// <summary>Starts (or restarts, if stronger) a cue at physics time <paramref name="now"/>.</summary>
        public bool Trigger(float magnitude, double now)
        {
            if (!Finite(magnitude) || !Finite(now) || now < 0 || magnitude <= 0) return false;
            magnitude = Math.Min(1f, magnitude);
            if (Active(now) && magnitude <= _magnitude) return false;
            _start = now; _magnitude = magnitude; Played++;
            return true;
        }

        public bool Active(double now) => _start >= 0 && Finite(now) && now >= _start && now - _start < RattleMs / 1000.0;

        /// <summary>
        /// Fallback rendering at one update, -1..1: the push plus the rattle averaged since the previous call
        /// (instantaneous at the first update of a cue, where the sine is zero). Call it once per update.
        /// </summary>
        public float Sample(double now)
        {
            double previous = _lastSample; _lastSample = now;
            if (!Active(now)) return 0;
            double b = now - _start;
            double a = previous >= _start && previous < now ? previous - _start : b;
            double push = b < PushMs / 1000.0 ? 1 : 0;
            double w = 2 * Math.PI * RattleHz;
            double sine = b - a < 1e-6 ? Math.Sin(w * b) : (Math.Cos(w * a) - Math.Cos(w * b)) / (w * (b - a));
            double mid = (a + b) * 500.0;   // ms at the interval midpoint
            double fadeFrom = RattleMs - RattleFadeMs;
            double gain = mid < fadeFrom ? 1 : Math.Max(0, (RattleMs - mid) / RattleFadeMs);
            return (float)Math.Max(-1, Math.Min(1, _magnitude * (push + RattleShare * gain * sine)));
        }

        public void Reset() { _start = -1; _magnitude = 0; }

        private static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
    }
}
