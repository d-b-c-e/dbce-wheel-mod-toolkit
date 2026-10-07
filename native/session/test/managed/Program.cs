using System;
using System.IO;
using System.Linq;
using Dbce.Wheel.Recording;

static class Program {
    static void Require(bool ok) { if (!ok) throw new Exception("Native session conformance failed."); }
    static int Main(string[] args) {
        string root = args.Single();
        var records = SessionReader.Read(Path.Combine(root, "valid.jsonl")).ToArray();
        Require(records.Length == 4 && records[0].Metadata.Properties["escaped"] == "quoted\" slash\\ line\n tab\t");
        Require(records[1].Sample.Channels["value"] == -0.12345678912345678);
        Require(records[3].Footer.Completed && records[3].Footer.Counts.WrittenSamples == 2);
        foreach (var name in new[] { "abandoned", "invalid", "order" }) {
            bool rejected = false;
            try { SessionReader.Read(Path.Combine(root, name + ".jsonl")).ToArray(); }
            catch (SessionReadException) { rejected = true; }
            Require(rejected);
        }
        foreach (var pair in new[] { ("duration", "durationLimit"), ("size", "fileSizeLimit"), ("record", "recordSizeLimit") }) {
            var footer = SessionReader.Read(Path.Combine(root, pair.Item1 + ".jsonl")).Last().Footer;
            Require(!footer.Completed && footer.StopReason == pair.Item2);
        }
        Console.WriteLine("PASS: strict managed reader accepts native completion/limits and rejects incomplete/invalid files.");
        return 0;
    }
}
