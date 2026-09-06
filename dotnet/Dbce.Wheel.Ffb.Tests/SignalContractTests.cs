using System;
using System.Globalization;
using System.IO;
using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    public class SignalContractTests
    {
        [Fact]
        public void NativeAndManagedUseTheSameImpactContract()
        {
            var lines = File.ReadAllLines(Path.Combine(AppContext.BaseDirectory, "impact-contract.csv"));
            Assert.Equal(9, lines.Length);
            for (int i = 1; i < lines.Length; i++)
            {
                var v = Array.ConvertAll(lines[i].Split(','), x => double.Parse(x, CultureInfo.InvariantCulture));
                var m = new ImpactMixer();
                Assert.True(m.Trigger(1, (float)v[3], 0));
                Assert.InRange(Math.Abs(m.Mix((float)v[1], v[0], (float)v[2]) - v[4]), 0, 1e-6);
            }
        }
        [Fact]
        public void OverlapIsBoundedAndRewindCancelsPendingImpacts()
        {
            var m = new ImpactMixer();
            for (int i = 0; i < 8; i++) Assert.True(m.Trigger(1, 1, 0));
            Assert.False(m.Trigger(1, 1, 0));
            Assert.Equal(1UL, m.RejectedEvents);
            Assert.InRange(m.Mix(1, .005), -1, 1);
            Assert.InRange(m.Mix(-1, .010), -1, 1);
            Assert.Equal(0, m.Mix(1, .001));
            Assert.Equal(0, m.Mix(0, .005));
            double area = 0;
            for (int i = 0; i < 1000; i++) area += ImpactMixer.Envelope(i * .0001) * .0001;
            Assert.InRange(Math.Abs(area), 0, 1e-8);
        }
        [Fact]
        public void ScalarValidityDistinguishesStoppedStaleAndUnavailable()
        {
            var s = new SignalSample();
            Assert.False(s.Usable(0, 1));
            s.Source = SignalSource.HudOcr; s.Quality = SignalQuality.Held;
            s.Unit = SignalUnit.MetresPerSecond; s.Seconds = 10; s.Value = 0;
            Assert.True(s.Usable(10.1, .2));
            Assert.False(s.Usable(10.3, .2));
            Assert.False(s.Usable(9, .2));
            s.Value = double.NaN;
            Assert.False(s.Usable(10, 1));
        }
    }
}
