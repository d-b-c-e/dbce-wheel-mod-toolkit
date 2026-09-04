using System;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    public class ForceShaperTests
    {
        private static ForceShaper Plain() => new ForceShaper
        {
            Deadzone = 0f, AttackSmoothing = 0f, DecaySmoothing = 0f, RampSeconds = 0f,
            FadeStartKmh = 0f, FadeFullKmh = 0f, OutputDeadband = 0f,
        };

        [Fact]
        public void UnityGainAtStrengthFifty_AndLinearAbove()
        {
            var s = Plain(); s.Strength = 50;
            Assert.Equal(0.5f, s.Shape(0.5f, 100f, 0.02f), 3);
            s.Strength = 100;
            Assert.Equal(1f, s.Shape(0.5f, 100f, 0.02f), 3);   // clamped at full scale
            s.Strength = 25;
            Assert.Equal(0.25f, s.Shape(0.5f, 100f, 0.02f), 3);
        }

        [Fact]
        public void InvertFlipsTheSignOnly()
        {
            var s = Plain(); s.Invert = true;
            Assert.Equal(-0.4f, s.Shape(0.4f, 100f, 0.02f), 3);
        }

        [Fact]
        public void LowSpeedFadeIsZeroAtRestAndFullAboveTwelveKmh()
        {
            var s = Plain(); s.FadeStartKmh = 3f; s.FadeFullKmh = 12f;
            Assert.Equal(0f, s.Shape(0.8f, 0f, 0.02f), 3);
            Assert.Equal(0f, s.Shape(0.8f, 3f, 0.02f), 3);
            Assert.Equal(0.8f, s.Shape(0.8f, 12f, 0.02f), 3);
            float half = s.Shape(0.8f, 7.5f, 0.02f);
            Assert.InRange(half, 0.35f, 0.45f);   // smoothstep midpoint
        }

        [Fact]
        public void RampStartsFromZeroAndRestartsOnDemand()
        {
            var s = Plain(); s.RampSeconds = 0.25f;
            float first = s.Shape(1f, 100f, 0.05f);
            Assert.InRange(first, 0.15f, 0.25f);
            for (int i = 0; i < 10; i++) s.Shape(1f, 100f, 0.05f);
            Assert.Equal(1f, s.Shape(1f, 100f, 0.05f), 3);
            s.RestartRamp();
            Assert.InRange(s.Shape(1f, 100f, 0.05f), 0.15f, 0.25f);
        }

        [Fact]
        public void SlewLimitsStructuralButEventsBypassIt()
        {
            var s = Plain(); s.SlewPerSecond = 2f;   // full scale in 0.5 s
            float a = s.Shape(1f, 100f, 0.1f);       // 0.2 per 100 ms step
            Assert.Equal(0.2f, a, 3);
            float ev = s.Shape(1f, 100f, 0.1f, isEvent: true);
            Assert.Equal(1f, ev, 3);
        }

        [Fact]
        public void DeadzoneIsSubtractive()
        {
            var s = Plain(); s.Deadzone = 0.1f;
            Assert.Equal(0f, s.Shape(0.05f, 100f, 0.02f), 3);
            Assert.Equal(0.4f, s.Shape(0.5f, 100f, 0.02f), 3);
        }

        [Fact]
        public void DualRateSmoothingFollowsAttackThenDecay()
        {
            var s = Plain(); s.AttackSmoothing = 0.5f; s.DecaySmoothing = 0.9f;
            float up = s.Shape(1f, 100f, 0.02f);
            Assert.Equal(0.5f, up, 3);                       // half way up in one step
            s.Shape(1f, 100f, 0.02f); s.Shape(1f, 100f, 0.02f);
            float down = s.Shape(0f, 100f, 0.02f);
            Assert.True(down > 0.7f);                        // slow release
        }

        [Fact]
        public void PeakLimitCapsAfterGain()
        {
            var s = Plain(); s.Strength = 100; s.PeakLimit = 0.6f;
            Assert.Equal(0.6f, s.Shape(0.9f, 100f, 0.02f), 3);
            Assert.Equal(-0.6f, s.Shape(-0.9f, 100f, 0.02f), 3);
        }

        [Fact]
        public void NaNInputBecomesZero()
        {
            var s = Plain();
            Assert.Equal(0f, s.Shape(float.NaN, 100f, 0.02f));
        }
    }

    public class ForceModelTests
    {
        [Fact]
        public void SimLite_CentresAgainstLateralForce_AndLightensTowardTheLimit()
        {
            var m = new ForceModel(ForceModelSettings.SimLite());
            var i = new ForceInputs { SpeedMps = 25f, HasSteer = true, Steer = 0.5f, HasFrontLateralForce = true, FrontLateralForce = 4000f, HasSlip = true, FrontSlipAngleDeg = 2f, IdealSlipAngleDeg = 8.5f };
            float small = m.Compute(i, 0.02f);
            Assert.True(small < 0f, "positive lateral force must produce a centring (negative) output");
            float full = 4000f / m.Settings.LateralForceReference;          // before the trail
            Assert.InRange(Math.Abs(small), full * 0.90f, full * 1.0f);      // 2° of 8.5°: trail ≈ 0.95
            i.FrontSlipAngleDeg = 17f;                                        // twice ideal → trail floor 0.6
            float limit = m.Compute(i, 0.02f);
            Assert.InRange(Math.Abs(limit), full * 0.58f, full * 0.62f);
            Assert.True(Math.Abs(limit) < Math.Abs(small));
        }

        [Fact]
        public void SimLite_NeverReversesSignWithSlip()
        {
            var m = new ForceModel(ForceModelSettings.SimLite());
            for (float slip = 0f; slip < 45f; slip += 1f)
            {
                var i = new ForceInputs { SpeedMps = 30f, HasFrontLateralForce = true, FrontLateralForce = 3000f, HasSlip = true, FrontSlipAngleDeg = slip, IdealSlipAngleDeg = 8.5f };
                Assert.True(m.Compute(i, 0.02f) < 0f);
            }
        }

        [Fact]
        public void Arcade_SpringAndDamperCentre_AndSpringScalesWithSpeed()
        {
            var m = new ForceModel(ForceModelSettings.Arcade());
            var slow = new ForceInputs { SpeedMps = 5f, HasSteer = true, Steer = 0.5f };
            var fast = new ForceInputs { SpeedMps = 40f, HasSteer = true, Steer = 0.5f };
            float fs = m.Compute(slow, 0.02f), ff = m.Compute(fast, 0.02f);
            Assert.True(fs < 0f && ff < 0f);
            Assert.True(Math.Abs(ff) > Math.Abs(fs));
            var rate = new ForceInputs { SpeedMps = 40f, HasSteerRate = true, SteerRate = 2f };
            Assert.True(m.Compute(rate, 0.02f) < 0f);
        }

        [Fact]
        public void Arcade_LateralGWithGripLossLightens()
        {
            var m = new ForceModel(ForceModelSettings.Arcade());
            var grip = new ForceInputs { SpeedMps = 40f, HasLateralG = true, LateralG = 0.8f, HasDrift = true, DriftAmount = 0f };
            var drift = new ForceInputs { SpeedMps = 40f, HasLateralG = true, LateralG = 0.8f, HasDrift = true, DriftAmount = 1f };
            Assert.True(Math.Abs(m.Compute(drift, 0.02f)) < Math.Abs(m.Compute(grip, 0.02f)));
        }

        [Fact]
        public void ShiftPulseIsSymmetricAndFlaggedAsEvent()
        {
            var m = new ForceModel(ForceModelSettings.Arcade());
            var i = new ForceInputs { SpeedMps = 40f, GearShift = true };
            float sum = 0f; int events = 0;
            float first = m.Compute(i, 0.02f); sum += first; if (m.LastWasEvent) events++;
            i.GearShift = false;
            for (int k = 0; k < 4; k++) { sum += m.Compute(i, 0.02f); if (m.LastWasEvent) events++; }
            Assert.True(first > 0f);
            Assert.InRange(sum, -0.05f, 0.05f);   // nets to zero
            Assert.True(events >= 4);
        }

        [Fact]
        public void ImpactDecaysAndUsesTheDirectionHint()
        {
            var m = new ForceModel(ForceModelSettings.Arcade());
            var i = new ForceInputs { SpeedMps = 30f, Impact = 1f, ImpactDirection = -1f };
            float a = m.Compute(i, 0.02f);
            Assert.True(a < 0f && m.LastWasEvent);
            i.Impact = 0f;
            float b = m.Compute(i, 0.02f), c = m.Compute(i, 0.02f);
            Assert.True(Math.Abs(b) < Math.Abs(a) && Math.Abs(c) < Math.Abs(b));
        }

        [Fact]
        public void Validator_FlagsADeadSteerInput()
        {
            var m = new ForceModel(ForceModelSettings.Arcade());
            for (int k = 0; k < 400; k++)
            {
                var i = new ForceInputs { SpeedMps = 30f, HasSteer = true, Steer = 0f, HasLateralG = true, LateralG = 0.6f };
                m.Compute(i, 0.02f);
            }
            Assert.True(m.Validator.SteerLooksDead);
            Assert.Contains("dead input", m.Validator.Report());
        }

        [Fact]
        public void Validator_ReportsCentringFractionByBand()
        {
            var m = new ForceModel(ForceModelSettings.SimLite());
            for (int k = 0; k < 100; k++)
            {
                // consistent: steer right, lateral force positive → centring
                m.Compute(new ForceInputs { SpeedMps = 30f, HasSteer = true, Steer = 0.5f, HasFrontLateralForce = true, FrontLateralForce = 3000f }, 0.02f);
            }
            Assert.Equal(1f, m.Validator.CentringFractionAtSpeed, 3);
            for (int k = 0; k < 300; k++)
            {
                // a reversed signal: steer right, lateral force negative → anti-centring
                m.Compute(new ForceInputs { SpeedMps = 30f, HasSteer = true, Steer = 0.5f, HasFrontLateralForce = true, FrontLateralForce = -3000f }, 0.02f);
            }
            Assert.True(m.Validator.CentringFractionAtSpeed < 0.7f);
            Assert.Contains("reversal", m.Validator.Report());
        }
    }

    public class DevicePreferenceTests
    {
        [Fact]
        public void ParsesVidPidIndexAndName()
        {
            var a = DevicePreference.Parse("346e:0006"); Assert.Equal(0x346e, a.Vid); Assert.Equal(6, a.Pid);
            var b = DevicePreference.Parse("#1"); Assert.Equal(1, b.Index);
            var c = DevicePreference.Parse("FANATEC Wheel"); Assert.Equal("FANATEC Wheel", c.NameContains);
            Assert.True(DevicePreference.Parse("").IsEmpty);
            Assert.Equal(0x0006346eu, DevicePreference.ProductKey(0x346e, 0x0006));
        }

        [Fact]
        public void VirtualPadsAreNeverChosenByDefault()
        {
            var devices = new[]
            {
                new WheelFfbNative.DeviceInfo { Index = 0, Name = "vJoy Device", Axes = 8, Buttons = 128, ForceFeedback = true },
                new WheelFfbNative.DeviceInfo { Index = 1, Name = "MOZA R12 Base", Axes = 8, Buttons = 128, ForceFeedback = true },
            };
            Assert.Equal(1, DevicePreference.Parse("").Choose(devices));
            Assert.Equal(0, DevicePreference.Parse("#0").Choose(devices));        // explicit wins
            Assert.Equal(1, DevicePreference.Parse("moza").Choose(devices));
            Assert.True(DevicePreference.IsVirtualDevice("XOutput Virtual Pad"));
            Assert.False(DevicePreference.IsVirtualDevice("FANATEC Wheel"));
        }
    }

    public class DiagnosticsTests
    {
        [Fact]
        public void SummaryReadsOneSidedForces()
        {
            var d = new FfbDiagnostics();
            for (int k = 0; k < 50; k++) d.RecordOutput(3000f, 80f, 0.5f, 8000f, 50, k * 0.02f);
            Assert.Contains("only ever point one way", d.Summary());
        }

        [Fact]
        public void TraceIsRateLimited()
        {
            var d = new FfbDiagnostics { TraceHz = 5f };
            var i = new ForceInputs { SpeedMps = 20f, HasSteer = true, Steer = 0.2f };
            int lines = 0;
            for (int k = 0; k < 100; k++) if (d.Trace(k * 0.02f, i, -0.1f, -0.1f) != null) lines++;
            Assert.InRange(lines, 9, 11);   // 2 s at 5 Hz
        }
    }
}
