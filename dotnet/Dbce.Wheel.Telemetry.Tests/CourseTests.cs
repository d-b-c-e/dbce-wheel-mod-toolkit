using System;
using System.IO;
using System.Linq;
using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests
{
    /// <summary>Course identity: stable, direction-aware, file-name safe, and strict about what it accepts.</summary>
    public class CourseIdentityTests
    {
        [Fact]
        public void IdHasTheDocumentedShapeAndRoundTrips()
        {
            var id = CourseIdentity.Create("woden", "kenya-ss1", "stage", CourseDirection.Forward);

            Assert.Equal("dbce.woden.kenya-ss1.stage.fwd", id.Id);
            Assert.True(CourseIdentity.TryParse(id.Id, out var parsed));
            Assert.Equal(("woden", "kenya-ss1", "stage", CourseDirection.Forward, (string)null),
                (parsed.Game, parsed.Venue, parsed.Route, parsed.Direction, parsed.ContentHash));
        }

        [Fact]
        public void DirectionIsPartOfTheIdentity()
        {
            var forward = CourseIdentity.Create("art", "finland", "stage-3", CourseDirection.Forward);
            var reverse = CourseIdentity.Create("art", "finland", "stage-3", CourseDirection.Reverse);

            Assert.NotEqual(forward.Id, reverse.Id);
            Assert.EndsWith(".rev", reverse.Id);
        }

        [Fact]
        public void ContentHashFollowsTheExactRoute()
        {
            var points = SyntheticCourses.Stage();
            string hash = CourseIdentity.HashPath(points);

            Assert.Equal(hash, CourseIdentity.HashPath(SyntheticCourses.Stage()));
            Assert.Matches("^[0-9a-f]{16}$", hash);

            var moved = SyntheticCourses.Stage();
            moved[200] = new CourseVector(moved[200].North + 0.01, moved[200].East, moved[200].Up);
            Assert.NotEqual(hash, CourseIdentity.HashPath(moved));

            var reversed = SyntheticCourses.Stage();
            reversed.Reverse();
            Assert.NotEqual(hash, CourseIdentity.HashPath(reversed));

            var custom = CourseIdentity.Create("art", "custom", "user-stage", CourseDirection.Forward, hash);
            Assert.True(CourseIdentity.TryParse(custom.Id, out var parsed));
            Assert.Equal(hash, parsed.ContentHash);
        }

        [Theory]
        [InlineData(null)]
        [InlineData("")]
        [InlineData("Kenya")]
        [InlineData("kenya ss1")]
        [InlineData("kenya.ss1")]
        [InlineData("kenya:ss1")]
        [InlineData("-kenya")]
        [InlineData("kenya-")]
        [InlineData("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")] // 41 characters
        public void RejectsTokensThatAreNotFileNameSafeLowercase(string venue)
        {
            Assert.False(CourseIdentity.TryCreate("woden", venue, "stage", CourseDirection.Forward, null, out _, out var error));
            Assert.Contains("venue", error);
        }

        [Theory]
        [InlineData("0123456789ABCDEF")]
        [InlineData("0123456789abcde")]
        [InlineData("0123456789abcdeg")]
        public void RejectsMalformedHashes(string hash)
        {
            Assert.False(CourseIdentity.TryCreate("art", "custom", "x", CourseDirection.Forward, hash, out _, out _));
        }

        [Fact]
        public void IdMustFitSimHubsEnvironmentId()
        {
            string forty = new string('a', 40);
            Assert.False(CourseIdentity.TryCreate(forty, forty, forty, CourseDirection.Forward, null, out _, out var error));
            Assert.Contains("127", error);

            // 5 + 39 + 1 + 39 + 1 + 38 + 4 = 127 bytes exactly.
            var longest = CourseIdentity.Create(new string('a', 39), new string('b', 39), new string('c', 38), CourseDirection.Forward);
            Assert.Equal(127, longest.Id.Length);
        }

        [Fact]
        public void IdsAreValidFileNamesUnchanged()
        {
            // SimHub stores a map as <id>.shtl after splitting the id on invalid file-name characters.
            var id = CourseIdentity.Create("iracing-arcade", "aragon", "gp-circuit", CourseDirection.Reverse,
                CourseIdentity.HashPath(SyntheticCourses.Circuit()));
            Assert.Equal(-1, id.Id.IndexOfAny(Path.GetInvalidFileNameChars()));
            Assert.Equal(id.Id, string.Join("_", id.Id.Split(Path.GetInvalidFileNameChars())));
        }

        [Theory]
        [InlineData("Kenya SS1", "kenya-ss1")]
        [InlineData("Rally Iberia – Campdasens", "rally-iberia-campdasens")]
        [InlineData("Holzberg_Level", "holzberg-level")]
        [InlineData("  ", null)]
        public void SlugMakesTokensFromStableNames(string text, string expected)
        {
            Assert.Equal(expected, CourseIdentity.Slug(text));
            if (expected != null) Assert.True(CourseIdentity.IsToken(expected));
        }
    }

    /// <summary>Course paths: lengths, loops, direction, and the inputs they refuse.</summary>
    public class CoursePathTests
    {
        private static CourseVector V(double n, double e, double u = 0) => new CourseVector(n, e, u);

        [Fact]
        public void LengthsOfKnownShapes()
        {
            Assert.Equal(1000, CoursePath.Create(new[] { V(0, 0), V(1000, 0) }, isLoop: false).LengthM, 9);

            var square = new[] { V(0, 0), V(100, 0), V(100, 100), V(0, 100) };
            Assert.Equal(400, CoursePath.Create(square, isLoop: true).LengthM, 9);
            Assert.Equal(400, CoursePath.Create(square.Concat(new[] { V(0, 0) }).ToArray(), isLoop: true).LengthM, 9);
            Assert.Equal(300, CoursePath.Create(square, isLoop: false).LengthM, 9);
        }

        [Fact]
        public void ConsecutiveDuplicatesAreDropped()
        {
            var path = CoursePath.Create(new[] { V(0, 0), V(0, 0), V(50, 0), V(50.001, 0), V(100, 0) }, isLoop: false);
            Assert.Equal(3, path.PointCount);
            Assert.Equal(100, path.LengthM, 9);
        }

        [Fact]
        public void RefusesInputsThatCannotBeACourse()
        {
            Assert.False(CoursePath.TryCreate(null, false, out _, out _));
            Assert.False(CoursePath.TryCreate(new[] { V(0, 0) }, false, out _, out _));
            Assert.False(CoursePath.TryCreate(new[] { V(0, 0), V(double.NaN, 0) }, false, out _, out var nan));
            Assert.Contains("not finite", nan);
            Assert.False(CoursePath.TryCreate(new[] { V(0, 0), V(0, 0), V(0, 0) }, false, out _, out _));
            Assert.False(CoursePath.TryCreate(new[] { V(0, 0), V(100, 0) }, true, out _, out var loop));
            Assert.Contains("3 distinct", loop);
            // 5 m is far more likely a wrong unit (kilometres, centimetres) than a course.
            Assert.False(CoursePath.TryCreate(new[] { V(0, 0), V(5, 0) }, false, out _, out var tiny));
            Assert.Contains("at least", tiny);
        }

        [Fact]
        public void PointAtWrapsOnALoopAndClampsOtherwise()
        {
            var square = new[] { V(0, 0), V(100, 0), V(100, 100), V(0, 100) };
            var loop = CoursePath.Create(square, isLoop: true);
            var open = CoursePath.Create(square, isLoop: false);

            Assert.Equal(50, loop.PointAt(450).North, 9);
            Assert.Equal(50, loop.PointAt(-350).North, 9);
            Assert.Equal(0, open.PointAt(500).North, 9);
            Assert.Equal(100, open.PointAt(500).East, 9);
            Assert.Equal(0, open.PointAt(-5).North, 9);
        }

        [Fact]
        public void ReversedPathIsTheSameRoadDrivenTheOtherWay()
        {
            var stage = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var back = stage.Reversed();
            Assert.Equal(stage.LengthM, back.LengthM, 6);
            Assert.Equal(stage[stage.PointCount - 1].North, back[0].North, 9);

            var circuit = CoursePath.Create(SyntheticCourses.Circuit(), isLoop: true);
            var other = circuit.Reversed();
            Assert.Equal(circuit[0].East, other[0].East, 9);
            Assert.Equal(circuit[circuit.PointCount - 1].North, other[1].North, 9);
        }

        [Fact]
        public void ProjectionTellsABridgeFromTheRoadBeneath()
        {
            // Two roads crossing in plan view, the second 8 m higher; a car on the bridge is on it.
            var path = CoursePath.Create(new[] { V(-100, 0, 0), V(100, 0, 0), V(100, 100, 8), V(0, 100, 8), V(0, -100, 8) }, isLoop: false);
            var onBridge = path.Project(V(0, 0, 8.5));
            Assert.True(onBridge.DistanceM > 400, "projected onto the lower road at " + onBridge.DistanceM);
            Assert.True(onBridge.OffsetM < 1);
        }
    }

    /// <summary>Progress, laps, completion and every kind of bad sample, on synthetic drives.</summary>
    public class CourseTrackerTests
    {
        [Fact]
        public void StageReportsExactlyOneOnlyAtTheFinish()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            double previous = -1;
            bool finished = false;

            foreach (var (truth, position) in SyntheticCourses.Drive(path, 0, path.LengthM))
            {
                var p = tracker.Update(position);
                Assert.True(p.HasFix);
                Assert.Equal(CourseFlags.None, p.Flags & ~CourseFlags.Finished);
                if (!p.Finished)
                {
                    Assert.True((float)p.Progress01 < 1f, "unfinished stage reported 1 at " + truth);
                    Assert.True(Math.Abs(p.DistanceM - truth) < 1.0, "distance " + p.DistanceM + " vs " + truth);
                }
                Assert.True(p.Progress01 >= previous - 0.05 / path.LengthM, "progress went backwards at " + truth);
                previous = p.Progress01;
                finished = p.Finished;
            }
            Assert.True(finished);
            Assert.Equal(1.0, previous);
        }

        [Fact]
        public void FinishedStageHoldsOneUntilRestart()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            foreach (var (_, position) in SyntheticCourses.Drive(path, path.LengthM - 300, path.LengthM)) tracker.Update(position);

            // Rolling on past the finish board, then reversing a little: still finished.
            Assert.Equal(1.0, tracker.Update(path.PointAt(path.LengthM - 10)).Progress01);

            uint before = tracker.Update(path.PointAt(path.LengthM - 10)).DiscontinuityCount;
            tracker.Restart();
            var first = tracker.Update(path.PointAt(5));
            Assert.False(first.Finished);
            Assert.Equal(before + 1, first.DiscontinuityCount);
            Assert.True(first.Progress01 < 0.01);
        }

        [Fact]
        public void CircuitCountsTwoLapsAndNeverReportsOne()
        {
            var path = CoursePath.Create(SyntheticCourses.Circuit(), isLoop: true);
            var tracker = new CourseTracker(path);
            int lapFlags = 0;
            CourseProgress p = default;

            foreach (var (_, position) in SyntheticCourses.Drive(path, 5, 2 * path.LengthM + 100))
            {
                p = tracker.Update(position);
                Assert.True(p.Progress01 >= 0 && p.Progress01 < 1, "loop progress " + p.Progress01);
                Assert.Equal(CourseFlags.None, p.Flags & (CourseFlags.Discontinuity | CourseFlags.WrongWay | CourseFlags.OffCourse));
                if ((p.Flags & CourseFlags.LapCompleted) != 0) lapFlags++;
            }
            Assert.Equal(2, lapFlags);
            Assert.Equal(2u, p.CompletedLaps);
        }

        [Fact]
        public void GridBehindTheLineStartsLapZero()
        {
            var path = CoursePath.Create(SyntheticCourses.Circuit(), isLoop: true);
            var tracker = new CourseTracker(path);
            CourseProgress p = default;
            foreach (var (_, position) in SyntheticCourses.Drive(path, path.LengthM - 30, path.LengthM + 100)) p = tracker.Update(position);
            Assert.Equal(0u, p.CompletedLaps);

            foreach (var (_, position) in SyntheticCourses.Drive(path, 100, path.LengthM + 50)) p = tracker.Update(position);
            Assert.Equal(1u, p.CompletedLaps);
        }

        [Fact]
        public void ReversingOverTheLineCannotCountALapTwice()
        {
            var path = CoursePath.Create(SyntheticCourses.Circuit(), isLoop: true);
            var tracker = new CourseTracker(path);
            CourseProgress p = default;
            foreach (var (_, position) in SyntheticCourses.Drive(path, 5, path.LengthM + 20)) p = tracker.Update(position);
            Assert.Equal(1u, p.CompletedLaps);

            bool wrongWay = false;
            foreach (var (_, position) in SyntheticCourses.Drive(path, path.LengthM + 20, path.LengthM - 20, speedMps: 5))
            {
                p = tracker.Update(position);
                wrongWay |= (p.Flags & CourseFlags.WrongWay) != 0;
                Assert.Equal(CourseFlags.None, p.Flags & CourseFlags.Discontinuity);
            }
            Assert.True(wrongWay);

            foreach (var (_, position) in SyntheticCourses.Drive(path, path.LengthM - 20, path.LengthM + 60)) p = tracker.Update(position);
            Assert.Equal(1u, p.CompletedLaps);
            Assert.Equal(CourseFlags.None, p.Flags & CourseFlags.WrongWay);
        }

        [Fact]
        public void ResetBackOnAStageIsOneDiscontinuity()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            foreach (var (_, position) in SyntheticCourses.Drive(path, 0, 1000)) tracker.Update(position);

            var reset = tracker.Update(path.PointAt(800));
            Assert.Equal(CourseFlags.Discontinuity, reset.Flags);
            Assert.Equal(1u, reset.DiscontinuityCount);
            Assert.Equal(800 / path.LengthM, reset.Progress01, 6);

            foreach (var (_, position) in SyntheticCourses.Drive(path, 800, 900))
                Assert.Equal(CourseFlags.None, tracker.Update(position).Flags);
        }

        [Fact]
        public void ResetBackOverTheLineDoesNotRecountTheLap()
        {
            var path = CoursePath.Create(SyntheticCourses.Circuit(), isLoop: true);
            var tracker = new CourseTracker(path);
            CourseProgress p = default;
            foreach (var (_, position) in SyntheticCourses.Drive(path, 5, path.LengthM + 50)) p = tracker.Update(position);
            Assert.Equal(1u, p.CompletedLaps);

            p = tracker.Update(path.PointAt(path.LengthM - 100));
            Assert.Equal(CourseFlags.Discontinuity, p.Flags);

            foreach (var (_, position) in SyntheticCourses.Drive(path, path.LengthM - 100, path.LengthM + 100)) p = tracker.Update(position);
            Assert.Equal(1u, p.CompletedLaps);
        }

        [Fact]
        public void TeleportForwardOverTheLineIsNotALap()
        {
            var path = CoursePath.Create(SyntheticCourses.Circuit(), isLoop: true);
            var tracker = new CourseTracker(path);
            CourseProgress p = default;
            foreach (var (_, position) in SyntheticCourses.Drive(path, 5, path.LengthM - 100)) p = tracker.Update(position);

            p = tracker.Update(path.PointAt(200));
            Assert.Equal(CourseFlags.Discontinuity, p.Flags);
            Assert.Equal(0u, p.CompletedLaps);

            foreach (var (_, position) in SyntheticCourses.Drive(path, 200, path.LengthM + 50)) p = tracker.Update(position);
            Assert.Equal(1u, p.CompletedLaps);
        }

        [Fact]
        public void InvalidSamplesChangeNothing()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            var before = tracker.Update(path.PointAt(500));

            var bad = tracker.Update(new CourseVector(double.NaN, 0, 0));
            Assert.Equal(CourseFlags.InvalidInput, bad.Flags);
            Assert.Equal(before.Progress01, bad.Progress01);
            Assert.Equal(CourseFlags.InvalidInput, tracker.Update(new CourseVector(0, double.PositiveInfinity, 0)).Flags);

            var after = tracker.Update(path.PointAt(501));
            Assert.Equal(CourseFlags.None, after.Flags);
            Assert.Equal(before.DiscontinuityCount, after.DiscontinuityCount);
        }

        [Fact]
        public void OffCourseHoldsProgressAndReturnsWithoutAJump()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            var on = tracker.Update(path.PointAt(600));

            var off = tracker.Update(SyntheticCourses.Beside(path, 610, 60));
            Assert.Equal(CourseFlags.OffCourse, off.Flags);
            Assert.Equal(on.Progress01, off.Progress01);

            var back = tracker.Update(path.PointAt(620));
            Assert.Equal(CourseFlags.None, back.Flags);
            Assert.Equal(620, back.DistanceM, 3);
        }

        [Fact]
        public void NoFixUntilACarIsNearTheCourse()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);

            var far = tracker.Update(SyntheticCourses.Beside(path, 100, 100));
            Assert.Equal(CourseFlags.NoFix | CourseFlags.OffCourse, far.Flags);
            Assert.Equal(0, far.Progress01);

            Assert.True(tracker.Update(path.PointAt(100)).HasFix);
        }

        [Fact]
        public void CrossingStaysOnTheBranchBeingDriven()
        {
            var path = CoursePath.Create(SyntheticCourses.FigureEight(), isLoop: true);
            var tracker = new CourseTracker(path);
            double previous = double.NaN;
            int globalWouldJump = 0;

            // A car 2 m to one side of the line: near the crossing it is closer to the other branch.
            for (double truth = 5; truth <= path.LengthM - 5; truth += 0.5)
            {
                var position = SyntheticCourses.Beside(path, truth, 2.0);
                var p = tracker.Update(position);
                Assert.Equal(CourseFlags.None, p.Flags);
                Assert.True(Math.Abs(p.DistanceM - truth) < 1.0, "tracker at " + p.DistanceM + " vs " + truth);
                if (!double.IsNaN(previous)) Assert.True(Math.Abs(p.DistanceM - previous) < 2.0);
                previous = p.DistanceM;
                if (Math.Abs(path.Project(position).DistanceM - truth) > 100) globalWouldJump++;
            }
            // The test has teeth: a whole-course search would have jumped branches at the crossing.
            Assert.True(globalWouldJump > 0);
        }

        [Fact]
        public void ReverseCourseRunsFromItsOwnStart()
        {
            var forward = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var reverse = forward.Reversed();
            var reverseId = CourseIdentity.Create("synthetic", "hills", "stage", CourseDirection.Reverse, CourseIdentity.HashPath(reverse.ToArray()));
            Assert.NotEqual(CourseIdentity.Create("synthetic", "hills", "stage", CourseDirection.Forward, CourseIdentity.HashPath(forward.ToArray())).Id, reverseId.Id);

            // Driving the road from its old finish to its old start completes the reverse stage.
            var tracker = new CourseTracker(reverse);
            CourseProgress p = default;
            foreach (var (_, position) in SyntheticCourses.Drive(forward, forward.LengthM, 0)) p = tracker.Update(position);
            Assert.True(p.Finished);
            Assert.Equal(1.0, p.Progress01);

            // Driving it the usual way on the reverse course is wrong-way and never finishes.
            var wrong = new CourseTracker(reverse);
            bool wrongWay = false;
            foreach (var (_, position) in SyntheticCourses.Drive(forward, 0, 300))
            {
                p = wrong.Update(position);
                wrongWay |= (p.Flags & CourseFlags.WrongWay) != 0;
            }
            Assert.True(wrongWay);
            Assert.False(p.Finished);
        }
    }

    /// <summary>Axis conventions and the sample's own invariants.</summary>
    public class CourseAxesAndSampleTests
    {
        [Fact]
        public void UnityAxesMapToNorthEastUp()
        {
            var p = CourseAxes.FromUnity(1, 2, 3);
            Assert.Equal((3.0, 1.0, 2.0), (p.North, p.East, p.Up));
            Assert.Equal(new[] { -3.0, 2.0, 1.0 }, CourseAxes.ToSimHubCarCoordinates(p));

            // Unity yaw 0 faces +Z (north) and 90 faces +X (east): SimHub's compass, unchanged.
            Assert.Equal(0, CourseAxes.YawFromUnity(0));
            Assert.Equal(90, CourseAxes.YawFromUnity(90));
            Assert.Equal(270, CourseAxes.YawFromUnity(-90));
            Assert.Equal(0, CourseAxes.YawFromUnity(360));
            Assert.Equal(10, CourseAxes.YawFromUnity(730), 9);
        }

        private static CourseSample Valid()
        {
            var path = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
            var tracker = new CourseTracker(path);
            var progress = tracker.Update(path.PointAt(700));
            return CourseSample.From(CourseIdentity.Create("synthetic", "hills", "stage", CourseDirection.Forward),
                "Hills", path, progress, path.PointAt(700), 45, 30.5, 7);
        }

        [Fact]
        public void SampleFromTrackerIsValid()
        {
            var sample = Valid();
            Assert.True(sample.TryValidate(out var error), error);
            Assert.Equal("dbce.synthetic.hills.stage.fwd", sample.CourseId);
            Assert.False(sample.IsLoop);
        }

        [Fact]
        public void SampleRejectsBrokenInvariants()
        {
            var s = Valid(); s.CourseId = "Hills";           Assert.False(s.TryValidate(out _));
            s = Valid(); s.Progress01 = 1;                    Assert.False(s.TryValidate(out _)); // 1 without finishing
            s = Valid(); s.Finished = true;                   Assert.False(s.TryValidate(out _)); // finished below 1
            s = Valid(); s.IsLoop = true; s.Progress01 = 1; s.Finished = true; Assert.False(s.TryValidate(out _));
            s = Valid(); s.HasFix = false;                    Assert.False(s.TryValidate(out _)); // progress without a fix
            s = Valid(); s.YawDeg = 360;                      Assert.False(s.TryValidate(out _));
            s = Valid(); s.Position.Up = double.NaN;          Assert.False(s.TryValidate(out _));
            s = Valid(); s.LengthM = 5;                       Assert.False(s.TryValidate(out _));
            s = Valid(); s.LapTimeS = -1;                     Assert.False(s.TryValidate(out _));
        }
    }
}
