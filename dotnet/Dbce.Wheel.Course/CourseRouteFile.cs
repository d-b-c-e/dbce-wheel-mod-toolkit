using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json;
using Dbce.Wheel.Playback;
using Dbce.Wheel.Telemetry;

namespace Dbce.Wheel.Course;

/// <summary>One accepted physics tick of a route capture, with the position mapped to north/east/up.</summary>
public sealed record CourseRouteRow(long Tick, double SimSeconds, CourseVector Position, double ProgressDistance,
    double? PlayerPercentage, string Status);

/// <summary>
/// Reader for a <c>dbce.course-route@1</c> capture folder (<c>route.json</c>, <c>progress.tsv</c> and the
/// <c>complete.tsv</c> seal), as Woden's CourseRouteCapture writes it.
/// </summary>
/// <remarks>
/// <para>
/// Read-only: files are opened for reading and nothing in the folder is changed. Order of checks: sizes
/// (before any hashing or parsing), then the seal through <see cref="ArtifactSeal"/> (names, reparse points,
/// every sealed byte), then the schema. Any violation is an <see cref="InvalidDataException"/> naming the field.
/// </para>
/// <para>
/// Points are stored as unconverted Unity world <c>[x, y, z]</c> (<c>frame: unity-world</c>); this reader maps
/// them with <see cref="CourseAxes.FromUnity"/>, the one place that conversion lives. Unknown extra properties
/// are ignored; duplicates are refused.
/// </para>
/// </remarks>
public sealed class CourseRouteFile
{
    public const string Schema = "dbce.course-route@1";
    public const string Frame = "unity-world";
    public const string ProgressHeader = "tick\tsim_s\tpx\tpy\tpz\tprogress_distance\tplayer_percentage\tstatus";
    public const long MaxBytes = 8L * 1024 * 1024;
    public const int MaxWaypoints = 20_000;
    public const int MaxRows = 36_000;

    private CourseRouteFile() { }

    public string Game { get; private init; } = "";
    public string RequestId { get; private init; } = "";
    public string GameAssemblySha256 { get; private init; } = "";
    public string TrackAsset { get; private init; } = "";
    public string? TrackName { get; private init; }
    public bool ReverseTrack { get; private init; }
    public int TotalLaps { get; private init; }
    public bool IsClosedCircuit { get; private init; }
    public double Length { get; private init; }
    public double Kms { get; private init; }
    public double UnitConversionFactor { get; private init; }
    public IReadOnlyList<CourseVector> Waypoints { get; private init; } = Array.Empty<CourseVector>();
    public IReadOnlyList<double> Distances { get; private init; } = Array.Empty<double>();
    public bool? CircuitSameAsStageProgress { get; private init; }
    public string State { get; private init; } = "";
    public string? Reason { get; private init; }
    public int SkippedRows { get; private init; }
    public DateTime CapturedUtc { get; private init; }
    public IReadOnlyList<CourseRouteRow> Progress { get; private init; } = Array.Empty<CourseRouteRow>();

    /// <summary>The captured route as a course path, looping when the game's circuit is closed.</summary>
    public CoursePath ToPath() => CoursePath.Create(Waypoints.ToList(), IsClosedCircuit);

    public static CourseRouteFile Read(string directory)
    {
        if (string.IsNullOrEmpty(directory)) throw new ArgumentException("capture folder required", nameof(directory));
        string routePath = Path.Combine(directory, "route.json");
        string progressPath = Path.Combine(directory, "progress.tsv");
        foreach (var path in new[] { routePath, progressPath, Path.Combine(directory, "complete.tsv") })
            if (!File.Exists(path)) throw new InvalidDataException("Course route capture is missing " + Path.GetFileName(path));
        if (new FileInfo(routePath).Length + new FileInfo(progressPath).Length > MaxBytes)
            throw new InvalidDataException("Course route capture exceeds 8 MB");
        ArtifactSeal.Verify(directory, "route.json", "progress.tsv");

        JsonDocument json;
        try
        {
            json = JsonDocument.Parse(File.ReadAllBytes(routePath), new JsonDocumentOptions
            {
                MaxDepth = 8, CommentHandling = JsonCommentHandling.Disallow, AllowTrailingCommas = false,
            });
        }
        catch (JsonException ex) { throw new InvalidDataException("route.json is not valid JSON: " + ex.Message, ex); }
        using (json)
        {
            var root = json.RootElement;
            if (root.ValueKind != JsonValueKind.Object) throw Bad("route.json root is not an object");
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var p in root.EnumerateObject())
                if (!seen.Add(p.Name)) throw Bad("route.json repeats " + p.Name);

            if (Text(root, "schema") != Schema) throw Bad("schema is not " + Schema);
            if (Text(root, "frame") != Frame) throw Bad("frame is not " + Frame);
            string state = Text(root, "state");
            if (state is not ("Completed" or "LimitReached")) throw Bad("state " + state + " is not a written capture");
            string hash = Text(root, "gameAssemblySha256");
            if (hash.Length != 64 || !hash.All(Uri.IsHexDigit)) throw Bad("gameAssemblySha256 is not a sha256");
            string requestId = Text(root, "requestId");
            if (requestId.Length != 32 || !requestId.All(Uri.IsHexDigit)) throw Bad("requestId is not 32 hex digits");
            string trackAsset = Text(root, "trackAsset");
            if (string.IsNullOrWhiteSpace(trackAsset) || trackAsset.Length > 128) throw Bad("trackAsset missing");
            string game = Text(root, "game");
            if (string.IsNullOrWhiteSpace(game)) throw Bad("game missing");
            int laps = Int(root, "totalLaps");
            if (laps < 0) throw Bad("totalLaps is negative");
            int rows = Int(root, "rows"), skipped = Int(root, "skippedRows");
            if (rows < 0 || rows > MaxRows || skipped < 0) throw Bad("rows or skippedRows out of range");
            if (!DateTime.TryParseExact(Text(root, "capturedUtc"), "O", CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out var captured) ||
                captured.Kind != DateTimeKind.Utc)
                throw Bad("capturedUtc is not a UTC round-trip time");

            var waypoints = Points(root, Int(root, "waypointCount"));
            var distances = new List<double>();
            foreach (var d in Value(root, "distances", JsonValueKind.Array).EnumerateArray())
            {
                if (distances.Count >= MaxWaypoints + 1) throw Bad("distances has more than " + (MaxWaypoints + 1) + " values");
                distances.Add(Single(d, "distances"));
            }

            var file = new CourseRouteFile
            {
                Game = game,
                RequestId = requestId,
                GameAssemblySha256 = hash.ToLowerInvariant(),
                TrackAsset = trackAsset,
                TrackName = OptionalText(root, "trackName"),
                ReverseTrack = Flag(root, "reverseTrack"),
                TotalLaps = laps,
                IsClosedCircuit = Flag(root, "isClosedCircuit"),
                Length = Single(Value(root, "length", JsonValueKind.Number), "length"),
                Kms = Single(Value(root, "kms", JsonValueKind.Number), "kms"),
                UnitConversionFactor = Single(Value(root, "unitConversionFactor", JsonValueKind.Number), "unitConversionFactor"),
                Waypoints = waypoints,
                Distances = distances,
                CircuitSameAsStageProgress = OptionalFlag(root, "circuitSameAsStageProgress"),
                State = state,
                Reason = OptionalText(root, "reason"),
                SkippedRows = skipped,
                CapturedUtc = captured,
                Progress = ReadProgress(progressPath, rows),
            };
            return file;
        }
    }

    private static List<CourseVector> Points(JsonElement root, int declared)
    {
        var array = Value(root, "waypoints", JsonValueKind.Array);
        int count = array.GetArrayLength();
        if (count != declared) throw Bad("waypointCount " + declared + " differs from " + count + " waypoints");
        if (count < 2 || count > MaxWaypoints) throw Bad("waypoints must number 2 to " + MaxWaypoints);
        var points = new List<CourseVector>(count);
        int i = 0;
        foreach (var point in array.EnumerateArray())
        {
            if (point.ValueKind != JsonValueKind.Array || point.GetArrayLength() != 3) throw Bad("waypoint " + i + " is not [x, y, z]");
            var xyz = point.EnumerateArray().Select(v => Single(v, "waypoint " + i)).ToArray();
            points.Add(CourseAxes.FromUnity(xyz[0], xyz[1], xyz[2]));
            i++;
        }
        return points;
    }

    private static List<CourseRouteRow> ReadProgress(string path, int declaredRows)
    {
        var lines = File.ReadAllLines(path);
        if (lines.Length == 0 || lines[0] != ProgressHeader) throw Bad("progress.tsv header differs");
        if (lines.Length - 1 != declaredRows) throw Bad("progress.tsv has " + (lines.Length - 1) + " rows; route.json declares " + declaredRows);
        var rows = new List<CourseRouteRow>(declaredRows);
        double last = double.NegativeInfinity;
        for (int i = 1; i < lines.Length; i++)
        {
            var c = lines[i].Split('\t');
            if (c.Length != 8) throw Bad("progress.tsv row " + i + " has " + c.Length + " columns");
            if (!long.TryParse(c[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out long tick)) throw Bad("progress.tsv row " + i + " tick");
            double sim = Number(c[1], i, "sim_s");
            if (sim < last) throw Bad("progress.tsv row " + i + " goes back in time");
            last = sim;
            var position = CourseAxes.FromUnity(Number(c[2], i, "px"), Number(c[3], i, "py"), Number(c[4], i, "pz"));
            double progress = Number(c[5], i, "progress_distance");
            double? percentage = c[6].Length == 0 ? null : Number(c[6], i, "player_percentage");
            if (c[7].Length == 0) throw Bad("progress.tsv row " + i + " has no status");
            rows.Add(new CourseRouteRow(tick, sim, position, progress, percentage, c[7]));
        }
        return rows;
    }

    private static double Number(string text, int row, string column)
    {
        if (!double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) || !double.IsFinite(value))
            throw Bad("progress.tsv row " + row + " " + column + " is not a finite number");
        return value;
    }

    private static JsonElement Value(JsonElement root, string name, JsonValueKind kind)
    {
        if (!root.TryGetProperty(name, out var value) || value.ValueKind != kind) throw Bad(name + " missing or not " + kind);
        return value;
    }

    private static string Text(JsonElement root, string name) => Value(root, name, JsonValueKind.String).GetString()!;

    private static string? OptionalText(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out var value)) throw Bad(name + " missing");
        if (value.ValueKind == JsonValueKind.Null) return null;
        if (value.ValueKind != JsonValueKind.String) throw Bad(name + " is not text");
        return value.GetString();
    }

    private static bool Flag(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out var value) || value.ValueKind is not (JsonValueKind.True or JsonValueKind.False))
            throw Bad(name + " missing or not a boolean");
        return value.GetBoolean();
    }

    private static bool? OptionalFlag(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out var value)) throw Bad(name + " missing");
        if (value.ValueKind == JsonValueKind.Null) return null;
        if (value.ValueKind is not (JsonValueKind.True or JsonValueKind.False)) throw Bad(name + " is not a boolean");
        return value.GetBoolean();
    }

    private static int Int(JsonElement root, string name)
    {
        if (!Value(root, name, JsonValueKind.Number).TryGetInt32(out int value)) throw Bad(name + " is not an integer");
        return value;
    }

    private static double Single(JsonElement value, string name)
    {
        if (value.ValueKind != JsonValueKind.Number || !value.TryGetSingle(out float f) || !float.IsFinite(f))
            throw Bad(name + " is not a finite float");
        return f;
    }

    private static InvalidDataException Bad(string message) => new("Course route capture: " + message);
}
