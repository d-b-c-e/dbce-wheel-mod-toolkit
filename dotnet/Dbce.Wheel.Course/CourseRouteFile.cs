using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using Dbce.Wheel.Playback;
using Dbce.Wheel.Telemetry;

namespace Dbce.Wheel.Course;

/// <summary>One accepted physics tick of a route capture, with the position mapped to north/east/up.</summary>
public sealed record CourseRouteRow(long Tick, double SimSeconds, CourseVector Position, double ProgressDistance,
    double? PlayerPercentage, string Status);

/// <summary>
/// Reader for sealed course captures: Woden's <c>dbce.course-route@1</c> and the game-specific raw
/// Art/DRIVE/iRacing schemas. All use route.json, progress.tsv and complete.tsv.
/// </summary>
/// <remarks>
/// <para>
/// Read-only: files are opened for reading and nothing in the folder is changed. Order of checks: sizes
/// (before any hashing or parsing), then the seal through <see cref="ArtifactSeal"/> (names, reparse points,
/// every sealed byte), then the schema. Any violation is an <see cref="InvalidDataException"/> naming the field.
/// </para>
/// <para>
/// Woden points are mapped with <see cref="CourseAxes.FromUnity"/> as before. Raw imports preserve XYZ and
/// full cloned metadata in <see cref="Raw"/>; the Woden conversion/comparison accessors refuse those imports.
/// Duplicate properties (including nested extras) are refused. Unknown raw metadata is preserved.
/// </para>
/// </remarks>
public sealed partial class CourseRouteFile
{
    public const string Schema = "dbce.course-route@1";
    public const string Frame = "unity-world";
    public const string ProgressHeader = "tick\tsim_s\tpx\tpy\tpz\tprogress_distance\tplayer_percentage\tstatus";
    public const long MaxBytes = 8L * 1024 * 1024;
    public const int MaxWaypoints = 20_000;
    public const int MaxRows = 36_000;

    private CourseRouteFile() { }

    /// <summary>The exact admitted schema. Raw imports do not imply a qualified course.</summary>
    public string SchemaId { get; private init; } = Schema;
    /// <summary>Unconverted game-specific data, or null for the original Woden format.</summary>
    public RawCourseRoute? Raw { get; private init; }

    public string Game { get; private init; } = "";
    public string RequestId { get; private init; } = "";
    public string GameAssemblySha256 { get; private init; } = "";
    private string trackAsset = "";
    private string? trackName;
    private bool reverseTrack, isClosedCircuit;
    private int totalLaps;
    private double length, kms, unitConversionFactor;
    private IReadOnlyList<CourseVector> waypoints = Array.Empty<CourseVector>();
    private IReadOnlyList<double> distances = Array.Empty<double>();
    private bool? circuitSameAsStageProgress;
    private DateTime capturedUtc;
    private IReadOnlyList<CourseRouteRow> progress = Array.Empty<CourseRouteRow>();
    public string TrackAsset { get => Woden(trackAsset); private init => trackAsset = value; }
    public string? TrackName { get => Woden(trackName); private init => trackName = value; }
    public bool ReverseTrack { get => Woden(reverseTrack); private init => reverseTrack = value; }
    public int TotalLaps { get => Woden(totalLaps); private init => totalLaps = value; }
    public bool IsClosedCircuit { get => Woden(isClosedCircuit); private init => isClosedCircuit = value; }
    public double Length { get => Woden(length); private init => length = value; }
    public double Kms { get => Woden(kms); private init => kms = value; }
    public double UnitConversionFactor { get => Woden(unitConversionFactor); private init => unitConversionFactor = value; }
    public IReadOnlyList<CourseVector> Waypoints { get => Woden(waypoints); private init => waypoints = value; }
    public IReadOnlyList<double> Distances { get => Woden(distances); private init => distances = value; }
    public bool? CircuitSameAsStageProgress { get => Woden(circuitSameAsStageProgress); private init => circuitSameAsStageProgress = value; }
    public string State { get; private init; } = "";
    public string? Reason { get; private init; }
    public int SkippedRows { get; private init; }
    public DateTime CapturedUtc { get => Woden(capturedUtc); private init => capturedUtc = value; }
    public IReadOnlyList<CourseRouteRow> Progress { get => Woden(progress); private init => progress = value; }

    private T Woden<T>(T value) => Raw == null ? value : throw new InvalidOperationException(
        "Raw course import has unqualified semantics; inspect Raw metadata/XYZ/fields instead of Woden course conversion.");

    /// <summary>The captured route as a course path, looping when the game's circuit is closed.</summary>
    public CoursePath ToPath() => CoursePath.Create(Waypoints.ToList(), IsClosedCircuit);

    public static CourseRouteFile Read(string directory)
    {
        if (string.IsNullOrEmpty(directory)) throw new ArgumentException("capture folder required", nameof(directory));
        string routePath = Path.Combine(directory, "route.json");
        string progressPath = Path.Combine(directory, "progress.tsv");
        foreach (var path in new[] { routePath, progressPath, Path.Combine(directory, "complete.tsv") })
            if (!File.Exists(path)) throw new InvalidDataException("Course route capture is missing " + Path.GetFileName(path));
        long sealBytes = new FileInfo(Path.Combine(directory, "complete.tsv")).Length;
        if (sealBytes > 4096 || new FileInfo(routePath).Length + new FileInfo(progressPath).Length + sealBytes > MaxBytes)
            throw new InvalidDataException("Course route capture exceeds 8 MB");
        // Do not let an otherwise valid seal send the shared verifier hashing arbitrary extra/unbounded files.
        var seal = StrictLines(Path.Combine(directory, "complete.tsv"));
        var sealedNames = seal.Skip(1).Select(l => l.Split('\t')[0]).ToArray();
        if (seal.Length != 3 || !sealedNames.OrderBy(n => n, StringComparer.Ordinal).SequenceEqual(new[] { "progress.tsv", "route.json" }))
            throw Bad("completion seal must bind exactly route.json and progress.tsv");
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
            CheckDuplicates(root);

            string schema = Text(root, "schema");
            if (schema != Schema && !RawSchemas.Contains(schema)) throw Bad("unsupported schema " + schema);
            if (schema != Schema) return ReadRaw(root, schema, progressPath);
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
        var lines = StrictLines(path);
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

    private static string[] StrictLines(string path)
    {
        // This preflight also reads the seal before ArtifactSeal.Verify. Refuse linked paths before any text read.
        for (string? current = Path.GetFullPath(path); current != null; current = Path.GetDirectoryName(current))
            if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw Bad("capture path is a reparse point");
        try
        {
            using var reader = new StreamReader(path, new UTF8Encoding(false, true), detectEncodingFromByteOrderMarks: false);
            var lines = new List<string>();
            for (string? line = reader.ReadLine(); line != null; line = reader.ReadLine()) lines.Add(line);
            return lines.ToArray();
        }
        catch (DecoderFallbackException ex) { throw new InvalidDataException("Course route capture: invalid UTF-8", ex); }
    }

    private static void CheckDuplicates(JsonElement value)
    {
        if (value.ValueKind == JsonValueKind.Object)
        {
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var p in value.EnumerateObject())
            {
                if (!seen.Add(p.Name)) throw Bad("route.json repeats " + p.Name);
                CheckDuplicates(p.Value);
            }
        }
        else if (value.ValueKind == JsonValueKind.Array)
            foreach (var item in value.EnumerateArray()) CheckDuplicates(item);
    }
}
