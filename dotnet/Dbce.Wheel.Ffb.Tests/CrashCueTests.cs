using System;
using System.Linq;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    public class CrashCueTests
    {
        private static MotionSample At(double t, float x = 0) => new MotionSample { Time = t, X = x };

        private static CrashDetector Settled()
        {
            var d = new CrashDetector();
            for (int i = 0; i <= 10; i++) d.Track(At(i * .02));   // 0.2 s of live, stationary car
            return d;
        }

        [Fact]
        public void IntensityFollowsArtOfRallysNormalSpeedRule()
        {
            Assert.Equal(0, Settled().Observe(.2, 3f, 0, false));
            Assert.Equal((10 - 3) / 17f, Settled().Observe(.2, 10f, 0, false), 5);
            Assert.Equal(1, Settled().Observe(.2, 40f, .1f, false));
            Assert.Equal((10 - 3) / 17f, Settled().Observe(.2, -10f, 0, false), 5);
        }

        // Codex's review scenarios (CRASH-PORT-REVIEW-2026-10-06): art of rally gives no cue in each; the first port gave 1.
        [Theory]
        [InlineData("stale-contact")]
        [InlineData("150-ms-physics-gap")]
        [InlineData("duplicate-tick")]
        [InlineData("teleport")]
        public void ContinuityBreaksAndStaleContactsGiveNoCueAsInArtOfRally(string scenario)
        {
            var d = Settled(); double time = .2;
            if (scenario == "stale-contact") time = 1;
            if (scenario == "150-ms-physics-gap") { time = .35; d.Track(At(time)); }
            if (scenario == "duplicate-tick") d.Track(At(time));
            if (scenario == "teleport") { time = .22; d.Track(At(time, 50)); }
            Assert.Equal(0, d.Observe(time, 20, 0, false));
            if (scenario != "stale-contact") Assert.True(d.Discontinuous);
        }

        [Fact]
        public void RoadGroundFreshEpochsAndUntrackedCarsDoNotCrash()
        {
            Assert.Equal(0, Settled().Observe(.2, 15f, 0, true));
            Assert.Equal(0, Settled().Observe(.2, 15f, .9f, false));
            var fresh = new CrashDetector(); fresh.Track(At(1.0));
            Assert.Equal(0, fresh.Observe(1.05, 15f, 0, false));
            Assert.Equal(0, new CrashDetector().Observe(1, 15f, 0, false));
            var moving = new CrashDetector();                            // 30 m/s along x, positions consistent
            for (int i = 0; i <= 10; i++) moving.Track(new MotionSample { Time = i * .02, X = i * .6f, Vx = 30 });
            Assert.False(moving.Discontinuous);
            Assert.True(moving.Observe(.21, 15f, 0, false) > 0);
            Assert.Equal(0, Settled().Observe(.19, 15f, 0, false));         // older than the last tracked tick
        }

        [Fact]
        public void OneHitGivesOneCueUnlessAStrongerContactArrivesQuickly()
        {
            var d = Settled();
            Assert.True(d.Observe(.20, 8f, 0, false) > 0);
            d.Track(At(.22)); Assert.Equal(0, d.Observe(.23, 6f, 0, false));
            d.Track(At(.24)); Assert.True(d.Observe(.25, 14f, 0, false) > 0);    // stronger within 0.12 s
            for (double t = .26; t <= .40; t += .02) d.Track(At(t));
            Assert.Equal(0, d.Observe(.40, 18f, 0, false));                      // stronger, but after 0.12 s, inside 0.35 s
            for (double t = .42; t <= .60; t += .02) d.Track(At(t));
            Assert.True(d.Observe(.60, 6f, 0, false) > 0);                       // a new hit after 0.35 s
        }

        [Fact]
        public void RequestsAreArtOfRallysIndependentPushAndRattle()
        {
            var r = CrashCue.Requests(.5f);
            Assert.Equal(.5f, r.PushMagnitude); Assert.Equal(120, r.PushMs);
            Assert.Equal(.25f, r.RattleMagnitude); Assert.Equal(25, r.RattleHz); Assert.Equal(250, r.RattleMs); Assert.Equal(150, r.RattleFadeMs);
            Assert.Equal(1f, CrashCue.Requests(3f).PushMagnitude);
            Assert.Equal(0f, CrashCue.Requests(float.NaN).PushMagnitude);
        }

        [Fact]
        public void FallbackOnsetIsThePushAloneBecauseTheSineStartsAtZero()
        {
            var c = new CrashCue(); c.Trigger(.5f, 10);
            Assert.Equal(.5f, c.Sample(10), 5);                      // art of rally onset +0.50, not 0.75
        }

        [Fact]
        public void FallbackAveragesTheRattleSoNoUpdateRateAliasesItIntoAConstant()
        {
            float[] Run(int hz) { var c = new CrashCue(); c.Trigger(.5f, 0); return Enumerable.Range(0, 3).Select(i => c.Sample(i / (double)hz)).ToArray(); }
            Assert.All(Run(25), v => Assert.Equal(.5f, v, 4));         // one full period per update: rattle averages out, push only
            var at50 = Run(50);
            Assert.Equal(.5f, at50[0], 4);
            Assert.Equal(.5f * (1 + .5f * 2 / MathF.PI), at50[1], 3);  // half period up: mean of the sine 2/pi
            Assert.Equal(.5f * (1 - .5f * 2 / MathF.PI), at50[2], 3);  // half period down
        }

        [Fact]
        public void FallbackFadesOutAndStopsAt250Ms()
        {
            var c = new CrashCue(); c.Trigger(1f, 0);
            for (double t = 0; t < .2; t += .01) c.Sample(t);
            Assert.False(c.Active(.25)); Assert.Equal(0f, c.Sample(.26), 4);
            var d = new CrashCue(); d.Trigger(1f, 0); d.Sample(0); d.Sample(.199);
            float late = d.Sample(.200);                               // 1 ms interval near the end: |rattle| <= 0.5 x fade(199.5 ms)
            Assert.InRange(Math.Abs(late), 0, .5f * (250 - 199.5f) / 150 + .01f);
        }

        [Fact]
        public void AStrongerHitRestartsTheCueAndInvalidInputIsRefused()
        {
            var c = new CrashCue();
            Assert.True(c.Trigger(.3f, 0));
            Assert.False(c.Trigger(.2f, .05));
            Assert.True(c.Trigger(.8f, .06));
            Assert.Equal(.8f, c.Sample(.06), 4);                       // restart onset is the push alone
            Assert.False(c.Trigger(float.NaN, 1));
            Assert.False(c.Trigger(0, 1));
            Assert.Equal(2, c.Played);
            c.Reset(); Assert.Equal(0f, c.Sample(.07), 4);
            Assert.Equal("crash-constant-fallback@2", CrashCue.FallbackModel);
        }
    }
}
