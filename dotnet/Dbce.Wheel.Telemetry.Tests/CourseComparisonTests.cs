using System;
using System.Collections.Generic;
using System.Linq;
using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests
{
    /// <summary>The offline comparison a route capture is judged with, on synthetic drives with known answers.</summary>
    public class CourseComparisonTests
    {
        private static CoursePath Stage() => CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);

        [Fact]
        public void CleanDriveAgreesAndRecoversTheGamesUnit()
        {
            var path = Stage();
            var drive = SyntheticCourses.Drive(path, 0, 1500);
            // A game that counts progress in half-metres: the comparison must report scale 0.5, not a 750 m error.
            var game = drive.Select(d => d.TrueDistanceM * 2).ToList();

            var r = CourseComparison.Run(path, drive.Select(d => d.Position).ToList(), game);

            Assert.True(r.FixOnFirstSample);
            Assert.Equal((0, 0, 0, 0), (r.Discontinuities, r.OffCourse, r.Invalid, r.UnflaggedBackwardSteps));
            Assert.True(r.OffsetP95M <= 1.6, "p95 offset " + r.OffsetP95M);
            Assert.InRange(r.CourseDistanceM, 1498, 1502);
            Assert.True(r.PlanPathM >= r.CourseDistanceM * 0.99 && r.PlanPathM <= r.CourseDistanceM * 1.02, "plan path " + r.PlanPathM);
            Assert.Equal(0.5, r.GameScale.Value, 2);
            Assert.True(r.GameDeltaMedianM < 1 && r.GameDeltaP95M < 1.5, "game delta " + r.GameDeltaMedianM + " / " + r.GameDeltaP95M);
        }

        [Fact]
        public void ResetIsCountedAndItsJumpIsNotDistance()
        {
            var path = Stage();
            var positions = SyntheticCourses.Drive(path, 0, 1000).Select(d => d.Position)
                .Concat(SyntheticCourses.Drive(path, 800, 1200).Select(d => d.Position)).ToList();

            var r = CourseComparison.Run(path, positions);

            Assert.Equal(1, r.Discontinuities);
            Assert.InRange(r.CourseDistanceM, 1398, 1402); // 1000 driven, the 200 m reset back skipped, 400 driven
            Assert.Null(r.GameScale);
        }

        [Fact]
        public void BadAndOffCourseSamplesAreCountedNotMeasured()
        {
            var path = Stage();
            var positions = SyntheticCourses.Drive(path, 0, 300).Select(d => d.Position).ToList();
            positions.Insert(100, new CourseVector(double.NaN, 0, 0));
            positions.Insert(200, SyntheticCourses.Beside(path, 150, 80));

            var r = CourseComparison.Run(path, positions);

            Assert.Equal(1, r.Invalid);
            Assert.Equal(1, r.OffCourse);
            Assert.Equal(0, r.Discontinuities);
            Assert.True(r.OffsetMaxM < 2, "an off-course sample never enters the offset statistics");
        }

        [Fact]
        public void GameProgressNeedsOneValuePerPosition()
        {
            var path = Stage();
            var positions = new List<CourseVector> { path.PointAt(0), path.PointAt(1) };
            Assert.Throws<ArgumentException>(() => CourseComparison.Run(path, positions, new List<double> { 0 }));
        }
    }
}
