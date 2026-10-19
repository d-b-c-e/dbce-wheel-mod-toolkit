using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Text.Json;

namespace Dbce.Wheel.Course;

/// <summary>Unconverted native XYZ. No physical unit, compass axis or origin correction is implied.</summary>
public sealed record RawCoursePoint(double X, double Y, double Z);

/// <summary>Native sample with validated but uninterpreted game-specific columns (empty stays empty).</summary>
public sealed record RawCourseRow(long Tick, double SimSeconds, RawCoursePoint Position, IReadOnlyDictionary<string, string> Fields);

/// <summary>Validated raw import. Full cloned metadata retains unknown fields, nulls and qualification limits.</summary>
public sealed class RawCourseRoute
{
    internal RawCourseRoute(JsonElement metadata, IReadOnlyList<RawCoursePoint> points, IReadOnlyList<RawCourseRow> rows)
    { Metadata = metadata.Clone(); Waypoints = points; Progress = rows; }
    public JsonElement Metadata { get; }
    public IReadOnlyList<RawCoursePoint> Waypoints { get; }
    public IReadOnlyList<RawCourseRow> Progress { get; }
}

public sealed partial class CourseRouteFile
{
    private static readonly string[] RawSchemas = { "dbce.art-course-route@1", "dbce.drive-course-route@1", "dbce.iracing-course-route@1" };
    private static readonly string[] RawHeaders = {
        "tick\tsim_s\tpx\tpy\tpz\twaypoint_index\tstatus",
        "tick\tsim_s\tpx\tpy\tpz\tprogress_raw\toffset_x\toffset_y\toffset_z\trace_on\trace_ended",
        "tick\tsim_s\tpx\tpy\tpz\traw_time\traw_clamped_time\tparticipant_index"
    };

    private static CourseRouteFile ReadRaw(JsonElement root, string schema, string progressPath)
    {
        int kind = Array.IndexOf(RawSchemas, schema);
        string game = Text(root, "game");
        if (game != new[] { "art of rally", "DRIVE Rally", "iRacing Arcade" }[kind]) throw Bad("game does not match raw schema");
        if (Text(root, "frame") != Frame) throw Bad("frame is not " + Frame);
        string hash = Text(root, "gameAssemblySha256"), request = Text(root, "requestId"), state = Text(root, "state");
        if (hash.Length != 64 || !hash.All(Uri.IsHexDigit)) throw Bad("gameAssemblySha256 is not a sha256");
        if (!Guid.TryParseExact(request, "N", out var id) || id == Guid.Empty) throw Bad("requestId is not a nonempty N GUID");
        if (state is not ("Completed" or "LimitReached")) throw Bad("state is not a written capture");
        int count = Int(root, "waypointCount"), rows = Int(root, "rows"), skipped = Int(root, "skippedRows");
        if (count < 2 || count > MaxWaypoints || rows < 1 || rows > MaxRows || skipped < 0) throw Bad("raw point/row counts out of range");
        var array = Value(root, "waypoints", JsonValueKind.Array);
        if (array.GetArrayLength() != count) throw Bad("waypointCount differs from raw waypoints");
        var points = array.EnumerateArray().Select(p => RawPoint(p, "waypoint")).ToArray();
        Nonempty(root, "directionStatus"); Nonempty(root, "progressUnits");
        if (root.TryGetProperty("courseSample", out var sample) && sample.ValueKind != JsonValueKind.Null)
            throw Bad("raw courseSample must be absent or null");
        if (kind != 2 || root.TryGetProperty("capturedUtc", out _))
        {
            if (!DateTime.TryParseExact(Text(root, "capturedUtc"), "O", CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out var utc) || utc.Kind != DateTimeKind.Utc)
                throw Bad("capturedUtc is not UTC round-trip time");
        }
        if (kind == 0)
        {
            Nonempty(root, "area", 64); Nonempty(root, "stageName", 128); Nonempty(root, "gameMode", 64);
            Int(root, "sceneConstant"); Flag(root, "stageForward"); Flag(root, "routeForward");
            Single(Value(root, "stageDistance", JsonValueKind.Number), "stageDistance"); Nonempty(root, "stageDistanceUnits");
            int? start = NullableInt(root, "gameStartIndex"), last = NullableInt(root, "gameLastIndex");
            if (start < 0 || start >= count || last < -2 || last >= count) throw Bad("raw waypoint indices outside route");
        }
        else if (kind == 1)
        {
            Nonempty(root, "trackId", 128); Nonempty(root, "scene", 512);
            if (Text(root, "mode") != "QUICK_RACE") throw Bad("unsupported DRIVE mode");
            Flag(root, "reverseTrack");
            if (Single(Value(root, "reportedDistance", JsonValueKind.Number), "reportedDistance") <= 0) throw Bad("reportedDistance is not positive");
            Nonempty(root, "distanceUnits"); Nonempty(root, "axesStatus"); Nonempty(root, "floatingOriginStatus");
            if (!root.TryGetProperty("initialFloatingOriginOffset", out var offset)) throw Bad("initialFloatingOriginOffset missing");
            if (offset.ValueKind != JsonValueKind.Null) RawPoint(offset, "initialFloatingOriginOffset");
        }
        else
        {
            Nonempty(root, "trackAsset", 128); Nonempty(root, "scene", 256); Nonempty(root, "pathObject", 256);
            if (!Value(root, "identityEpoch", JsonValueKind.Number).TryGetInt64(out long epoch) || epoch <= 0) throw Bad("identityEpoch is not positive int64");
            if (Flag(root, "isClosedCircuit") && count < 3) throw Bad("closed iRacing route needs at least three raw points");
            Single(Value(root, "reportedLengthMetres", JsonValueKind.Number), "reportedLengthMetres");
            if (!Value(root, "geometryLengthM", JsonValueKind.Number).TryGetDouble(out double length) || !double.IsFinite(length) || length <= 0) throw Bad("geometryLengthM is invalid");
            string pathHash = Text(root, "pathHash");
            if (pathHash.Length != 16 || !pathHash.All(Uri.IsHexDigit)) throw Bad("pathHash is not 16 hex digits");
            if (!root.TryGetProperty("direction", out var direction) || direction.ValueKind != JsonValueKind.Null) throw Bad("raw iRacing direction must be null");
            Nonempty(root, "progressAssociation");
        }
        return new CourseRouteFile {
            SchemaId = schema, Game = game, RequestId = request, GameAssemblySha256 = hash,
            State = state, Reason = OptionalText(root, "reason"), SkippedRows = skipped,
            Raw = new RawCourseRoute(root, Array.AsReadOnly(points), ReadRawProgress(progressPath, rows, kind))
        };
    }

    private static IReadOnlyList<RawCourseRow> ReadRawProgress(string path, int declared, int kind)
    {
        var lines = StrictLines(path);
        if (lines.Length == 0 || lines[0] != RawHeaders[kind]) throw Bad("raw progress.tsv header differs from schema");
        if (lines.Length - 1 != declared) throw Bad("raw progress.tsv row count differs from declaration");
        var columns = RawHeaders[kind].Split('\t');
        var rows = new List<RawCourseRow>(declared);
        double last = double.NegativeInfinity;
        long lastTick = -1;
        for (int i = 1; i < lines.Length; i++)
        {
            var c = lines[i].Split('\t');
            if (c.Length != columns.Length) throw Bad("raw progress.tsv column count at row " + i);
            if (!long.TryParse(c[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out long tick) || tick < 0 || tick < lastTick || kind != 0 && tick == lastTick)
                throw Bad("raw progress.tsv tick at row " + i);
            double seconds = Number(c[1], i, "sim_s");
            if (seconds < 0 || seconds < last) throw Bad("raw progress.tsv goes back in time");
            var position = new RawCoursePoint(RawNumber(c[2], i, "px"), RawNumber(c[3], i, "py"), RawNumber(c[4], i, "pz"));
            if (kind == 0)
            {
                ParseInt(c[5], "waypoint_index");
                if (string.IsNullOrEmpty(c[6])) throw Bad("raw status missing");
            }
            else if (kind == 1)
            {
                RawNumber(c[5], i, "progress_raw");
                bool missing = c[6].Length == 0;
                if (missing != (c[7].Length == 0) || missing != (c[8].Length == 0)) throw Bad("partial raw floating-origin offset");
                if (!missing) for (int n = 6; n <= 8; n++) RawNumber(c[n], i, columns[n]);
                if (c[9] is not ("0" or "1") || c[10] is not ("0" or "1")) throw Bad("raw race flag is not 0/1");
            }
            else
            {
                bool missing = c[5].Length == 0;
                if (missing != (c[6].Length == 0) || missing != (c[7].Length == 0)) throw Bad("partial raw participant progress");
                if (!missing)
                {
                    RawNumber(c[5], i, "raw_time"); RawNumber(c[6], i, "raw_clamped_time");
                    if (ParseInt(c[7], "participant_index") < 0) throw Bad("negative participant_index");
                }
            }
            var fields = new ReadOnlyDictionary<string, string>(columns.Skip(5).Select((name, index) => (name, c[index + 5])).ToDictionary(p => p.name, p => p.Item2));
            rows.Add(new RawCourseRow(tick, seconds, position, fields));
            last = seconds; lastTick = tick;
        }
        return rows.AsReadOnly();
    }

    private static RawCoursePoint RawPoint(JsonElement value, string name)
    {
        if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() != 3) throw Bad(name + " is not raw [x,y,z]");
        return new RawCoursePoint(Single(value[0], name), Single(value[1], name), Single(value[2], name));
    }
    private static void Nonempty(JsonElement root, string name, int limit = 4096)
    {
        string text = Text(root, name);
        if (string.IsNullOrWhiteSpace(text) || text.Length > limit) throw Bad(name + " missing or too long");
    }
    private static int? NullableInt(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out var value)) throw Bad(name + " missing");
        return value.ValueKind == JsonValueKind.Null ? null : Int(root, name);
    }
    private static int ParseInt(string text, string name) => int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int value) ? value : throw Bad(name + " is not int32");
    private static double RawNumber(string text, int row, string column)
    {
        double number = Number(text, row, column);
        if (number > float.MaxValue || number < -float.MaxValue) throw Bad(column + " is not a finite float");
        return number;
    }
}
