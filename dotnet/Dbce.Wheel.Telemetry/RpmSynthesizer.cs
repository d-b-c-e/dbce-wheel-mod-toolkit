using System;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>
    /// A plausible engine speed for games that have none, from road speed and a
    /// gear-ratio table. Three projects had each written their own version of
    /// this (OutRun 2006, Cruis'n, Gravel); this is the one to keep.
    /// </summary>
    /// <remarks>
    /// <para>
    /// The output exists to drive rev counters, shift lights and bass shakers,
    /// not to be true. It rises through each gear's speed band from idle to just
    /// under the redline, drops on the up-shift, and never exceeds
    /// <see cref="MaxRpm"/>. When the game reports no gear either, the gear is
    /// inferred from speed against the same table.
    /// </para>
    /// <para>
    /// Pass the frame's <c>EngineMaxRpm</c>/<c>EngineIdleRpm</c> the same values
    /// every frame — dashboards normalise against them, and the idle keeper must
    /// use the same constants (see <see cref="IdleKeeper"/>).
    /// </para>
    /// </remarks>
    public sealed class RpmSynthesizer
    {
        /// <summary>Speed (m/s) at which each gear reaches the redline; ascending.</summary>
        public float[] GearTopSpeedsMps { get; }
        public float MaxRpm { get; }
        public float IdleRpm { get; }
        /// <summary>Fraction of the redline reached at the top of each gear (leaves headroom for the limiter cue).</summary>
        public float RedlineFraction { get; }

        /// <param name="gearTopSpeedsMps">Speed at which each gear tops out, metres per second, ascending. At least one entry.</param>
        /// <param name="maxRpm">Redline reported to the dash as <c>EngineMaxRpm</c>.</param>
        /// <param name="idleRpm">Idle reported to the dash as <c>EngineIdleRpm</c>.</param>
        /// <param name="redlineFraction">Fraction of <paramref name="maxRpm"/> reached at the top of each gear.</param>
        public RpmSynthesizer(float[] gearTopSpeedsMps, float maxRpm = 7500f, float idleRpm = 900f, float redlineFraction = 0.95f)
        {
            if (gearTopSpeedsMps == null || gearTopSpeedsMps.Length == 0)
                throw new ArgumentException("At least one gear top speed is required.", nameof(gearTopSpeedsMps));
            for (int i = 1; i < gearTopSpeedsMps.Length; i++)
                if (gearTopSpeedsMps[i] <= gearTopSpeedsMps[i - 1])
                    throw new ArgumentException("Gear top speeds must be strictly ascending.", nameof(gearTopSpeedsMps));
            GearTopSpeedsMps = (float[])gearTopSpeedsMps.Clone();
            MaxRpm = maxRpm;
            IdleRpm = idleRpm;
            RedlineFraction = Math.Max(0.1f, Math.Min(1f, redlineFraction));
        }

        /// <summary>A table from a top speed and a gear count, with ratios spaced like a road car's.</summary>
        public static RpmSynthesizer FromTopSpeed(float topSpeedMps, int gears, float maxRpm = 7500f, float idleRpm = 900f)
        {
            if (gears < 1) throw new ArgumentOutOfRangeException(nameof(gears));
            // Geometric spacing: each gear tops out at a fixed multiple of the last,
            // which is how real gearboxes are spaced and reads right on a rev counter.
            var tops = new float[gears];
            float first = topSpeedMps / (float)Math.Pow(1.45, gears - 1);
            for (int i = 0; i < gears; i++) tops[i] = first * (float)Math.Pow(1.45, i);
            tops[gears - 1] = topSpeedMps;
            return new RpmSynthesizer(tops, maxRpm, idleRpm);
        }

        /// <summary>Gear (1-based) inferred from speed against the table.</summary>
        public int InferGear(float speedMps)
        {
            float s = Math.Abs(speedMps);
            for (int i = 0; i < GearTopSpeedsMps.Length; i++)
                if (s <= GearTopSpeedsMps[i]) return i + 1;
            return GearTopSpeedsMps.Length;
        }

        /// <summary>
        /// Engine speed for <paramref name="speedMps"/> in <paramref name="gear"/>
        /// (1-based; 0 or out of range infers the gear from speed).
        /// </summary>
        public float Rpm(float speedMps, int gear = 0)
        {
            float s = Math.Abs(speedMps);
            if (gear < 1 || gear > GearTopSpeedsMps.Length) gear = InferGear(s);
            float lo = gear > 1 ? GearTopSpeedsMps[gear - 2] : 0f;
            float hi = GearTopSpeedsMps[gear - 1];
            float frac = hi <= lo ? 1f : Math.Max(0f, Math.Min(1f, (s - lo) / (hi - lo)));
            float rpm = IdleRpm + (MaxRpm * RedlineFraction - IdleRpm) * frac;
            return Math.Min(MaxRpm, Math.Max(IdleRpm, rpm));
        }
    }
}
