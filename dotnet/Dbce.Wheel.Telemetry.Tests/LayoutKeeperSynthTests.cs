using System;
using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests
{
    public class LayoutTests
    {
        private static TelemetryFrame Sample() => new TelemetryFrame
        {
            IsRaceOn = true, TimestampMs = 1234, EngineMaxRpm = 7500f, EngineIdleRpm = 900f, CurrentEngineRpm = 4321f,
            Speed = 33.5f, Gear = 3, Accel = 200, Brake = 10, Steer = -50, PositionX = 1f, PositionY = 2f, PositionZ = 3f,
            LapNumber = 7, RacePosition = 2,
        };

        [Theory]
        [InlineData(ForzaLayout.Sled232, 232)]
        [InlineData(ForzaLayout.Fm7Dash311, 311)]
        [InlineData(ForzaLayout.Horizon324, 324)]
        public void SizesAreExact_BecauseReceiversValidateByLength(ForzaLayout layout, int size)
        {
            Assert.Equal(size, ForzaPacket.SizeOf(layout));
            var buf = new byte[size];
            ForzaPacket.Write(Sample(), buf, layout);   // must not throw at the exact size
        }

        [Fact]
        public void HorizonOverloadMatchesTheOriginalWriteByteForByte()
        {
            var a = ForzaPacket.CreateBuffer();
            var b = new byte[ForzaPacket.SizeOf(ForzaLayout.Horizon324)];
            ForzaPacket.Write(Sample(), a);
            ForzaPacket.Write(Sample(), b, ForzaLayout.Horizon324);
            Assert.Equal(a, b);
        }

        [Fact]
        public void SledIsIdenticalAcrossLayouts()
        {
            var sled = new byte[232]; var fm7 = new byte[311]; var fh = new byte[324];
            ForzaPacket.Write(Sample(), sled, ForzaLayout.Sled232);
            ForzaPacket.Write(Sample(), fm7, ForzaLayout.Fm7Dash311);
            ForzaPacket.Write(Sample(), fh, ForzaLayout.Horizon324);
            for (int i = 0; i < 232; i++)
            {
                Assert.Equal(sled[i], fm7[i]);
                Assert.Equal(sled[i], fh[i]);
            }
        }

        [Fact]
        public void Fm7DashSitsTwelveBytesEarlierThanHorizon()
        {
            var fm7 = new byte[311]; var fh = new byte[324];
            ForzaPacket.Write(Sample(), fm7, ForzaLayout.Fm7Dash311);
            ForzaPacket.Write(Sample(), fh, ForzaLayout.Horizon324);
            // Speed: Horizon 256, FM7 244. Gear: 319 vs 307. Steer: 320 vs 308.
            Assert.Equal(BitConverter.ToSingle(fh, 256), BitConverter.ToSingle(fm7, 244));
            Assert.Equal(33.5f, BitConverter.ToSingle(fm7, 244));
            Assert.Equal(fh[319], fm7[307]);
            Assert.Equal((byte)3, fm7[307]);
            Assert.Equal(fh[320], fm7[308]);
            Assert.Equal(unchecked((byte)(sbyte)-50), fm7[308]);
            Assert.Equal(fh[315], fm7[303]);   // Accel
            Assert.Equal(fh[316], fm7[304]);   // Brake
        }

        [Fact]
        public void OnlyHorizonCarriesTheSentinel()
        {
            var fm7 = new byte[311]; var fh = new byte[324];
            ForzaPacket.Write(Sample(), fm7, ForzaLayout.Fm7Dash311);
            ForzaPacket.Write(Sample(), fh, ForzaLayout.Horizon324);
            Assert.Equal(ForzaPacket.SourceSentinel, fh[323]);
            Assert.Equal(311, fm7.Length);   // nothing past the dash
        }

        [Fact]
        public void UndersizedBufferForTheLayoutIsRejected()
        {
            Assert.Throws<ArgumentException>(() => ForzaPacket.Write(Sample(), new byte[310], ForzaLayout.Fm7Dash311));
            Assert.Throws<ArgumentException>(() => ForzaPacket.Write(Sample(), new byte[231], ForzaLayout.Sled232));
        }
    }

    public class IdleKeeperTests
    {
        [Fact]
        public void KeeperFrameIsLiveZeroedAndNotDegenerate()
        {
            using var keeper = new IdleKeeper(new[] { "127.0.0.1:65000" }, 7500f, 700f);
            var buf = keeper.BuildFrame(100);
            Assert.Equal(324, buf.Length);
            Assert.Equal(1, BitConverter.ToInt32(buf, ForzaPacket.OffIsRaceOn));      // dash stays live
            Assert.Equal(7500f, BitConverter.ToSingle(buf, ForzaPacket.OffEngineMaxRpm)); // never a zero max rpm
            Assert.Equal(700f, BitConverter.ToSingle(buf, ForzaPacket.OffEngineIdleRpm));
            Assert.Equal(0f, BitConverter.ToSingle(buf, ForzaPacket.OffSpeed));
            Assert.Equal(0f, BitConverter.ToSingle(buf, ForzaPacket.OffCurrentEngineRpm));
            Assert.Equal((byte)1, buf[ForzaPacket.OffGear]);                          // 0 would show reverse
            Assert.Equal(IdleKeeper.SourceSentinel, buf[ForzaPacket.OffSourceSentinel]);
            Assert.Equal(100u, BitConverter.ToUInt32(buf, ForzaPacket.OffTimestampMs));
        }

        [Fact]
        public void KeeperHonoursTheLayout()
        {
            using var keeper = new IdleKeeper(new[] { ":65001" }, 8500f, 900f, ForzaLayout.Fm7Dash311);
            var buf = keeper.BuildFrame(1);
            Assert.Equal(311, buf.Length);
            Assert.Equal((byte)1, buf[307]);
        }

        [Fact]
        public void NoTargetsMeansInert()
        {
            using var keeper = new IdleKeeper(new string[0], 7500f, 700f);
            keeper.Start();
            Assert.Equal(0, keeper.FramesSent);
        }
    }

    public class RpmSynthesizerTests
    {
        [Fact]
        public void RisesWithinAGearAndDropsOnTheShift()
        {
            var s = new RpmSynthesizer(new[] { 20f, 40f, 60f, 90f }, 7500f, 900f);
            Assert.Equal(900f, s.Rpm(0f), 1);
            float nearTopOfFirst = s.Rpm(19.9f);
            float startOfSecond = s.Rpm(20.1f);
            Assert.True(nearTopOfFirst > 7000f);
            Assert.True(startOfSecond < nearTopOfFirst);
            Assert.True(s.Rpm(1000f) <= 7500f);
        }

        [Fact]
        public void GearIsInferredFromSpeedWhenTheGameHasNone()
        {
            var s = new RpmSynthesizer(new[] { 20f, 40f, 60f, 90f });
            Assert.Equal(1, s.InferGear(10f));
            Assert.Equal(3, s.InferGear(50f));
            Assert.Equal(4, s.InferGear(500f));
            Assert.Equal(2, s.InferGear(-30f));   // reversing still shows a gear band
        }

        [Fact]
        public void FromTopSpeedProducesAnAscendingTable()
        {
            var s = RpmSynthesizer.FromTopSpeed(80f, 6);
            Assert.Equal(6, s.GearTopSpeedsMps.Length);
            Assert.Equal(80f, s.GearTopSpeedsMps[5], 3);
            for (int i = 1; i < 6; i++) Assert.True(s.GearTopSpeedsMps[i] > s.GearTopSpeedsMps[i - 1]);
        }

        [Fact]
        public void RejectsBadTables()
        {
            Assert.Throws<ArgumentException>(() => new RpmSynthesizer(new float[0]));
            Assert.Throws<ArgumentException>(() => new RpmSynthesizer(new[] { 10f, 10f }));
        }
    }
}
