using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using Dbce.Wheel.Course;
using Dbce.Wheel.Playback;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests;

// Synthetic payloads mirror the completed producers' raw schemas, including intentionally absent data.
// No game, coordinate conversion, telemetry encoder or owner capture is involved.
internal sealed class RawCapture : IDisposable
{
    internal readonly string Root = Path.Combine(Path.GetTempPath(), "dbce-course-raw-" + Guid.NewGuid().ToString("N"));
    internal static readonly string[] Schemas = { "dbce.art-course-route@1", "dbce.drive-course-route@1", "dbce.iracing-course-route@1" };
    internal static readonly string[] Headers = {
        "tick\tsim_s\tpx\tpy\tpz\twaypoint_index\tstatus",
        "tick\tsim_s\tpx\tpy\tpz\tprogress_raw\toffset_x\toffset_y\toffset_z\trace_on\trace_ended",
        "tick\tsim_s\tpx\tpy\tpz\traw_time\traw_clamped_time\tparticipant_index"
    };
    internal static string[] Rows(int kind) => kind switch {
        0 => new[] { "1\t0.1\t10\t20\t30\t5\tUNDERWAY", "2\t0.2\t11\t20\t31\t-1\tFINISHED" },
        1 => new[] { "1\t0.1\t10\t20\t30\t97.5\t\t\t\t1\t0", "2\t0.2\t11\t20\t31\t2.5\t1000\t0\t2000\t0\t1" },
        _ => new[] { "1\t0.1\t10\t20\t30\t\t\t", "2\t0.2\t11\t20\t31\t-25\t12.5\t2" }
    };
    internal RawCapture() => Directory.CreateDirectory(Root);
    internal JsonObject Route(int kind)
    {
        var r = new JsonObject { ["schema"] = Schemas[kind], ["game"] = new[] { "art of rally", "DRIVE Rally", "iRacing Arcade" }[kind],
            ["requestId"] = "1bc4a993ff4241318cac454649377123", ["gameAssemblySha256"] = new string('a',64),
            ["frame"] = "unity-world", ["waypointCount"] = 2, ["waypoints"] = new JsonArray(new JsonArray(1f,2f,3f),new JsonArray(4f,5f,6f)),
            ["rows"] = 2, ["skippedRows"] = 3, ["state"] = "Completed", ["reason"] = "native state only",
            ["directionStatus"] = "unverified native list", ["progressUnits"] = "raw, unqualified", ["future"] = new JsonObject { ["scale"] = null, ["notes"] = new JsonArray("unknown",7) } };
        if (kind != 2) r["capturedUtc"] = "2026-10-19T18:00:00.0000000Z";
        if (kind == 0)
        {
            r["gameMode"] = "Rally"; r["area"] = "CUSTOM"; r["stageName"] = "custom stage"; r["sceneConstant"] = 4;
            r["stageForward"] = true; r["routeForward"] = false; r["stageDistance"] = 1.25f;
            r["stageDistanceUnits"] = "game displays km, unqualified against route"; r["gameStartIndex"] = null; r["gameLastIndex"] = -2;
        }
        else if (kind == 1)
        {
            r["trackId"] = "holzberg"; r["scene"] = "finland"; r["mode"] = "QUICK_RACE"; r["reverseTrack"] = true;
            r["reportedDistance"] = 1535.5f; r["distanceUnits"] = "native, unqualified"; r["axesStatus"] = "raw XYZ";
            r["floatingOriginStatus"] = "raw offset, not applied"; r["initialFloatingOriginOffset"] = null; r["courseSample"] = null;
        }
        else
        {
            ((JsonArray)r["waypoints"]).Add(new JsonArray(7f,8f,9f)); r["waypointCount"]=3;
            r["trackAsset"] = "aragon"; r["scene"] = "race"; r["pathObject"] = "native path"; r["identityEpoch"] = 12345678900L;
            r["isClosedCircuit"] = true; r["reportedLengthMetres"] = 5100f; r["geometryLengthM"] = 5000.123456789;
            r["pathHash"] = "0123456789abcdef"; r["direction"] = null; r["courseSample"] = null;
            r["progressAssociation"] = "same index candidate, not qualified";
        }
        return r;
    }
    internal string Write(int kind, Action<JsonObject> edit = null, Func<string,string> editJson = null, Func<string,string> editTsv = null)
    {
        string directory = Path.Combine(Root, Guid.NewGuid().ToString("N")); Directory.CreateDirectory(directory);
        var route = Route(kind); edit?.Invoke(route);
        string json = route.ToJsonString(), tsv = Headers[kind] + "\n" + string.Join("\n",Rows(kind)) + "\n";
        File.WriteAllText(Path.Combine(directory,"route.json"), editJson?.Invoke(json) ?? json, new UTF8Encoding(false));
        File.WriteAllText(Path.Combine(directory,"progress.tsv"), editTsv?.Invoke(tsv) ?? tsv, new UTF8Encoding(false));
        ArtifactSeal.Complete(directory,new[] { "route.json", "progress.tsv" }); return directory;
    }
    internal static void Reseal(string directory)
    { File.Delete(Path.Combine(directory,"complete.tsv")); ArtifactSeal.Complete(directory,new[] { "route.json", "progress.tsv" }); }
    public void Dispose()
    {
        if (!Path.GetFullPath(Root).StartsWith(Path.GetFullPath(Path.GetTempPath()),StringComparison.OrdinalIgnoreCase) ||
            !Path.GetFileName(Root).StartsWith("dbce-course-raw-",StringComparison.Ordinal)) throw new Exception("unsafe fixture cleanup");
        Directory.Delete(Root,recursive:true);
    }
}

public sealed class RawCourseFilesTests
{
    [Theory]
    [InlineData(0)] [InlineData(1)] [InlineData(2)]
    public void ImportsRawSchemasWithoutMappingXYZOrManufacturingSemanticValues(int kind)
    {
        using var fixture = new RawCapture(); string dir = fixture.Write(kind);
        var before = Directory.GetFiles(dir).ToDictionary(p=>p,p=>(File.ReadAllBytes(p),File.GetLastWriteTimeUtc(p)));
        var file = CourseRouteFile.Read(dir); var raw = Assert.IsType<RawCourseRoute>(file.Raw);
        Assert.Equal(RawCapture.Schemas[kind],file.SchemaId);
        Assert.Equal(new RawCoursePoint(1,2,3),raw.Waypoints[0]);
        Assert.Equal(new RawCoursePoint(10,20,30),raw.Progress[0].Position);
        Assert.Equal("unknown",raw.Metadata.GetProperty("future").GetProperty("notes")[0].GetString());
        Assert.Equal(System.Text.Json.JsonValueKind.Null,raw.Metadata.GetProperty("future").GetProperty("scale").ValueKind);
        Assert.Equal("unverified native list",raw.Metadata.GetProperty("directionStatus").GetString());
        Assert.Equal(3,file.SkippedRows);
        if (kind == 0)
        {
            Assert.Equal("-1",raw.Progress[1].Fields["waypoint_index"]);
            Assert.Equal(1.25f,raw.Metadata.GetProperty("stageDistance").GetSingle());
            Assert.False(raw.Metadata.TryGetProperty("courseSample",out _)); // Producer does not emit the property.
        }
        else if (kind == 1)
        {
            Assert.Equal("97.5",raw.Progress[0].Fields["progress_raw"]); Assert.Equal("2.5",raw.Progress[1].Fields["progress_raw"]);
            Assert.Equal("",raw.Progress[0].Fields["offset_x"]); Assert.Equal("1000",raw.Progress[1].Fields["offset_x"]);
            Assert.Equal(new RawCoursePoint(11,20,31),raw.Progress[1].Position); // Offset was not applied.
        }
        else
        {
            Assert.Equal("",raw.Progress[0].Fields["raw_time"]); Assert.Equal("-25",raw.Progress[1].Fields["raw_time"]);
            Assert.Equal("12.5",raw.Progress[1].Fields["raw_clamped_time"]); // No invented [0,1] clamping.
            Assert.False(raw.Metadata.TryGetProperty("capturedUtc",out _));
            Assert.Equal(12345678900L,raw.Metadata.GetProperty("identityEpoch").GetInt64());
        }
        foreach (var p in before) { Assert.Equal(p.Value.Item1,File.ReadAllBytes(p.Key)); Assert.Equal(p.Value.Item2,File.GetLastWriteTimeUtc(p.Key)); }
        Assert.Equal(before.Keys.OrderBy(p=>p),Directory.GetFiles(dir).OrderBy(p=>p));
        Assert.Throws<NotSupportedException>(()=>((IList<RawCoursePoint>)raw.Waypoints)[0]=new RawCoursePoint(9,9,9));
        Assert.Throws<NotSupportedException>(()=>((IDictionary<string,string>)raw.Progress[0].Fields)["invented"]="1");
        var limited=CourseRouteFile.Read(fixture.Write(kind,r=>r["state"]="LimitReached"));
        Assert.Equal("LimitReached",limited.State);
        if (kind==1)
        {
            var offset=CourseRouteFile.Read(fixture.Write(kind,r=>r["initialFloatingOriginOffset"]=new JsonArray(1000,0,-2000)));
            Assert.Equal(-2000,offset.Raw.Metadata.GetProperty("initialFloatingOriginOffset")[2].GetInt32());
            Assert.Equal(new RawCoursePoint(1,2,3),offset.Raw.Waypoints[0]);
        }
    }

    [Theory]
    [InlineData(0)] [InlineData(1)] [InlineData(2)]
    public void RefusesWodenConversionAndComparisonsForRawSemantics(int kind)
    {
        using var f = new RawCapture(); var file = CourseRouteFile.Read(f.Write(kind));
        foreach (Action access in new Action[] { ()=>{_ = file.TrackAsset;}, ()=>{_ = file.TrackName;}, ()=>{_ = file.ReverseTrack;},
            ()=>{_ = file.TotalLaps;}, ()=>{_ = file.IsClosedCircuit;}, ()=>{_ = file.Length;}, ()=>{_ = file.Kms;},
            ()=>{_ = file.UnitConversionFactor;}, ()=>{_ = file.Waypoints;}, ()=>{_ = file.Distances;},
            ()=>{_ = file.CircuitSameAsStageProgress;}, ()=>{_ = file.CapturedUtc;}, ()=>{_ = file.Progress;},
            ()=>file.ToPath(), ()=>CourseCaptureCheck.OwnDrive(file) })
            Assert.Contains("unqualified",Assert.Throws<InvalidOperationException>(access).Message);
    }

    [Theory]
    [InlineData(0)] [InlineData(1)] [InlineData(2)]
    public void RejectsMalformedCommonMetadataEvenWithAValidSeal(int kind)
    {
        using var f = new RawCapture();
        foreach (Action<JsonObject> edit in new Action<JsonObject>[] {
            r=>r["schema"]="dbce.art-course-route@2", r=>r["game"]="wrong game", r=>r["frame"]="north-east-up",
            r=>r["gameAssemblySha256"]="bad", r=>r["requestId"]=new string('0',32), r=>r["requestId"]="bad",
            r=>r["state"]="Faulted", r=>r["rows"]=36001, r=>r["rows"]=0, r=>r["rows"]=1, r=>r["skippedRows"]=-1,
            r=>r["waypointCount"]=4, r=>r["waypoints"]=new JsonArray(new JsonArray(1,2,3)),
            r=>r["waypoints"][0]=new JsonArray(1,2), r=>r["waypoints"][0][1]=1e300,
            r=>r.Remove("progressUnits"), r=>r["directionStatus"]="", r=>r["courseSample"]=new JsonObject { ["Progress01"]=1 },
            r=>r["capturedUtc"]="2026-10-19 12:00" })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(kind,edit)));
        foreach (Func<string,string> edit in new Func<string,string>[] { j=>j[..^1], j=>j+"{}", j=>"{/*comment*/"+j[1..],
            j=>j[..^1]+",}", j=>j.Replace("\"schema\":", "\"schema\":\"duplicate\",\"schema\":"),
            j=>j.Replace("\"scale\":null", "\"scale\":null,\"scale\":null") })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(kind,editJson:edit)));
    }

    [Theory]
    [InlineData(0)] [InlineData(1)] [InlineData(2)]
    public void RejectsWrongOrTruncatedRowsAndBrokenSeals(int kind)
    {
        using var f = new RawCapture();
        foreach (Func<string,string> edit in new Func<string,string>[] {
            t=>t.Replace(RawCapture.Headers[kind],RawCapture.Headers[(kind+1)%3]), t=>string.Join("\n",t.Split('\n').Take(2))+"\n",
            t=>t.Replace("\t0.2\t","\t0.01\t"), t=>t.Replace("\t0.2\t","\tNaN\t"),
            t=>t.Replace("\t10\t20\t30\t","\tInfinity\t20\t30\t"), t=>t.Replace("\t10\t20\t30\t","\t1e100\t20\t30\t"),
            t=>t.Replace("2\t0.2\t","0\t0.2\t"), t=>t.Replace("1\t0.1\t","bad\t0.1\t"), t=>t.Replace("1\t0.1\t","-1\t0.1\t"),
            t=>t+RawCapture.Rows(kind)[0]+"\n", t=>t.Replace("\t30\t","\t30\textra\t") })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(kind,editTsv:edit)));
        foreach (string name in new[] { "route.json", "progress.tsv" })
        {
            string dir = f.Write(kind); string path = Path.Combine(dir,name); byte[] bytes = File.ReadAllBytes(path);
            File.WriteAllBytes(path,bytes[..^1]); Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(dir));
            dir=f.Write(kind); path=Path.Combine(dir,name); bytes=File.ReadAllBytes(path); bytes[0]^=1; File.WriteAllBytes(path,bytes);
            Assert.Contains("changed or truncated",Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(dir)).Message); // Same length still needs the exact digest.
            dir=f.Write(kind); File.Delete(Path.Combine(dir,name)); Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(dir));
        }
        string unsealed=f.Write(kind); File.Delete(Path.Combine(unsealed,"complete.tsv")); Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(unsealed));
        string utf8=f.Write(kind); File.WriteAllBytes(Path.Combine(utf8,"progress.tsv"),new byte[]{0xff,0xfe,0xfa}); RawCapture.Reseal(utf8);
        Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(utf8));
        string extra=f.Write(kind); File.WriteAllText(Path.Combine(extra,"extra"),"unbounded extra payload should never be read");
        File.Delete(Path.Combine(extra,"complete.tsv")); ArtifactSeal.Complete(extra,new[]{"route.json","progress.tsv","extra"});
        Assert.Contains("exactly",Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(extra)).Message);
    }

    [Fact]
    public void RefusesSchemaSpecificInvalidTypesAndPartialUnknownValues()
    {
        using var f=new RawCapture();
        foreach (Action<JsonObject> edit in new Action<JsonObject>[] { r=>r["sceneConstant"]="4",r=>r["stageForward"]="false",
            r=>r.Remove("gameStartIndex"),r=>r["gameLastIndex"]=-3,r=>r["gameStartIndex"]=2,r=>r["stageDistance"]="1.2" })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(0,edit)));
        foreach (Action<JsonObject> edit in new Action<JsonObject>[] { r=>r.Remove("initialFloatingOriginOffset"),r=>r["initialFloatingOriginOffset"]=new JsonArray(1,2),
            r=>r["reverseTrack"]="true",r=>r["mode"]="FREE_ROAM",r=>r["reportedDistance"]=0 })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(1,edit)));
        foreach (Action<JsonObject> edit in new Action<JsonObject>[] { r=>r["direction"]="fwd",r=>r["identityEpoch"]=0,
            r=>r["identityEpoch"]=1.5,r=>r["pathHash"]="bad",r=>r.Remove("progressAssociation"),r=>r["geometryLengthM"]=0,
            r=>{ ((JsonArray)r["waypoints"]).RemoveAt(2); r["waypointCount"]=2; } })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(2,edit)));
        foreach (var (kind,before,after) in new[] { (0,"5\tUNDERWAY","5.5\tUNDERWAY"),(0,"5\tUNDERWAY","5\t"),
            (1,"97.5\t\t\t\t1\t0","97.5\t1\t\t\t1\t0"),(1,"\t1\t0\n","\ttrue\t0\n"),
            (2,"-25\t12.5\t2","-25\t\t2"),(2,"-25\t12.5\t2","-25\t12.5\t-1") })
            Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(kind,editTsv:t=>t.Replace(before,after))));
    }

    [Fact]
    public void RejectsByteAndPointCapsBeforeParsing()
    {
        using var f=new RawCapture();
        string large=f.Write(1); File.WriteAllText(Path.Combine(large,"route.json"),new string('x',(int)CourseRouteFile.MaxBytes));
        Assert.Contains("exceeds",Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(large)).Message);
        string seal=f.Write(1); File.WriteAllText(Path.Combine(seal,"complete.tsv"),new string('x',4097));
        Assert.Contains("exceeds",Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(seal)).Message);
        Assert.Throws<InvalidDataException>(()=>CourseRouteFile.Read(f.Write(1,r=>r["waypointCount"]=20001)));
    }
}
