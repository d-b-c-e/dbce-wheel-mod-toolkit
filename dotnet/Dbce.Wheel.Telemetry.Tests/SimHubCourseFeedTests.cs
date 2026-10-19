using System;
using System.Linq;
using System.Text;
using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests
{
    /// <summary>
    /// Locks the draft SimHub external-sim layout against what SimHub 9.13.2 parses.
    /// </summary>
    /// <remarks>
    /// The expected field numbers, wire types and sizes below were read from SimHub 9.13.2's
    /// StandardFieldType attributes and TelemetryHeader struct (metadata only). They are a
    /// golden table: if SimHub changes them, these tests are where the draft must change.
    /// No signature here is real; SimHub's editor computes those for an actual definition.
    /// </remarks>
    public class SimHubCourseFeedTests
    {
        private const uint GameSig = 0x11223344u;
        private const uint TelemetrySig = 0x55667788u;

        private static CourseSample Course(bool finished = false)
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            CourseProgress progress = default;
            foreach (var (_, position) in SyntheticCourses.Drive(path, finished ? path.LengthM - 50 : 0, finished ? path.LengthM : 700))
                progress = tracker.Update(position);
            var sample = CourseSample.From(CourseIdentity.Create("synthetic", "hills", "stage", CourseDirection.Forward),
                "Hills – Stage", path, progress, path.PointAt(progress.DistanceM), 123.5, 41.25, 0xABCDEF0123456789UL);
            sample.DiscontinuityCount = 3;
            sample.IsSpectating = false;
            return sample;
        }

        private static TelemetryFrame Dash() => new TelemetryFrame
        {
            Speed = 25f, CurrentEngineRpm = 6500f, EngineMaxRpm = 7800f,
            Accel = 255, Brake = 0, Clutch = 51, HandBrake = 255,
            LastLap = 95.5f, BestLap = 93.25f,
        };

        private static SimHubDashText Text() => new SimHubDashText { VehicleName = "Group B Hatch", VehicleId = "car-08", Gear = "3" };

        private static SimHubHeader Header() => new SimHubHeader
        {
            GameSignature = GameSig, TelemetrySignature = TelemetrySig,
            LayoutMajorVersion = SimHubCourseFeed.LayoutMajor, LayoutMinorVersion = SimHubCourseFeed.LayoutMinor,
            EmitterInstanceId = 42, PacketsCounter = 1000, IsSessionRunning = 1, SessionTimeSeconds = 12.5,
            SessionId = 999, PhysicsDiscontinuityCounter = 77, // both overwritten from the course
        };

        [Fact]
        public void HeaderIs55BytesAndThePacketSizeIsFixed()
        {
            Assert.Equal(55, SimHubCourseFeed.HeaderSize);
            Assert.Equal(55 + 872, SimHubCourseFeed.PacketSize);
            Assert.Equal(SimHubCourseFeed.PacketSize, SimHubCourseFeed.CreateBuffer().Length);
        }

        [Fact]
        public void StandardFieldsAreInAscendingOrderAsSimHubsEditorSortsThem()
        {
            var numbers = SimHubCourseFeed.Fields.Select(f => f.StandardFieldType).ToArray();
            Assert.Equal(numbers.OrderBy(n => n).ToArray(), numbers);
            Assert.Equal(numbers.Length, numbers.Distinct().Count());
        }

        [Theory]
        [InlineData(0,  "YawDegrees",           SimHubWire.Float32, 4)]
        [InlineData(12, "VehiclePositionNorth", SimHubWire.Double,  8)]
        [InlineData(13, "VehiclePositionEast",  SimHubWire.Double,  8)]
        [InlineData(14, "VehiclePositionUp",    SimHubWire.Double,  8)]
        [InlineData(16, "SpeedKmh",             SimHubWire.Float32, 4)]
        [InlineData(18, "VehicleName",          SimHubWire.Utf8,    256)]
        [InlineData(19, "VehicleId",            SimHubWire.Utf8,    128)]
        [InlineData(20, "EnvironmentName",      SimHubWire.Utf8,    256)]
        [InlineData(21, "EnvironmentId",        SimHubWire.Utf8,    128)]
        [InlineData(25, "EngineRpm",            SimHubWire.Float32, 4)]
        [InlineData(26, "EngineMaxRpm",         SimHubWire.Float32, 4)]
        [InlineData(31, "Throttle",             SimHubWire.Float32, 4)]
        [InlineData(32, "Brake",                SimHubWire.Float32, 4)]
        [InlineData(33, "Clutch",               SimHubWire.Float32, 4)]
        [InlineData(34, "Handbrake",            SimHubWire.Float32, 4)]
        [InlineData(37, "TrackLength",          SimHubWire.Double,  8)]
        [InlineData(38, "Gear",                 SimHubWire.Utf8,    8)]
        [InlineData(46, "TrackProgression",     SimHubWire.Float32, 4)]
        [InlineData(49, "CompletedLaps",        SimHubWire.UInt32,  4)]
        [InlineData(52, "CurrentLapTime",       SimHubWire.Double,  8)]
        [InlineData(53, "LastLapTimeSeconds",   SimHubWire.Double,  8)]
        [InlineData(54, "BestLapTimeSeconds",   SimHubWire.Double,  8)]
        public void FieldMatchesSimHub9132(int number, string name, SimHubWire wire, int size)
        {
            var field = Assert.Single(SimHubCourseFeed.Fields, f => f.StandardFieldType == number);
            Assert.Equal((name, wire, size), (field.Name, field.Wire, field.Size));
        }

        [Fact]
        public void DeclaresTheDashBesideTheCourse()
        {
            // A definition that only carried the map would take over the game in SimHub and
            // blank its dashboard; these are what an existing dash reads.
            var names = SimHubCourseFeed.Fields.Select(f => f.Name).ToArray();
            foreach (var dash in new[] { "SpeedKmh", "EngineRpm", "EngineMaxRpm", "Throttle", "Brake", "Clutch",
                                         "Handbrake", "Gear", "VehicleName", "LastLapTimeSeconds", "BestLapTimeSeconds" })
                Assert.Contains(dash, names);
            foreach (var course in new[] { "VehiclePositionNorth", "VehiclePositionEast", "VehiclePositionUp",
                                           "TrackProgression", "EnvironmentId", "TrackLength" })
                Assert.Contains(course, names);
        }

        [Fact]
        public void RoundTripsEveryField()
        {
            var course = Course();
            var buffer = SimHubCourseFeed.CreateBuffer();
            Assert.True(SimHubCourseFeed.TryWrite(buffer, Header(), course, Dash(), Text(), out var error), error);
            Assert.True(SimHubCourseFeed.TryRead(buffer, GameSig, TelemetrySig, out var packet, out error), error);

            var h = packet.Header;
            Assert.Equal((42UL, 1000UL, (byte)1, 12.5), (h.EmitterInstanceId, h.PacketsCounter, h.IsSessionRunning, h.SessionTimeSeconds));
            Assert.Equal((ushort)1, h.LayoutMajorVersion);
            // Taken from the course, not from the header passed in.
            Assert.Equal(course.SessionId, h.SessionId);
            Assert.Equal(3u, h.PhysicsDiscontinuityCounter);
            Assert.Equal((byte)1, h.IsUserInControl);

            var n = packet.Numbers;
            Assert.Equal((float)123.5, n["YawDegrees"]);
            Assert.Equal(course.Position.North, n["VehiclePositionNorth"]);
            Assert.Equal(course.Position.East, n["VehiclePositionEast"]);
            Assert.Equal(course.Position.Up, n["VehiclePositionUp"]);
            Assert.Equal(90.0, n["SpeedKmh"], 3);
            Assert.Equal(6500.0, n["EngineRpm"]);
            Assert.Equal(7800.0, n["EngineMaxRpm"]);
            Assert.Equal((1.0, 0.0, (double)0.2f, 1.0), (n["Throttle"], n["Brake"], n["Clutch"], n["Handbrake"]));
            Assert.Equal(course.LengthM, n["TrackLength"]);
            Assert.Equal((float)course.Progress01, n["TrackProgression"]);
            Assert.Equal(0.0, n["CompletedLaps"]);
            Assert.Equal(41.25, n["CurrentLapTime"]);
            Assert.Equal((95.5, 93.25), (n["LastLapTimeSeconds"], n["BestLapTimeSeconds"]));

            var t = packet.Texts;
            Assert.Equal("dbce.synthetic.hills.stage.fwd", t["EnvironmentId"]);
            Assert.Equal("Hills – Stage", t["EnvironmentName"]);
            Assert.Equal(("Group B Hatch", "car-08", "3"), (t["VehicleName"], t["VehicleId"], t["Gear"]));
        }

        [Fact]
        public void FinishedStageSendsExactlyOne()
        {
            var buffer = SimHubCourseFeed.CreateBuffer();
            Assert.True(SimHubCourseFeed.TryWrite(buffer, Header(), Course(finished: true), Dash(), Text(), out var error), error);
            Assert.True(SimHubCourseFeed.TryRead(buffer, GameSig, TelemetrySig, out var packet, out _));
            Assert.Equal(1.0, packet.Numbers["TrackProgression"]);

            Assert.True(SimHubCourseFeed.TryWrite(buffer, Header(), Course(), Dash(), Text(), out _));
            Assert.True(SimHubCourseFeed.TryRead(buffer, GameSig, TelemetrySig, out packet, out _));
            Assert.True(packet.Numbers["TrackProgression"] < 1.0);
        }

        [Fact]
        public void HeaderFieldsSitAtTheirPackedOffsets()
        {
            var buffer = SimHubCourseFeed.CreateBuffer();
            Assert.True(SimHubCourseFeed.TryWrite(buffer, Header(), Course(), Dash(), Text(), out _));
            Assert.Equal(GameSig, BitConverter.ToUInt32(buffer, 0));
            Assert.Equal(TelemetrySig, BitConverter.ToUInt32(buffer, 4));
            Assert.Equal(1000UL, BitConverter.ToUInt64(buffer, 21));
            Assert.Equal(0xABCDEF0123456789UL, BitConverter.ToUInt64(buffer, 31));
            Assert.Equal(12.5, BitConverter.ToDouble(buffer, 43));
            Assert.Equal(3u, BitConverter.ToUInt32(buffer, 51));
            // The first field starts right after the header.
            Assert.Equal(123.5f, BitConverter.ToSingle(buffer, 55));
        }

        [Fact]
        public void StringsAreZeroTerminatedAndPadded()
        {
            var buffer = SimHubCourseFeed.CreateBuffer();
            Assert.True(SimHubCourseFeed.TryWrite(buffer, Header(), Course(), Dash(), Text(), out _));
            int offset = SimHubCourseFeed.HeaderSize + SimHubCourseFeed.Fields
                .TakeWhile(f => f.Name != "EnvironmentId").Sum(f => f.Size);
            byte[] id = Encoding.UTF8.GetBytes("dbce.synthetic.hills.stage.fwd");
            Assert.Equal(id, buffer.Skip(offset).Take(id.Length).ToArray());
            Assert.All(buffer.Skip(offset + id.Length).Take(128 - id.Length), b => Assert.Equal(0, b));
        }

        [Fact]
        public void RefusesWhatSimHubWouldMisread()
        {
            var buffer = SimHubCourseFeed.CreateBuffer();
            Assert.False(SimHubCourseFeed.TryWrite(new byte[10], Header(), Course(), Dash(), Text(), out _));

            var unfinishedOne = Course(); unfinishedOne.Progress01 = 1;
            Assert.False(SimHubCourseFeed.TryWrite(buffer, Header(), unfinishedOne, Dash(), Text(), out _));

            var longName = Course(); longName.CourseName = new string('x', 256);
            Assert.False(SimHubCourseFeed.TryWrite(buffer, Header(), longName, Dash(), Text(), out var error));
            Assert.Contains("EnvironmentName", error);

            var gear = Text(); gear.Gear = "12345678"; // 8 bytes leaves no terminator
            Assert.False(SimHubCourseFeed.TryWrite(buffer, Header(), Course(), Dash(), gear, out error));
            Assert.Contains("Gear", error);

            var dash = Dash(); dash.CurrentEngineRpm = float.NaN;
            Assert.False(SimHubCourseFeed.TryWrite(buffer, Header(), Course(), dash, Text(), out error));
            Assert.Contains("EngineRpm", error);
        }

        [Fact]
        public void ReaderChecksLengthThenSignaturesLikeSimHub()
        {
            var buffer = SimHubCourseFeed.CreateBuffer();
            Assert.True(SimHubCourseFeed.TryWrite(buffer, Header(), Course(), Dash(), Text(), out _));

            Assert.False(SimHubCourseFeed.TryRead(buffer.Take(buffer.Length - 1).ToArray(), GameSig, TelemetrySig, out _, out var error));
            Assert.Equal("wrong length", error);
            Assert.False(SimHubCourseFeed.TryRead(buffer, GameSig + 1, TelemetrySig, out _, out error));
            Assert.Equal("wrong game signature", error);
            Assert.False(SimHubCourseFeed.TryRead(buffer, GameSig, TelemetrySig + 1, out _, out error));
            Assert.Equal("wrong telemetry signature", error);
        }

        [Theory]
        [InlineData(-1, "R")]
        [InlineData(0, "N")]
        [InlineData(3, "3")]
        [InlineData(12, "12")]
        public void GearIsText(int gear, string expected) => Assert.Equal(expected, SimHubCourseFeed.GearText(gear));

        [Fact]
        public void LongestCourseIdFitsEnvironmentId()
        {
            var course = Course();
            course.CourseId = CourseIdentity.Create(new string('a', 39), new string('b', 39), new string('c', 38), CourseDirection.Forward).Id;
            Assert.True(SimHubCourseFeed.TryWrite(SimHubCourseFeed.CreateBuffer(), Header(), course, Dash(), Text(), out var error), error);
        }
    }
}
