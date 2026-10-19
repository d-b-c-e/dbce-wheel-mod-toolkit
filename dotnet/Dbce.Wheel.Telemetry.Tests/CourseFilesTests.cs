using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using Dbce.Wheel.Course;
using Dbce.Wheel.Playback;
using Dbce.Wheel.Telemetry;
using Xunit;
using Xunit.Abstractions;

namespace Dbce.Wheel.Telemetry.Tests
{
    /// <summary>
    /// Synthetic route captures in the exact shape Woden's CourseRouteCapture writes (route.json fields,
    /// progress.tsv columns, dbce-playback-seal-v1), so the reader is checked without any owner capture.
    /// </summary>
    internal sealed class SyntheticCapture : IDisposable
    {
        public const string Build = "f422894d8d2b0df4edb7e5259e5e60cb8c4f8dea2e85ebdfc09dd6766349250c";
        public readonly string Root = Path.Combine(Path.GetTempPath(), "dbce-course-files-" + Guid.NewGuid().ToString("N"));
        public readonly CoursePath Course;

        public SyntheticCapture()
        {
            Directory.CreateDirectory(Root);
            Course = CoursePath.Create(SyntheticCourses.Stage(), isLoop: false);
        }

        public static float[] Unity(CourseVector p) => new[] { (float)p.East, (float)p.Up, (float)p.North };

        public JsonObject Route(int rows)
        {
            var points = new JsonArray();
            foreach (var p in Course.ToArray()) points.Add(new JsonArray(Unity(p).Select(v => (JsonNode)JsonValue.Create(v)).ToArray()));
            var distances = new JsonArray();
            for (int i = 0; i < Course.PointCount; i++) distances.Add(JsonValue.Create(i * 5.0f));
            return new JsonObject
            {
                ["schema"] = "dbce.course-route@1", ["game"] = "Super Woden Rally Edge", ["requestId"] = Guid.NewGuid().ToString("N"),
                ["gameAssemblySha256"] = Build, ["trackAsset"] = "Kenya SS1", ["trackName"] = "Kenya SS1", ["reverseTrack"] = false,
                ["totalLaps"] = 1, ["isClosedCircuit"] = false, ["length"] = (float)Course.LengthM, ["kms"] = (float)(Course.LengthM / 1000),
                ["unitConversionFactor"] = 1f, ["frame"] = "unity-world", ["axes"] = "x east, y up, z north (toolkit CourseAxes.FromUnity)",
                ["waypointCount"] = Course.PointCount, ["waypoints"] = points, ["distances"] = distances,
                ["circuitSameAsStageProgress"] = true, ["state"] = "Completed", ["reason"] = "car END", ["rows"] = rows,
                ["skippedRows"] = 0, ["capturedUtc"] = "2026-10-19T03:00:00.0000000Z",
            };
        }

        /// <summary>Game progress = true distance along the route times <paramref name="unit"/>.</summary>
        public List<string> Rows(double toM, double unit = 1)
        {
            var rows = new List<string>();
            int i = 0;
            foreach (var (truth, position) in SyntheticCourses.Drive(Course, 0, toM))
            {
                var u = Unity(position);
                rows.Add(string.Join("\t", (1000 + i).ToString(CultureInfo.InvariantCulture), (i / 60.0).ToString("R", CultureInfo.InvariantCulture),
                    F(u[0]), F(u[1]), F(u[2]), F((float)(truth * unit)), F((float)(truth / Course.LengthM)), "RACE"));
                i++;
            }
            return rows;
        }

        /// <summary>Writes and seals a capture folder; edits run on the text before sealing, so the seal is valid.</summary>
        public string Write(string name, JsonObject route, IList<string> rows, Func<string, string> editRoute = null, Func<string, string> editProgress = null)
        {
            string dir = System.IO.Path.Combine(Root, name);
            Directory.CreateDirectory(dir);
            string routeText = route.ToJsonString();
            string progressText = "tick\tsim_s\tpx\tpy\tpz\tprogress_distance\tplayer_percentage\tstatus\n" + string.Concat(rows.Select(r => r + "\n"));
            File.WriteAllText(System.IO.Path.Combine(dir, "route.json"), editRoute == null ? routeText : editRoute(routeText), new UTF8Encoding(false));
            File.WriteAllText(System.IO.Path.Combine(dir, "progress.tsv"), editProgress == null ? progressText : editProgress(progressText), new UTF8Encoding(false));
            ArtifactSeal.Complete(dir, new[] { "route.json", "progress.tsv" });
            return dir;
        }

        public string Valid(string name = "valid", double toM = 900)
        {
            var rows = Rows(toM);
            return Write(name, Route(rows.Count), rows);
        }

        private static string F(float v) => v.ToString("R", CultureInfo.InvariantCulture);

        public void Dispose()
        {
            if (Directory.Exists(Root)) Directory.Delete(Root, recursive: true);
        }
    }

    /// <summary>Reader and comparison glue for route captures and recorded trajectories.</summary>
    public class CourseFilesTests
    {
        [Fact]
        public void ReadsACaptureAndMapsUnityAxesOnce()
        {
            using var capture = new SyntheticCapture();
            var route = capture.Route(1);
            route["waypoints"][0] = new JsonArray(1f, 2f, 3f);
            string dir = capture.Write("axes", route, new[] { "7\t0.5\t10\t20\t30\t1.5\t\tRACE" });

            var file = CourseRouteFile.Read(dir);

            Assert.Equal((3.0, 1.0, 2.0), (file.Waypoints[0].North, file.Waypoints[0].East, file.Waypoints[0].Up));
            var row = Assert.Single(file.Progress);
            Assert.Equal((30.0, 10.0, 20.0), (row.Position.North, row.Position.East, row.Position.Up));
            Assert.Equal((7L, 0.5, 1.5, (double?)null, "RACE"), (row.Tick, row.SimSeconds, row.ProgressDistance, row.PlayerPercentage, row.Status));
            Assert.Equal(("Kenya SS1", false, false, 1), (file.TrackAsset, file.ReverseTrack, file.IsClosedCircuit, file.TotalLaps));
            Assert.Equal(SyntheticCapture.Build, file.GameAssemblySha256);
            Assert.Equal(DateTimeKind.Utc, file.CapturedUtc.Kind);
        }

        [Fact]
        public void ReadingChangesNothingInTheFolder()
        {
            using var capture = new SyntheticCapture();
            string dir = capture.Valid();
            var before = Directory.GetFiles(dir).ToDictionary(f => f, f => (File.ReadAllBytes(f), File.GetLastWriteTimeUtc(f)));

            CourseRouteFile.Read(dir);

            var after = Directory.GetFiles(dir);
            Assert.Equal(before.Keys.OrderBy(k => k), after.OrderBy(k => k));
            foreach (var f in after)
            {
                Assert.Equal(before[f].Item1, File.ReadAllBytes(f));
                Assert.Equal(before[f].Item2, File.GetLastWriteTimeUtc(f));
            }
        }

        [Fact]
        public void OwnDriveRecoversTheGamesUnitOnASyntheticCapture()
        {
            using var capture = new SyntheticCapture();
            var rows = capture.Rows(1500, unit: 0.5);
            var file = CourseRouteFile.Read(capture.Write("own", capture.Route(rows.Count), rows));

            var r = CourseCaptureCheck.OwnDrive(file);

            Assert.True(r.FixOnFirstSample);
            Assert.Equal((0, 0, 0), (r.Discontinuities, r.OffCourse, r.UnflaggedBackwardSteps));
            Assert.Equal(2.0, r.GameScale.Value, 2);
            Assert.True(r.GameDeltaMedianM < 1 && r.GameDeltaP95M < 1.5, "game delta " + r.GameDeltaMedianM + " / " + r.GameDeltaP95M);
        }

        [Fact]
        public void RecordedTrajectoryIsJudgedOnTheCapturedRoute()
        {
            using var capture = new SyntheticCapture();
            var file = CourseRouteFile.Read(capture.Valid());
            string tapeDir = Path.Combine(capture.Root, "tape");
            var meta = new Dictionary<string, string>
            {
                ["contract"] = "dbce.trajectory@1", ["game"] = "Super Woden Rally Edge", ["gameSha256"] = SyntheticCapture.Build,
                ["scenario"] = "Kenya SS1|car=8", ["fixedDeltaTime"] = "0.02", ["captureSource"] = "synthetic", ["physicalOutput"] = "false",
            };
            using (var recorder = new TrajectoryRecorder(tapeDir, meta))
            {
                int i = 0;
                foreach (var (_, position) in SyntheticCourses.Drive(capture.Course, 0, 1200, speedMps: 25, hz: 50))
                {
                    var frame = new TrajectoryFrame { Time = i++ * 0.02 };
                    var u = SyntheticCapture.Unity(position);
                    frame.Values[0] = u[0]; frame.Values[1] = u[1]; frame.Values[2] = u[2]; frame.Values[6] = 1;
                    recorder.Write(frame);
                }
                recorder.Complete();
            }

            var tape = TrajectoryTape.Read(tapeDir);
            var r = CourseCaptureCheck.Trajectory(file, tape);

            Assert.True(CourseCaptureCheck.SameBuild(file, tape));
            Assert.True(r.FixOnFirstSample);
            Assert.Equal((0, 0), (r.Discontinuities, r.UnflaggedBackwardSteps));
            Assert.True(r.OffsetP95M <= 1.6, "p95 offset " + r.OffsetP95M);
            Assert.True(Math.Abs(r.CourseDistanceM - r.PlanPathM) <= 0.05 * r.PlanPathM, $"course {r.CourseDistanceM} vs plan {r.PlanPathM}");
        }

        public static IEnumerable<object[]> BadRoutes() => new[]
        {
            new object[] { (Action<JsonObject>)(r => r["schema"] = "dbce.course-route@2"), "schema" },
            new object[] { (Action<JsonObject>)(r => r["frame"] = "north-east-up"), "frame" },
            new object[] { (Action<JsonObject>)(r => r["waypointCount"] = 3), "waypointCount" },
            new object[] { (Action<JsonObject>)(r => { r["waypoints"] = new JsonArray(new JsonArray(1f, 2f, 3f)); r["waypointCount"] = 1; }), "waypoints must number" },
            new object[] { (Action<JsonObject>)(r => r["waypoints"][1] = new JsonArray(1f, 2f)), "waypoint 1 is not [x, y, z]" },
            new object[] { (Action<JsonObject>)(r => r["waypoints"][2] = new JsonArray(1f, "2", 3f)), "waypoint 2 is not a finite float" },
            new object[] { (Action<JsonObject>)(r => r["waypoints"][3] = new JsonArray(1f, 1e300, 3f)), "waypoint 3 is not a finite float" },
            new object[] { (Action<JsonObject>)(r => r.Remove("trackAsset")), "trackAsset" },
            new object[] { (Action<JsonObject>)(r => r["gameAssemblySha256"] = "abc"), "gameAssemblySha256" },
            new object[] { (Action<JsonObject>)(r => r["state"] = "Faulted"), "state Faulted" },
            new object[] { (Action<JsonObject>)(r => r["rows"] = 2), "declares 2" },
            new object[] { (Action<JsonObject>)(r => r["reverseTrack"] = "false"), "reverseTrack" },
            new object[] { (Action<JsonObject>)(r => r["capturedUtc"] = "2026-10-19 03:00"), "capturedUtc" },
            new object[] { (Action<JsonObject>)(r => r.Remove("circuitSameAsStageProgress")), "circuitSameAsStageProgress" },
        };

        [Theory]
        [MemberData(nameof(BadRoutes))]
        public void RefusesMalformedRoutes(Action<JsonObject> edit, string expected)
        {
            using var capture = new SyntheticCapture();
            var rows = capture.Rows(30);
            var route = capture.Route(rows.Count);
            edit(route);
            string dir = capture.Write("bad", route, rows);

            var ex = Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(dir));
            Assert.Contains(expected, ex.Message);
        }

        [Theory]
        [InlineData("dup", "repeats schema")]
        [InlineData("comment", "not valid JSON")]
        [InlineData("comma", "not valid JSON")]
        [InlineData("array", "root is not an object")]
        public void RefusesRouteTextThatIsNotOneCleanObject(string kind, string expected)
        {
            using var capture = new SyntheticCapture();
            var rows = capture.Rows(30);
            Func<string, string> edit = kind switch
            {
                "dup" => t => "{\"schema\":\"x\"," + t.Substring(1),
                "comment" => t => "/* c */" + t,
                "comma" => t => t.Substring(0, t.Length - 1) + ",}",
                _ => t => "[" + t + "]",
            };
            string dir = capture.Write("text", capture.Route(rows.Count), rows, editRoute: edit);

            var ex = Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(dir));
            Assert.Contains(expected, ex.Message);
        }

        [Theory]
        [InlineData("header", "header differs")]
        [InlineData("columns", "columns")]
        [InlineData("nan", "px is not a finite number")]
        [InlineData("backwards", "goes back in time")]
        [InlineData("status", "has no status")]
        [InlineData("blank", "columns")]
        public void RefusesMalformedProgress(string kind, string expected)
        {
            using var capture = new SyntheticCapture();
            var rows = capture.Rows(30);
            int last = rows.Count - 1;
            Func<string, string> edit = null;
            switch (kind)
            {
                case "header": edit = t => "tick\tsim\t" + t.Substring("tick\tsim_s\t".Length); break;
                case "columns": rows[5] = rows[5] + "\textra"; break;
                case "nan": { var c = rows[5].Split('\t'); c[2] = "NaN"; rows[5] = string.Join("\t", c); break; }
                case "backwards": { var c = rows[last].Split('\t'); c[1] = "0.001"; rows[last] = string.Join("\t", c); break; }
                case "status": { var c = rows[5].Split('\t'); c[7] = ""; rows[5] = string.Join("\t", c); break; }
                case "blank": rows[5] = ""; break;
            }
            string dir = capture.Write("progress", capture.Route(rows.Count), rows, editProgress: edit);

            var ex = Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(dir));
            Assert.Contains(expected, ex.Message);
        }

        [Fact]
        public void RefusesUnsealedChangedMissingAndOversizedCaptures()
        {
            using var capture = new SyntheticCapture();

            string changed = capture.Valid("changed");
            File.AppendAllText(Path.Combine(changed, "progress.tsv"), "1\t1\t1\t1\t1\t1\t\tRACE\n");
            Assert.Contains("changed or truncated", Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(changed)).Message);

            string unsealed = capture.Valid("unsealed");
            File.Delete(Path.Combine(unsealed, "complete.tsv"));
            Assert.Contains("missing complete.tsv", Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(unsealed)).Message);

            string missing = capture.Valid("missing");
            File.Delete(Path.Combine(missing, "route.json"));
            Assert.Contains("missing route.json", Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(missing)).Message);

            // Over 8 MB is refused before any hashing or parsing.
            var rows = capture.Rows(30);
            string big = capture.Write("big", capture.Route(rows.Count), rows,
                editRoute: t => t.Substring(0, t.Length - 1) + new string(' ', (int)CourseRouteFile.MaxBytes) + "}");
            Assert.Contains("exceeds 8 MB", Assert.Throws<InvalidDataException>(() => CourseRouteFile.Read(big)).Message);
        }
    }

    /// <summary>Runs only when <see cref="Variable"/> names an actual private capture folder.</summary>
    public sealed class CourseCaptureFactAttribute : FactAttribute
    {
        public const string Variable = "DBCE_COURSE_CAPTURE";
        public const string ReferenceVariable = "DBCE_COURSE_REFERENCE";

        public CourseCaptureFactAttribute()
        {
            if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable(Variable)))
                Skip = Variable + " is not set; real captures stay private and are judged locally";
        }
    }

    /// <summary>Runs only when <see cref="CourseCaptureFactAttribute.ReferenceVariable"/> names a recorded trajectory folder.</summary>
    public sealed class CourseReferenceFactAttribute : FactAttribute
    {
        public CourseReferenceFactAttribute()
        {
            if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable(CourseCaptureFactAttribute.ReferenceVariable)))
                Skip = CourseCaptureFactAttribute.ReferenceVariable + " is not set; recorded trajectories stay private";
        }
    }

    /// <summary>
    /// The pass thresholds from docs/COURSE-TELEMETRY.md ("Woden route extraction handoff"), applied to an actual
    /// capture folder and, when <see cref="CourseCaptureFactAttribute.ReferenceVariable"/> names one, a recorded
    /// trajectory of the same course. Nothing is copied or changed.
    /// </summary>
    public class CourseCaptureEvidenceTests
    {
        private readonly ITestOutputHelper _output;

        public CourseCaptureEvidenceTests(ITestOutputHelper output) => _output = output;

        [CourseReferenceFact]
        public void ReferenceTrajectoryReadsThroughTheSharedTapeReader()
        {
            // The reference a capture is judged against must pass Playback's own seal, metadata and timeline checks.
            var tape = TrajectoryTape.Read(Environment.GetEnvironmentVariable(CourseCaptureFactAttribute.ReferenceVariable));
            double plan = 0;
            for (int i = 1; i < tape.Frames.Length; i++)
            {
                var a = CourseAxes.FromUnity(tape.Frames[i - 1].Values[0], tape.Frames[i - 1].Values[1], tape.Frames[i - 1].Values[2]);
                var b = CourseAxes.FromUnity(tape.Frames[i].Values[0], tape.Frames[i].Values[1], tape.Frames[i].Values[2]);
                plan += Math.Sqrt((b.North - a.North) * (b.North - a.North) + (b.East - a.East) * (b.East - a.East));
            }
            _output.WriteLine($"{tape.Metadata["game"]} | {tape.Metadata["scenario"]} | build {tape.Metadata["gameSha256"].Substring(0, 8)} | " +
                              $"{tape.Frames.Length} frames at {tape.Metadata["fixedDeltaTime"]} s | plan path {plan:0.0} m");
            Assert.True(tape.Frames.Length >= 2);
            Assert.True(plan > 0);
        }

        [CourseCaptureFact]
        public void ActualCaptureMeetsTheHandoffThresholds()
        {
            var route = CourseRouteFile.Read(Environment.GetEnvironmentVariable(CourseCaptureFactAttribute.Variable));
            var path = route.ToPath();
            _output.WriteLine($"{route.TrackAsset} reverse={route.ReverseTrack} loop={route.IsClosedCircuit} waypoints={route.Waypoints.Count} " +
                              $"path={path.LengthM:0.0} m length={route.Length} kms={route.Kms} unit={route.UnitConversionFactor} " +
                              $"rows={route.Progress.Count} skipped={route.SkippedRows} state={route.State} ({route.Reason})");

            var own = CourseCaptureCheck.OwnDrive(route);
            _output.WriteLine($"own drive: fix={own.FixOnFirstSample} discontinuities={own.Discontinuities} off-course={own.OffCourse} " +
                              $"backward={own.UnflaggedBackwardSteps} offset p95={own.OffsetP95M:0.00} max={own.OffsetMaxM:0.00} " +
                              $"course={own.CourseDistanceM:0.0} m plan={own.PlanPathM:0.0} m gameScale={own.GameScale} " +
                              $"delta median={own.GameDeltaMedianM} p95={own.GameDeltaP95M}");
            Assert.True(own.FixOnFirstSample);
            Assert.Equal(0, own.Discontinuities);
            Assert.True(own.OffsetP95M <= 10, "p95 offset " + own.OffsetP95M);
            Assert.Equal(0, own.UnflaggedBackwardSteps);
            Assert.NotNull(own.GameScale);
            Assert.True(own.GameDeltaMedianM <= 5 && own.GameDeltaP95M <= 15, "game delta " + own.GameDeltaMedianM + " / " + own.GameDeltaP95M);

            string reference = Environment.GetEnvironmentVariable(CourseCaptureFactAttribute.ReferenceVariable);
            if (string.IsNullOrEmpty(reference)) return;
            var tape = TrajectoryTape.Read(reference);
            var r = CourseCaptureCheck.Trajectory(route, tape);
            _output.WriteLine($"reference {tape.Metadata["scenario"]}: sameBuild={CourseCaptureCheck.SameBuild(route, tape)} fix={r.FixOnFirstSample} " +
                              $"discontinuities={r.Discontinuities} backward={r.UnflaggedBackwardSteps} offset p95={r.OffsetP95M:0.00} " +
                              $"course={r.CourseDistanceM:0.0} m plan={r.PlanPathM:0.0} m");
            Assert.True(CourseCaptureCheck.SameBuild(route, tape), "reference was recorded on another game build");
            Assert.True(r.FixOnFirstSample);
            Assert.Equal(0, r.Discontinuities);
            Assert.True(r.OffsetP95M <= 10, "reference p95 offset " + r.OffsetP95M);
            Assert.Equal(0, r.UnflaggedBackwardSteps);
            Assert.True(Math.Abs(r.CourseDistanceM - r.PlanPathM) <= 0.05 * r.PlanPathM, $"course {r.CourseDistanceM} vs plan {r.PlanPathM}");
        }
    }
}
