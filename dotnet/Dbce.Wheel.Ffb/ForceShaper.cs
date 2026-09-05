using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// The output conditioning chain every wheel mod ends up writing: deadzone,
    /// dual-rate smoothing, gain, soft saturation, slew limiting with event
    /// bypass, low-speed fade, warm-up ramp and inversion. Pure math, no device.
    /// </summary>
    /// <remarks>
    /// Order matters and is fixed (see knowledge/FFB-SIGNAL-DESIGN.md):
    /// deadzone → EMA → <b>gain/invert</b> → soft saturation → slew → fade →
    /// ramp → clamp. Gain and invert are the boundary: above them the value is
    /// in model units, below them in true output units and true output sign, and
    /// every stage below is a device-side limit. They moved there in 0.8.0.
    /// Applied last, gain re-inflated an already-saturated value into the hard
    /// clamp - above about Strength 65 the slew limiter then latched the output
    /// at full scale in whichever direction it first clipped, and the model could
    /// not recover it. Invert applied last gave the slew limiter a <c>_last</c> of
    /// the opposite sign, so with <c>Invert</c> on a steady input oscillated near
    /// zero and no force ever arrived.
    /// Structural force goes through all of it; an <c>isEvent</c> update
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

        // ---- from Cruis'n (mvffb), which had these and this repo did not -----
        // All default to off, so every existing profile is untouched.

        /// <summary>
        /// Low-pass time constant in MILLISECONDS. When &gt; 0 it replaces
        /// <see cref="AttackSmoothing"/>/<see cref="DecaySmoothing"/> entirely.
        /// </summary>
        /// <remarks>
        /// This is the frame-rate independent filter: <c>a = 1 - exp(-dt/tau)</c>,
        /// so 50 ms means 50 ms whether the caller ticks at 60 Hz or 250 Hz. The
        /// dimensionless coefficients do not - the same number smooths harder the
        /// faster you call it, which makes a tune that was arrived at by driving
        /// only valid at the rate it was tuned at. Prefer this one.
        /// </remarks>
        public float SmoothingMs = 0f;

        /// <summary>
        /// A jump of at least this much between consecutive inputs bypasses the
        /// smoothing, so a crash arrives as a pulse rather than a shove. 0 = only
        /// an explicit <c>isEvent</c> bypasses.
        /// </summary>
        /// <remarks>
        /// Needed when the source has no event flag to give you. An arcade board
        /// hands over a finished motor byte and never says "this one was a wall".
        /// </remarks>
        public float ImpulseBypass = 0f;

        /// <summary>
        /// When the input is zero and the smoothed output is below this, snap to
        /// zero so the effect can actually stop. 0 = off.
        /// </summary>
        /// <remarks>
        /// Conditional on the input being zero, which is what makes it different
        /// from <see cref="OutputDeadband"/>: it kills a dying tail without also
        /// killing a small force the game genuinely asked for.
        /// </remarks>
        public float SettleBelow = 0f;

        /// <summary>
        /// Report a new value only once it has moved at least this much, or
        /// crossed zero. Stops sub-threshold chatter reaching the device. 0 = off.
        /// </summary>
        public float RelevelHysteresis = 0f;

        public float GainFromStrength => Math.Max(0, Strength) / 50f;

        private float _smoothed, _last, _rampT, _prevIn;
        private bool _havePrevIn;
        private bool _started;

        /// <summary>Clears filter state (between stages) so a stale force is not carried over.</summary>
        public void Reset() { _smoothed = 0f; _last = 0f; _rampT = 0f; _started = false; _prevIn = 0f; _havePrevIn = false; }

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

            // 1b. automatic impulse detection. A big enough jump IS an event even
            //     when the caller had no way to tell us so.
            if (ImpulseBypass > 0f && _havePrevIn && Math.Abs(v - _prevIn) >= ImpulseBypass)
                isEvent = true;
            _prevIn = v; _havePrevIn = true;
            float target = v;

            // 2. smoothing (structural only). SmoothingMs, when set, is a
            //    frame-rate independent time constant and wins over the pair of
            //    dimensionless coefficients.
            if (!isEvent)
            {
                if (SmoothingMs > 0f)
                {
                    float a = 1f - (float)Math.Exp(-(dt * 1000f) / SmoothingMs);
                    _smoothed = _smoothed + (v - _smoothed) * a;
                }
                else
                {
                    float a = Math.Abs(v) > Math.Abs(_smoothed) ? AttackSmoothing : DecaySmoothing;
                    a = Math.Max(0f, Math.Min(0.95f, a));
                    _smoothed = _smoothed + (v - _smoothed) * (1f - a);
                }
                v = _smoothed;
            }
            else _smoothed = v;

            // 3. gain. This is the line between the two halves of the chain:
            //    above it the value is in model units, below it in output units,
            //    and everything below is a device-side limit. Gain used to be
            //    applied last, which let Strength re-inflate an already-saturated
            //    value into the hard clamp - at Strength 75 a crash and a hard
            //    corner both arrived at exactly 1.0. Driving INTO the knee is
            //    what a drive control is supposed to do.
            //
            //    Invert moves with it, and for a sharper reason: the slew limiter
            //    below compares against _last, which is the value actually sent.
            //    With invert applied at the end those two had OPPOSITE SIGNS, so
            //    a steady input oscillated between -0.06 and 0 forever and the
            //    force never arrived. Both stages now work in true output units
            //    and true output sign, which is what makes _last comparable.
            v *= GainFromStrength;
            if (Invert) v = -v;

            // 4. soft saturation
            if (SoftSaturation > 0f)
            {
                float k = SoftSaturation;
                v = (float)((1 - k) * v + k * Math.Tanh(v));
            }

            // 5. slew (structural only). Now in output units, so a rate limit set
            //    to protect the wheel is not quietly scaled by Strength.
            if (!isEvent && SlewPerSecond > 0f && dt > 0f)
            {
                float maxStep = SlewPerSecond * dt;
                v = Math.Max(_last - maxStep, Math.Min(_last + maxStep, v));
            }

            // 6. low-speed fade
            if (FadeFullKmh > FadeStartKmh)
            {
                float t = (speedKmh - FadeStartKmh) / (FadeFullKmh - FadeStartKmh);
                t = Math.Max(0f, Math.Min(1f, t));
                v *= t * t * (3f - 2f * t);   // smoothstep
            }

            // 7. warm-up / restart ramp
            if (RampSeconds > 0f && _rampT < RampSeconds)
            {
                _rampT += dt; _started = true;
                v *= Math.Min(1f, _rampT / RampSeconds);
            }

            // 8. clamp, peak limit, deadband
            float limit = Math.Max(0f, Math.Min(1f, PeakLimit));
            v = Math.Max(-limit, Math.Min(limit, v));
            if (Math.Abs(v) < OutputDeadband) v = 0f;

            // 9. settle: the game asked for nothing and what is left is a dying
            //    tail, so stop rather than trail. Conditional on the target being
            //    zero, unlike OutputDeadband.
            if (SettleBelow > 0f && target == 0f && Math.Abs(v) < SettleBelow)
            {
                _smoothed = 0f;
                v = 0f;
            }

            // 10. re-level hysteresis: hold the previous value until this one has
            //     moved far enough to be worth sending. Zero crossings always pass.
            if (RelevelHysteresis > 0f && (v != 0f) == (_last != 0f) &&
                Math.Abs(v - _last) < RelevelHysteresis)
                v = _last;

            _last = v;
            return v;
        }

        /// <summary>Last shaped value, for panels and traces.</summary>
        public float Last => _last;
        public bool Started => _started;
    }
}
