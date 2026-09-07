using System;

namespace Dbce.Wheel.Ffb
{
    /// <summary>Version 1 axle-force compatibility pipeline: gain, fade, clamp, then EMA. Existing model profiles are unchanged.</summary>
    public static class AxleForceCurve
    {
        public const int CompatibilityVersion = 1;

        /// <summary>Below this speed, in km/h, there is no force at all.</summary>
        public const float FadeStartKmh = 3f;

        /// <summary>At and above this speed the force is at full strength.</summary>
        public const float FadeFullKmh = 12f;

        /// <summary>
        /// Pneumatic trail at twice the ideal slip angle, as a fraction of the
        /// straight-ahead trail. The shrinking trail is what makes the wheel
        /// lighten as the front starts to slide.
        /// </summary>
        public const float LimitTrail = 0.6f;

        /// <summary>Unity's <c>Mathf.Clamp01</c>.</summary>
        public static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);

        /// <summary>Unity's <c>Mathf.Lerp</c> - the interpolant is clamped.</summary>
        public static float Lerp(float a, float b, float t) => a + (b - a) * Clamp01(t);

        /// <summary>Unity's <c>Mathf.SmoothStep(0, 1, t)</c>: cubic, clamped.</summary>
        public static float SmoothStep01(float t)
        {
            t = Clamp01(t);
            return -2f * t * t * t + 3f * t * t;
        }

        /// <summary>
        /// Pneumatic trail as a fraction of its straight-ahead value: 1.0 at
        /// zero slip, falling linearly to <see cref="LimitTrail"/> at twice the
        /// ideal slip angle and held there. This is the term that makes the
        /// wheel lighten as the front starts to slide.
        /// </summary>
        public static float Trail(float absSlipDeg, float idealSlipDeg)
        {
            float ideal = Math.Max(1f, idealSlipDeg);
            return Lerp(1f, LimitTrail, Clamp01(absSlipDeg / (2f * ideal)));
        }

        /// <summary>
        /// The steering force, normalised to -1..1, before smoothing.
        /// </summary>
        /// <param name="frontLateralForce">
        /// Sum of the front axle's lateral forces, newtons (<c>lw.Fy + rw.Fy</c>).
        /// Not <c>Mz</c>: the game's aligning torque reverses sign past about 8
        /// degrees of slip and this game's tyres run 12-29 in ordinary corners.
        /// </param>
        /// <param name="absSlipDeg">Mean absolute front slip angle, degrees.</param>
        /// <param name="idealSlipDeg">Peak-grip slip angle, degrees.</param>
        /// <param name="speedKmh">Road speed, km/h, for the low-speed fade.</param>
        /// <param name="fyReference">Lateral force counting as full scale, newtons.</param>
        /// <param name="gain">Strength as a multiplier: <c>Strength / 50</c>.</param>
        /// <param name="invert">Flip the sign for devices reading the axis the other way.</param>
        public static float Normalised(float frontLateralForce, float absSlipDeg,
                                       float idealSlipDeg, float speedKmh,
                                       float fyReference, float gain, bool invert)
        {
            float trail = Trail(absSlipDeg, idealSlipDeg);

            float normalised = frontLateralForce * trail / Math.Max(1f, fyReference);
            normalised *= gain;

            // Aligning torque is meaningless at parking speed - the slip angle is
            // -atan(lateral / forward velocity) with a tiny denominator, so any
            // steering angle lands past the curve's peak and the wheel is pushed
            // into the turn on both sides of centre. Every sim fades it out.
            normalised *= SmoothStep01((speedKmh - FadeStartKmh) / (FadeFullKmh - FadeStartKmh));

            if (invert) normalised = -normalised;
            return normalised < -1f ? -1f : (normalised > 1f ? 1f : normalised);
        }

        /// <summary>
        /// First-order smoothing, applied to <see cref="Normalised"/>. Raw
        /// per-step force is noisy over kerbs and rocks and reads as rattle
        /// rather than detail.
        /// </summary>
        /// <remarks>
        /// <paramref name="smoothing"/> is the weight kept from the previous
        /// output, so 0 is unfiltered and 0.5 is an even blend. Symmetric: it
        /// does not distinguish a rising force from a falling one.
        /// </remarks>
        public static float Smooth(float previous, float target, float smoothing)
            => Lerp(target, previous, Clamp01(smoothing));
    }
}
