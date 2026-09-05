using System;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    public class EventRegressionTests
    {
        [Fact]
        public void CenteredImpactDoesNotDisappearAndResetClearsDiagnostics()
        {
            var m = new ForceModel(new ForceModelSettings { ImpactStrength = 0.5f });
            float f = m.Compute(new ForceInputs { HasSteer = true, Steer = 0, Impact = 1 }, 0.01f);
            Assert.Equal(0.5f, Math.Abs(f));
            Assert.True(m.LastWasEvent);
            m.Reset();
            Assert.False(m.LastWasEvent);
            Assert.Equal(0f, m.LastStructural);
            Assert.Equal(0f, m.Compute(new ForceInputs(), 0.01f));
        }

        [Theory]
        [InlineData(30)] [InlineData(60)] [InlineData(250)] [InlineData(1000)]
        public void IdleThenHitTriggersOnceAndHeldForceDoesNotRetrigger(int hz)
        {
            var d = new ForceRiseDetector();
            int hits = 0;
            for (int n = 0; n < hz * 3; n++)
                if (d.Observe(n < hz ? 0 : 1, n / (double)hz)) hits++;
            Assert.Equal(1, hits);
        }

        [Fact]
        public void RampTriggersButLowCornerAndStartupPlateauDoNot()
        {
            var d = new ForceRiseDetector();
            for (int n = 0; n < 100; n++) Assert.False(d.Observe(1, n * 0.01));
            d.Reset();
            int hits = 0;
            for (int n = 0; n <= 10; n++) if (d.Observe(n / 10f, n * 0.01)) hits++;
            Assert.Equal(1, hits);
            d.Reset();
            for (int n = 0; n < 100; n++) Assert.False(d.Observe(n % 2 == 0 ? 0 : 0.6f, n * 0.01));
        }

        [Fact]
        public void InvalidOrReversedClockResetsHistoryAndDetectorRecovers()
        {
            var d = new ForceRiseDetector();
            Assert.False(d.Observe(0, 10));
            Assert.False(d.Observe(1, 9));
            Assert.False(d.Observe(float.NaN, 11));
            Assert.False(d.Observe(1, 12));
            Assert.False(d.Observe(0, 13));
            Assert.True(d.Observe(1, 13.1));
            Assert.False(d.Observe(1, 13.1));
        }
    }
}
