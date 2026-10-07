using System;
using Dbce.Wheel.Ffb;
using Xunit;

namespace Dbce.Wheel.Ffb.Tests
{
    public class CrashCueTests
    {
        private static CrashDetector Settled()
        {
            var d = new CrashDetector();
            for (int i = 0; i <= 10; i++) d.Track(i * .02);   // 0.2 s of live car
            return d;
        }

        [Fact]
        public void IntensityFollowsArtOfRallysNormalSpeedRule()
        {
            var d = Settled();
            Assert.Equal(0, d.Observe(.2, 3f, 0, false));                 // at the threshold: nothing
            Assert.Equal((10 - 3) / 17f, Settled().Observe(.2, 10f, 0, false), 5);
            Assert.Equal(1, Settled().Observe(.2, 40f, .1f, false));       // capped
            Assert.Equal((10 - 3) / 17f, Settled().Observe(.2, -10f, 0, false), 5);  // sign of the normal speed is irrelevant
        }

        [Fact]
        public void RoadGroundAndFreshSpawnsDoNotCrash()
        {
            Assert.Equal(0, Settled().Observe(.2, 15f, 0, true));          // road collider
            Assert.Equal(0, Settled().Observe(.2, 15f, .9f, false));       // landing / ground under the car
            var fresh = new CrashDetector(); fresh.Track(1.0);
            Assert.Equal(0, fresh.Observe(1.05, 15f, 0, false));           // within 0.1 s of (re)spawn
            Assert.Equal(0, new CrashDetector().Observe(1, 15f, 0, false)); // never tracked
            var gap = Settled(); gap.Track(5);                             // physics gap resets the settle time
            Assert.Equal(0, gap.Observe(5.01, 15f, 0, false));
        }

        [Fact]
        public void OneHitGivesOneCueUnlessAStrongerContactArrivesQuickly()
        {
            var d = Settled();
            Assert.True(d.Observe(.20, 8f, 0, false) > 0);
            Assert.Equal(0, d.Observe(.25, 6f, 0, false));                 // weaker manifold contact merged
            Assert.True(d.Observe(.28, 14f, 0, false) > 0);                // stronger within 0.12 s replaces
            Assert.Equal(0, d.Observe(.40, 18f, 0, false));                // stronger but after 0.12 s, inside 0.35 s
            Assert.True(d.Observe(.60, 6f, 0, false) > 0);                 // a new hit after 0.35 s
        }

        [Fact]
        public void CueIsA120MsPushWithA25HzRattleFadingOutBy250Ms()
        {
            var c = new CrashCue();
            Assert.True(c.Trigger(.6f, 10));
            Assert.Equal(.6f * 1.5f, c.Sample(10), 4);                     // push + rattle crest at t=0
            Assert.Equal(.6f * .5f, c.Sample(10.100), 3);                  // 2.5 rattle periods later, cos(5 pi) = -1: push 1 - 0.5
            Assert.Equal(.6f * -.5f * 110 / 150f, c.Sample(10.140), 3);    // push over after 120 ms; at 140 ms cos(7 pi) = -1, fade 110/150
        }

        [Fact]
        public void RattleFadesLinearlyOverItsLast150MsAndStops()
        {
            var c = new CrashCue(); c.Trigger(1f, 0);
            // At 160 ms (4 periods, cos = 1), fade = (250-160)/150 = 0.6: rattle 0.5 x 0.6.
            Assert.Equal(.3f, c.Sample(.160), 3);
            Assert.Equal(0f, c.Sample(.250), 4);
            Assert.False(c.Active(.251));
            Assert.Equal(0f, c.Sample(.4), 4);
        }

        [Fact]
        public void AStrongerHitRestartsTheCueAndInvalidInputIsRefused()
        {
            var c = new CrashCue();
            Assert.True(c.Trigger(.3f, 0));
            Assert.False(c.Trigger(.2f, .05));                             // weaker during the cue
            Assert.True(c.Trigger(.8f, .06));                              // stronger restarts
            Assert.Equal(.8f * 1.5f > 1 ? 1f : .8f * 1.5f, c.Sample(.06), 4);  // clamped to full force
            Assert.False(c.Trigger(float.NaN, 1));
            Assert.False(c.Trigger(0, 1));
            Assert.Equal(2, c.Played);
            c.Reset(); Assert.Equal(0f, c.Sample(.07), 4);
        }
    }
}
