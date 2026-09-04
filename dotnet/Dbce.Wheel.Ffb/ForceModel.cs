using System;
using System.Text;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// What a game's sampler can supply to the force model each tick. Fields the
    /// game does not have are left at their defaults and the matching
    /// <c>Has*</c> flag false; the model uses what is available.
    /// </summary>
    public struct ForceInputs
    {
        /// <summary>Steering position, -1..1 (left negative).</summary>
        public float Steer;
        /// <summary>Steering rate, units of full travel per second.</summary>
        public float SteerRate;
        public bool HasSteer, HasSteerRate;

        /// <summary>Vehicle speed, metres per second (absolute).</summary>
        public float SpeedMps;

        /// <summary>Sum of lateral tyre force on the steered axle, newtons (sign: positive pushes the wheel toward +Steer).</summary>
        public float FrontLateralForce;
        public bool HasFrontLateralForce;
        /// <summary>Lateral acceleration in g, when tyre forces are not available.</summary>
        public float LateralG;
        public bool HasLateralG;
        /// <summary>Longitudinal acceleration in g (positive accelerating).</summary>
        public float LongitudinalG;
        public bool HasLongitudinalG;

        /// <summary>Mean absolute slip angle of the steered axle, degrees, and the tyre model's peak-grip slip angle.</summary>
        public float FrontSlipAngleDeg, IdealSlipAngleDeg;
        public bool HasSlip;
        /// <summary>0..1 how far into a drift the car is, if the game knows.</summary>
        public float DriftAmount;
        public bool HasDrift;

        /// <summary>0..1 surface texture level for this tick (rumble strip, gravel).</summary>
        public float Texture;
        /// <summary>Set for one tick on collision; magnitude 0..1.</summary>
        public float Impact;
        /// <summary>Signed direction hint for the impact (-1 left, +1 right, 0 unknown).</summary>
        public float ImpactDirection;
        /// <summary>Set for one tick on a gear change.</summary>
        public bool GearShift;
    }

    /// <summary>Tuning for <see cref="ForceModel"/>; the vocabulary shared by every mod.</summary>
    public sealed class ForceModelSettings
    {
        /// <summary>Virtual centring spring: force per unit steer at speed.</summary>
        public float SpringStrength = 0f;
        /// <summary>Damping: force per unit steer rate.</summary>
        public float DamperStrength = 0f;
        /// <summary>Lateral load term: 1.0 = reference lateral force gives full output.</summary>
        public float SteeringWeight = 1f;
        /// <summary>Reference lateral force in newtons treated as full scale (per steered axle).</summary>
        public float LateralForceReference = 11500f;
        /// <summary>Reference lateral acceleration in g treated as full scale when only g is available.</summary>
        public float LateralGReference = 1.2f;
        /// <summary>Fraction of lateral force retained at twice the ideal slip angle (trail floor). 1 = no lightening.</summary>
        public float TrailFloor = 0.6f;
        /// <summary>Extra lightening from an explicit drift signal, 0..1 of the lateral term.</summary>
        public float GripLoss = 0f;
        /// <summary>Weight transfer: multiplies the structural force by 1 + clamp(-longG × this, -0.2, +0.3).</summary>
        public float WeightTransfer = 0f;
        /// <summary>Speed (m/s) at which the spring reaches full strength.</summary>
        public float SpringFullSpeedMps = 30f;
        /// <summary>Texture amplitude at full texture level.</summary>
        public float TextureStrength = 0f;
        /// <summary>Impulse amplitude for an impact of 1.0.</summary>
        public float ImpactStrength = 0f;
        /// <summary>Amplitude of the symmetric gear-shift double pulse.</summary>
        public float ShiftPulseStrength = 0f;

        // ---- shape of each term -------------------------------------------------
        // These were constants inside Compute. They change how the model FEELS, so
        // they belong in the profile like everything else; the defaults are the
        // values that were hardcoded, so an unset profile behaves as before.

        /// <summary>Damper authority at a standstill, as a fraction of its authority at speed. 1 = speed-independent.</summary>
        public float DamperStaticFraction = 0.4f;
        /// <summary>Multiple of the ideal slip angle at which the trail reaches <see cref="TrailFloor"/>.</summary>
        public float TrailSlipSpan = 2f;
        /// <summary>Bounds on the weight-transfer multiplier: how much braking may add and throttle may shed.</summary>
        public float WeightTransferMin = -0.20f, WeightTransferMax = 0.30f;
        /// <summary>Texture oscillator frequency in Hz. Above ~15 the zero-order hold at 60 Hz eats it.</summary>
        public float TextureHz = 12f;
        /// <summary>Duration of the decaying impact impulse, seconds.</summary>
        public float ImpactSeconds = 0.12f;
        /// <summary>Duration of the positive half of the gear-shift pulse, seconds.</summary>
        public float ShiftPulseSeconds = 0.04f;

        /// <summary>
        /// The sim-like preset: lateral tyre force through a pneumatic trail that
        /// shrinks toward the limit; no virtual spring. art-of-sim-rally's shipped
        /// model (2026-09-02), where the game exposes per-wheel lateral force.
        /// </summary>
        /// <remarks>
        /// Equivalent to the profile <c>simlite@1</c>. Prefer loading a named,
        /// versioned profile (<see cref="ForceProfile"/>): a profile can be swapped
        /// or A/B tested without rebuilding the game, and it pins what was tuned.
        /// </remarks>
        public static ForceModelSettings SimLite() => new ForceModelSettings
        {
            SpringStrength = 0f, DamperStrength = 0f, SteeringWeight = 1f,
            LateralForceReference = 11500f, TrailFloor = 0.6f,
        };

        /// <summary>
        /// The arcade preset: OutRun2006Tweaks-FFB's "center-out" model — spring +
        /// damper + lateral load lightening in a drift, weight transfer, events.
        /// Starting values, not truth; every game retunes.
        /// </summary>
        /// <remarks>
        /// This is <b>not</b> OutRun's shipped tuning; several constants differ.
        /// <c>arcade-outrun@1</c> in <c>profiles/force-profiles.ini</c> is the one
        /// that matches the game, and is what OutRun2006Tweaks-FFB loads.
        /// </remarks>
        public static ForceModelSettings Arcade() => new ForceModelSettings
        {
            SpringStrength = 0.45f, DamperStrength = 0.10f, SteeringWeight = 0.9f,
            LateralGReference = 1.0f, TrailFloor = 1f, GripLoss = 0.5f, WeightTransfer = 0.35f,
            SpringFullSpeedMps = 40f, TextureStrength = 0.15f, ImpactStrength = 0.8f, ShiftPulseStrength = 0.25f,
        };
    }

    /// <summary>
    /// Composable force model over <see cref="ForceInputs"/>: the structure that
    /// carried over between four games, with the constants that did not left as
    /// settings. Output is normalised -1..1 before <see cref="ForceShaper"/>.
    /// </summary>
    /// <remarks>
    /// <para>
    /// Sign convention: positive output pushes the wheel toward positive steer
    /// (the direction the car is turning) — i.e. it is <b>anti</b>-centring — and
    /// the model produces negative output for a positive steer input, which is
    /// centring. Which DirectInput sign that maps to is a per-wheel fact handled
    /// by <see cref="ForceShaper.Invert"/>; measure it.
    /// </para>
    /// <para>
    /// The <see cref="Validator"/> watches the inputs, because the source signal
    /// decides more than the model: art of rally's aligning torque reversed past
    /// the slip peak, and OutRun's steer input sat at zero through corners while
    /// the model's spring term silently produced nothing.
    /// </para>
    /// </remarks>
    public sealed class ForceModel
    {
        public ForceModelSettings Settings { get; }
        public InputValidator Validator { get; } = new InputValidator();

        private float _shiftPulseT = -1f, _shiftEmitted;
        private float _impactT = -1f, _impactSign, _impactMag;
        private float _texturePhase;

        public ForceModel(ForceModelSettings settings) { Settings = settings ?? new ForceModelSettings(); }

        /// <summary>True when the last <see cref="Compute"/> carried an impulse that should bypass smoothing.</summary>
        public bool LastWasEvent { get; private set; }
        /// <summary>Last structural (non-event) force, for traces.</summary>
        public float LastStructural { get; private set; }

        public void Reset() { _shiftPulseT = -1f; _impactT = -1f; _texturePhase = 0f; Validator.Reset(); }

        /// <summary>One tick. <paramref name="dt"/> in seconds.</summary>
        public float Compute(in ForceInputs i, float dt)
        {
            var s = Settings;
            dt = Math.Max(0f, Math.Min(0.1f, dt));
            float speedNorm = Math.Min(1f, Math.Abs(i.SpeedMps) / Math.Max(0.1f, s.SpringFullSpeedMps));

            // --- lateral load: tyre force if we have it, lateral g otherwise ---------
            float lateral = 0f;
            if (i.HasFrontLateralForce)
                lateral = i.FrontLateralForce / Math.Max(1f, s.LateralForceReference);
            else if (i.HasLateralG)
                lateral = i.LateralG / Math.Max(0.05f, s.LateralGReference);
            // Trail: lightens toward the limit instead of reversing like a Pacejka Mz.
            if (i.HasSlip && s.TrailFloor < 1f)
            {
                float ideal = Math.Max(0.5f, i.IdealSlipAngleDeg);
                float t = Math.Min(1f, Math.Abs(i.FrontSlipAngleDeg) / Math.Max(0.1f, s.TrailSlipSpan * ideal));
                lateral *= 1f - (1f - s.TrailFloor) * t;
            }
            if (i.HasDrift && s.GripLoss > 0f) lateral *= 1f - s.GripLoss * Math.Max(0f, Math.Min(1f, i.DriftAmount));
            // The road pushes back against the direction the tyre is loaded.
            float fLat = -lateral * s.SteeringWeight;

            // --- spring and damper (arcade) ----------------------------------------
            float fSpring = i.HasSteer ? -i.Steer * s.SpringStrength * speedNorm : 0f;
            float k = Math.Max(0f, Math.Min(1f, s.DamperStaticFraction));
            float fDamper = i.HasSteerRate ? -i.SteerRate * s.DamperStrength * (k + (1f - k) * speedNorm) : 0f;

            float loadMod = 1f;
            if (i.HasLongitudinalG && s.WeightTransfer != 0f)
                loadMod = 1f + Math.Max(s.WeightTransferMin, Math.Min(s.WeightTransferMax, -i.LongitudinalG * s.WeightTransfer));

            float structural = (fSpring + fLat) * loadMod + fDamper;
            LastStructural = structural;

            // --- texture: low-frequency modulation, capped where zero-order hold kills it ---
            float texture = 0f;
            if (s.TextureStrength > 0f && i.Texture > 0f && dt > 0f)
            {
                _texturePhase += dt * Math.Max(0.1f, s.TextureHz) * (float)Math.PI * 2f;
                texture = (float)Math.Sin(_texturePhase) * s.TextureStrength * Math.Min(1f, i.Texture);
            }

            // --- events -------------------------------------------------------------
            bool isEvent = false;
            float ev = 0f;
            if (i.Impact > 0f) { _impactT = 0f; _impactMag = Math.Min(1f, i.Impact); _impactSign = i.ImpactDirection != 0f ? Math.Sign(i.ImpactDirection) : (i.HasSteer ? -Math.Sign(i.Steer) : 1f); }
            if (_impactT >= 0f)
            {
                float span = Math.Max(0.001f, s.ImpactSeconds);
                float decay = Math.Max(0f, 1f - _impactT / span);
                ev += _impactSign * _impactMag * s.ImpactStrength * decay;
                _impactT += dt; if (_impactT > span) _impactT = -1f;
                isEvent = true;
            }
            if (i.GearShift && s.ShiftPulseStrength > 0f) { _shiftPulseT = 0f; _shiftEmitted = 0f; }
            if (_shiftPulseT >= 0f)
            {
                // Symmetric double pulse: +K for ~40 ms, then exactly what was emitted
                // comes back negative, so the pulse nets to zero at any tick rate and
                // reads as a jolt, not the game yanking the wheel sideways.
                float half = Math.Max(0.001f, s.ShiftPulseSeconds);
                if (_shiftPulseT < half) { ev += s.ShiftPulseStrength; _shiftEmitted += s.ShiftPulseStrength; }
                else if (_shiftEmitted > 0f) { float back = Math.Min(s.ShiftPulseStrength, _shiftEmitted); ev -= back; _shiftEmitted -= back; }
                _shiftPulseT += dt;
                if (_shiftPulseT >= half && _shiftEmitted <= 0f) _shiftPulseT = -1f;
                isEvent = true;
            }

            LastWasEvent = isEvent;
            float total = structural + texture + ev;
            Validator.Observe(i, structural);
            return Math.Max(-1f, Math.Min(1f, total));
        }
    }

    /// <summary>
    /// Watches the model's inputs for the two failures that were only ever found
    /// by reading a trace: an input that never moves (OutRun's steer at zero
    /// through corners) and a force whose sign disagrees with the steering by
    /// speed band (art of rally's aligning torque past the slip peak).
    /// </summary>
    public sealed class InputValidator
    {
        private readonly int[] _agree = new int[4], _disagree = new int[4];
        private int _steerSamples, _steerNonZero, _lateralNonZero, _lateralWithSteerZero;
        private float _maxAbsSteer, _maxAbsLateral;

        public void Reset() { Array.Clear(_agree, 0, 4); Array.Clear(_disagree, 0, 4); _steerSamples = _steerNonZero = _lateralNonZero = _lateralWithSteerZero = 0; _maxAbsSteer = _maxAbsLateral = 0f; }

        private static int Band(float mps) => mps < 3.3f ? 0 : mps < 8.3f ? 1 : mps < 16.7f ? 2 : 3;   // <12, <30, <60, 60+ km/h

        public void Observe(in ForceInputs i, float structuralForce)
        {
            _steerSamples++;
            float lat = i.HasFrontLateralForce ? i.FrontLateralForce : (i.HasLateralG ? i.LateralG : 0f);
            if (i.HasSteer && Math.Abs(i.Steer) > 0.05f) _steerNonZero++;
            if (Math.Abs(lat) > 0.02f * (i.HasFrontLateralForce ? 1000f : 1f)) { _lateralNonZero++; if (i.HasSteer && Math.Abs(i.Steer) <= 0.02f) _lateralWithSteerZero++; }
            _maxAbsSteer = Math.Max(_maxAbsSteer, Math.Abs(i.Steer));
            _maxAbsLateral = Math.Max(_maxAbsLateral, Math.Abs(lat));

            // Sign agreement only when both are clearly non-zero: centring means the
            // force opposes the steer, so "agree" here means opposite signs.
            if (i.HasSteer && Math.Abs(i.Steer) > 0.15f && Math.Abs(structuralForce) > 0.02f)
            {
                int b = Band(Math.Abs(i.SpeedMps));
                if (Math.Sign(structuralForce) == -Math.Sign(i.Steer)) _agree[b]++; else _disagree[b]++;
            }
        }

        /// <summary>True when the steer input never moved while lateral load did — a dead input.</summary>
        public bool SteerLooksDead => _steerSamples > 200 && _lateralNonZero > 50 && _steerNonZero == 0 && _lateralWithSteerZero > 50;

        /// <summary>Fraction of samples (above 30 km/h) where the force centred; below ~0.7 the source signal is suspect.</summary>
        public float CentringFractionAtSpeed
        {
            get { int a = _agree[2] + _agree[3], d = _disagree[2] + _disagree[3]; return a + d == 0 ? 1f : a / (float)(a + d); }
        }

        /// <summary>One paragraph for logs and support bundles.</summary>
        public string Report()
        {
            var sb = new StringBuilder();
            sb.Append("inputs: samples=").Append(_steerSamples)
              .Append(" steer|max|=").Append(_maxAbsSteer.ToString("F2"))
              .Append(" lateral|max|=").Append(_maxAbsLateral.ToString("F1"));
            sb.Append(" | centring by band <12/<30/<60/60+ km/h: ");
            for (int b = 0; b < 4; b++)
            {
                int t = _agree[b] + _disagree[b];
                sb.Append(t == 0 ? "-" : (100 * _agree[b] / t).ToString()).Append(b < 3 ? "% / " : "%");
            }
            if (SteerLooksDead) sb.Append(" | WARNING: steer input never leaves zero while lateral load moves - dead input");
            if (CentringFractionAtSpeed < 0.7f) sb.Append(" | WARNING: force opposes the steer less than 70% of the time above 30 km/h - check the source signal for reversal");
            return sb.ToString();
        }
    }
}
