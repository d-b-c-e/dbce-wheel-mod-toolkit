using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text.Json;
using Dbce.Wheel.Telemetry;
using Xunit;
using Xunit.Abstractions;

namespace Dbce.Wheel.Telemetry.Tests
{
    /// <summary>Runs only when <see cref="Variable"/> names a folder of private copies; skipped otherwise.</summary>
    public sealed class PrivateEvidenceFactAttribute : FactAttribute
    {
        public const string Variable = "DBCE_COURSE_PRIVATE";

        public PrivateEvidenceFactAttribute()
        {
            if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable(Variable)))
                Skip = Variable + " is not set; owner SimHub map copies stay private and are never committed";
        }
    }

    /// <summary>
    /// The tracker against SimHub's own progress on real laps, using private copies of the owner's
    /// SimHub map cache. Only paths and hashes are committed; the data never is.
    /// </summary>
    /// <remarks>
    /// Each pair is one lap SimHub recorded locally (positions with SimHub's stored progress
    /// <c>p</c>) and SimHub's cloud consensus map for the same track. The consensus becomes the
    /// <see cref="CoursePath"/>; the tracker then follows the local lap, and its progress is
    /// compared with SimHub's. Copies were made with <c>cp -p</c> from
    /// <c>C:\Program Files (x86)\SimHub\PluginsData</c> on 2026-10-18; the hashes pin them.
    /// </remarks>
    public class PrivateEvidenceTests
    {
        private readonly ITestOutputHelper _output;

        public PrivateEvidenceTests(ITestOutputHelper output) => _output = output;

        private static readonly (string Local, string LocalSha, string Cloud, string CloudSha, double ReportedM)[] Pairs =
        {
            ("EAWRC23/MapRecords/28_276_9741.9277.shtl", "a8ddeac6aa7714bf20563c74a57cd76c09e14d6b18338e2dd4ff9dc7a4e42f35",
             "EAWRC23/MapRecordsCloud/28_276_9741.9277-9741.927734375.shtl", "169a7fe104169b842de6e944bd42eb7bbbcf958357c2ed713968bfdbcf732ff2",
             9741.927734375),
            ("PCars3/MapRecords/Monument_Canyon-Array_Peak.shtl", "27419cf0068e7383434f678d31e7f29579bad0952083bc1e14190a8005bdbfae",
             "PCars3/MapRecordsCloud/Monument_Canyon-Array_Peak-3612.07763671875.shtl", "9fe8dde08c8b0382cb623ef8d8d72b6f684b871d0a4688d98144cf3a75c22023",
             3612.07763671875),
            ("PCars3/MapRecords/Monument_Canyon-River_Run.shtl", "eb2ca2f5afea93acaf6446cbd596ab06737ffb902cee46d9b629252333ab4956",
             "PCars3/MapRecordsCloud/Monument_Canyon-River_Run-2450.27416992188.shtl", "5c4100ac0c0d6841fe8a5be1bde1568b4b752760536da5f361e2494b2ec5893e",
             2450.27416992188),
            ("PCars3/MapRecords/SportslandSugo-GP.shtl", "4269ee8b3a93582f3e55bc91aae56f4126239ba590958771d4fe2f6ab9b1bec5",
             "PCars3/MapRecordsCloud/SportslandSugo-GP-3567.00756835938.shtl", "2828e686b3118924d4ee1e9f3c35c42246b719a9ad3dffe3f768414ab44f185b",
             3567.00756835938),
        };

        [PrivateEvidenceFact]
        public void TrackerFollowsSimHubsOwnProgressOnRealLaps()
        {
            string root = Environment.GetEnvironmentVariable(PrivateEvidenceFactAttribute.Variable);
            foreach (var pair in Pairs)
            {
                var lap = Load(root, pair.Local, pair.LocalSha);
                var consensus = Load(root, pair.Cloud, pair.CloudSha).Select(s => s.Position).ToList();
                // SimHub's own closed-loop test (GetMapScore): start and end within 40 m.
                bool loop = CourseVector.Distance(consensus[0], consensus[consensus.Count - 1]) < 40;
                var path = CoursePath.Create(consensus, loop);
                var tracker = new CourseTracker(path);

                var errors = new List<double>();
                int jumps = 0, offCourse = 0;
                foreach (var (position, simHubProgress) in lap)
                {
                    var p = tracker.Update(position);
                    if ((p.Flags & CourseFlags.Discontinuity) != 0) jumps++;
                    if ((p.Flags & CourseFlags.OffCourse) != 0) offCourse++;
                    if (!p.HasFix) continue;
                    double e = Math.Abs(p.Progress01 - simHubProgress);
                    if (loop) e = Math.Min(e, 1 - e);
                    errors.Add(e * pair.ReportedM);
                }
                errors.Sort();
                double median = errors[errors.Count / 2];
                double p95 = errors[(int)(errors.Count * 0.95)];
                _output.WriteLine($"{Path.GetFileName(pair.Local)}: loop={loop} lap={lap.Count} consensus={consensus.Count} " +
                                  $"path={path.LengthM:0.0} m; progress error median {median:0.0} m p95 {p95:0.0} m; " +
                                  $"discontinuities {jumps}, off-course {offCourse}");

                Assert.Equal(0, jumps);
                Assert.True(median < 5, "median progress error " + median + " m on " + pair.Local);
                Assert.True(p95 < 15, "p95 progress error " + p95 + " m on " + pair.Local);
            }
        }

        private static List<(CourseVector Position, double P)> Load(string root, string relative, string sha256)
        {
            byte[] bytes = File.ReadAllBytes(Path.Combine(root, relative));
            using (var hash = SHA256.Create())
                Assert.Equal(sha256, Convert.ToHexString(hash.ComputeHash(bytes)).ToLowerInvariant());

            using var gzip = new GZipStream(new MemoryStream(bytes), CompressionMode.Decompress);
            using var json = JsonDocument.Parse(gzip);
            var samples = new List<(CourseVector, double)>();
            foreach (var item in json.RootElement.GetProperty("CarCoordinates").EnumerateArray())
            {
                // SimHub stores [x, y-up, z]; which plan axis is north does not matter for projection.
                var v = item.GetProperty("Value");
                samples.Add((new CourseVector(v[2].GetDouble(), v[0].GetDouble(), v[1].GetDouble()), item.GetProperty("p").GetDouble()));
            }
            return samples;
        }
    }
}
