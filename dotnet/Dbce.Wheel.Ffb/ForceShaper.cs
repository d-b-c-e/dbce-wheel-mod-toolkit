using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// The output conditioning chain every wheel mod ends up writing: deadzone,
    /// dual-rate smoothing, soft saturation, slew limiting with event bypass,
    /// low-speed fade, warm-up ramp, gain and inversion. Pure math, no device.
    /// </summary>
    /// <remarks>
    /// Order matters and is fixed (see knowledge/FFB-SIGNAL-DESIGN.md):
    /// deadzone → EMA → soft saturation → slew → fade → ramp → gain/invert →
    /// clamp. Structural force goes through all of it; an <c>isEvent</c> update
    /// (crash impulse, shift pulse) bypasses the EMA and slew so it arrives as
    /// a pulse rather than a shove. Rules lifted from OutRun2006Tweaks-FFB
    /// (chain), art-of-sim-rally (fade, Strength as the only dial) and
    /// cruisn-collection (slew and clamp as the instability ladder).
    /// </remarks>
    public sealed class ForceShaper
    {
        /// <summary>Strength 0–100; 50 is unity gain, so the slider's midpoint is the tuned default.</summary>
        public int Strength = 50;
        public bool Invert;
        /// <summary>Subtractive deadzone in normalised units (0.0015 ≈ 15 of 10000).</summary>
        public float Deadzone = 0.0015f;
        /// <summary>EMA coefficient while |force| is rising (0 = no smoothing, higher = more).</summary>
        public float AttackSmoothing = 0.2f;
        /// <summary>EMA coefficient while |force| is falling.</summary>
        public float DecaySmoothing = 0.2f;
        /// <summary>Soft saturation: 0 = hard clamp only; 1 = tanh with full-scale knee.</summary>
        public float SoftSaturation = 0f;
        /// <summary>Maximum change per second in normalised units; 0 disables. 6 = full scale in 1/6 s.</summary>
        public float SlewPerSecond = 0f;
        /// <summary>Output deadband: values below this after shaping become 0 (stops a base buzzing at rest).</summary>
        public float OutputDeadband = 0.0015f;
        /// <summary>Speed below which force is zero and above which it is full; slip angles are meaningless at parking speed.</summary>
        public float FadeStartKmh = 3f, FadeFullKmh = 12f;
        /// <summary>Seconds over which output ramps from zero at start and after <see cref="RestartRamp"/>.</summary>
        public float RampSeconds = 0.25f;
        /// <summary>Hard cap on |output| after everything else (peak limit; 1 = none).</summary>
        public float PeakLimit = 1f;

        public float GainFromStrength => Math.Max(0, Strength) / 50f;

        private float _smoothed, _last, _rampT;
        private bool _started;

        /// <summary>Clears filter state (between stages) so a stale force is not carried over.</summary>
        public void Reset() { _smoothed = 0f; _last = 0f; _rampT = 0f; _started = false; }

        /// <summary>Starts the ramp-in again, e.g. after the native layer recreated the effect.</summary>
        public void RestartRamp() { _rampT = 0f; _started = false; }

        /// <summary>
        /// Shapes one normalised input (-1..1, before gain) into the value to send.
        /// </summary>
        /// <param name="normalised">Model output, -1..1 at full scale before Strength.</param>
        /// <param name="speedKmh">Vehicle speed for the low-speed fade.</param>
        /// <param name="dt">Seconds since the last call.</param>
        /// <param name="isEvent">True for an impulse that must bypass smoothing and slew.</param>
        public float Shape(float normalised, float speedKmh, float dt, bool isEvent = false)
        {
            if (float.IsNaN(normalised) || float.IsInfinity(normalised)) normalised = 0f;
            dt = Math.Max(0f, Math.Min(0.1f, dt));
            float v = normalised;

            // 1. subtractive deadzone
            if (Math.Abs(v) <= Deadzone) v = 0f; else v -= Math.Sign(v) * Deadzone;

            // 2. dual-rate EMA (structural only)
            if (!isEvent)
            {
                float a = Math.Abs(v) > Math.Abs(_smoothed) ? AttackSmoothing : DecaySmoothing;
                a = Math.Max(0f, Math.Min(0.95f, a));
                _smoothed = _smoothed + (v - _smoothed) * (1f - a);
                v = _smoothed;
            }
            else _smoothed = v;

            // 3. soft saturation
            if (SoftSaturation > 0f)
            {
                float k = SoftSaturation;
                v = (float)((1 - k) * v + k * Math.Tanh(v));
            }

            // 4. slew (structural only)
            if (!isEvent && SlewPerSecond > 0f && dt > 0f)
            {
                float maxStep = SlewPerSecond * dt;
                v = Math.Max(_last - maxStep, Math.Min(_last + maxStep, v));
            }

            // 5. low-speed fade
            if (FadeFullKmh > FadeStartKmh)
            {
                float t = (speedKmh - FadeStartKmh) / (FadeFullKmh - FadeStartKmh);
                t = Math.Max(0f, Math.Min(1f, t));
                v *= t * t * (3f - 2f * t);   // smoothstep
            }

            // 6. warm-up / restart ramp
            if (RampSeconds > 0f && _rampT < RampSeconds)
            {
                _rampT += dt; _started = true;
                v *= Math.Min(1f, _rampT / RampSeconds);
            }

            // 7. gain, invert, clamp, peak limit, deadband
            v *= GainFromStrength;
            if (Invert) v = -v;
            float limit = Math.Max(0f, Math.Min(1f, PeakLimit));
            v = Math.Max(-limit, Math.Min(limit, v));
            if (Math.Abs(v) < OutputDeadband) v = 0f;

            _last = v;
            return v;
        }

        /// <summary>Last shaped value, for panels and traces.</summary>
        public float Last => _last;
        public bool Started => _started;
    }
}
