using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace Dbce.Wheel.Playback
{
    /// <summary>One fixed-step pose, with optional input/presentation values. All motion is world-space SI.</summary>
    public sealed class TrajectoryFrame
    {
        public double Time;
        // position xyz, quaternion xyzw, velocity xyz, angular velocity xyz,
        // steer, throttle, brake, rpm, gear. This ordering is part of the v1 contract.
        public float[] Values = new float[18];
        public void Validate()
        {
            if (double.IsNaN(Time) || double.IsInfinity(Time) || Time < 0 || Values == null || Values.Length != 18)
                throw new InvalidDataException("Invalid trajectory frame");
            foreach (var v in Values) if (float.IsNaN(v) || float.IsInfinity(v)) throw new InvalidDataException("Nonfinite trajectory value");
            double q = 0; for (int i = 3; i < 7; i++) q += Values[i] * (double)Values[i];
            if (Math.Abs(q - 1) > .01) throw new InvalidDataException("Trajectory quaternion is not normalized");
        }
        internal string Encode() { Validate(); return Time.ToString("R", CultureInfo.InvariantCulture) + "\t" + string.Join("\t", Values.Select(v => v.ToString("R", CultureInfo.InvariantCulture))); }
        internal static TrajectoryFrame Decode(string line)
        {
            var p = line.Split('\t');
            if (p.Length != 19) throw new InvalidDataException("Trajectory column count");
            try
            {
                var f = new TrajectoryFrame { Time = double.Parse(p[0], CultureInfo.InvariantCulture), Values = p.Skip(1).Select(s => float.Parse(s, CultureInfo.InvariantCulture)).ToArray() };
                f.Validate(); return f;
            }
            catch (FormatException ex) { throw new InvalidDataException("Invalid trajectory number", ex); }
        }
    }

    /// <summary>A deliberately bounded single-stage tape. Scene/car changes must start a separate capture.</summary>
    public sealed class TrajectoryTape
    {
        public const string Header = "time_s\tpx_m\tpy_m\tpz_m\tqx\tqy\tqz\tqw\tvx_mps\tvy_mps\tvz_mps\twx_radps\twy_radps\twz_radps\tsteer\tthrottle\tbrake\trpm\tgear";
        public readonly Dictionary<string, string> Metadata;
        public readonly TrajectoryFrame[] Frames;
        private TrajectoryTape(Dictionary<string, string> metadata, TrajectoryFrame[] frames) { Metadata = metadata; Frames = frames; }
        public static TrajectoryTape Read(string directory)
        {
            ArtifactSeal.Verify(directory, "trajectory.tsv", "trajectory.meta");
            if (new FileInfo(Path.Combine(directory, "trajectory.tsv")).Length > 128 * 1024 * 1024) throw new InvalidDataException("Trajectory exceeds 128 MB");
            var meta = ReadMetadata(Path.Combine(directory, "trajectory.meta"));
            ValidateMetadata(meta);
            var rows = new List<TrajectoryFrame>(); double last = -1;
            using (var reader = File.OpenText(Path.Combine(directory, "trajectory.tsv")))
            {
                if (reader.ReadLine() != Header) throw new InvalidDataException("Trajectory header differs");
                string line;
                while ((line = reader.ReadLine()) != null)
                {
                    var f = TrajectoryFrame.Decode(line);
                    if (f.Time <= last || rows.Count >= 216000) throw new InvalidDataException("Trajectory timeline or sample bound");
                    if (last >= 0 && Math.Abs(f.Time - last - double.Parse(meta["fixedDeltaTime"], CultureInfo.InvariantCulture)) > .0001)
                        throw new InvalidDataException("Trajectory has a missing or repeated physics step");
                    rows.Add(f); last = f.Time;
                }
            }
            if (rows.Count < 2) throw new InvalidDataException("Trajectory needs at least two frames");
            return new TrajectoryTape(meta, rows.ToArray());
        }
        public void ValidateTarget(string game, string gameHash, string scenario, double fixedDeltaTime)
        {
            if (Metadata["game"] != game || Metadata["gameSha256"] != gameHash || Metadata["scenario"] != scenario ||
                Math.Abs(double.Parse(Metadata["fixedDeltaTime"], CultureInfo.InvariantCulture) - fixedDeltaTime) > .000001)
                throw new InvalidDataException("Recorded game build, scenario or physics timestep differs");
        }
        internal static void ValidateMetadata(IDictionary<string, string> metadata)
        {
            foreach (var key in new[] { "contract", "game", "gameSha256", "scenario", "fixedDeltaTime", "captureSource", "physicalOutput" })
                if (!metadata.TryGetValue(key, out var value) || string.IsNullOrWhiteSpace(value)) throw new InvalidDataException("Missing trajectory metadata: " + key);
            if (metadata["contract"] != "dbce.trajectory@1") throw new InvalidDataException("Unsupported trajectory contract");
            if (metadata["gameSha256"].Length != 64 || metadata["gameSha256"].Any(c => !Uri.IsHexDigit(c))) throw new InvalidDataException("Invalid game hash");
            if (!double.TryParse(metadata["fixedDeltaTime"], NumberStyles.Float, CultureInfo.InvariantCulture, out var dt) || double.IsNaN(dt) || dt < .001 || dt > .1)
                throw new InvalidDataException("Invalid physics timestep");
            if (metadata["physicalOutput"] != "true" && metadata["physicalOutput"] != "false") throw new InvalidDataException("Invalid output policy");
            foreach (var pair in metadata)
                if (pair.Key.IndexOfAny(new[] { '=', '\n', '\r' }) >= 0 || pair.Value.IndexOfAny(new[] { '\n', '\r' }) >= 0)
                    throw new InvalidDataException("Invalid metadata text");
        }
        private static Dictionary<string, string> ReadMetadata(string path)
        {
            if (new FileInfo(path).Length > 65536) throw new InvalidDataException("Metadata too large");
            var result = new Dictionary<string, string>();
            foreach (var line in File.ReadLines(path))
            {
                int eq = line.IndexOf('=');
                if (eq <= 0 || result.ContainsKey(line.Substring(0, eq))) throw new InvalidDataException("Invalid/duplicate metadata key");
                result.Add(line.Substring(0, eq), line.Substring(eq + 1));
            }
            return result;
        }
    }

    public sealed class TrajectoryRecorder : IDisposable
    {
        private readonly string directory;
        private readonly BufferedTapeWriter writer;
        private readonly double dt;
        private double last = -1;
        private bool closed, failed;
        public int Count { get; private set; }
        public TrajectoryRecorder(string directory, IDictionary<string, string> metadata)
        {
            TrajectoryTape.ValidateMetadata(metadata);
            this.directory = Path.GetFullPath(directory);
            // A fresh directory is the transaction boundary; never replace evidence.
            if (Directory.Exists(directory)) throw new IOException("Capture directory already exists");
            Directory.CreateDirectory(directory);
            File.WriteAllLines(Path.Combine(directory, "trajectory.meta"), metadata.OrderBy(p => p.Key).Select(p => p.Key + "=" + p.Value), new UTF8Encoding(false));
            dt = double.Parse(metadata["fixedDeltaTime"], CultureInfo.InvariantCulture);
            writer = new BufferedTapeWriter(Path.Combine(directory, "trajectory.tsv"));
            writer.WriteLine(TrajectoryTape.Header);
        }
        public void Write(TrajectoryFrame frame)
        {
            if (closed || failed) throw new InvalidOperationException("Capture is closed or failed");
            try
            {
                frame.Validate();
                if (Count >= 216000 || (last >= 0 && Math.Abs(frame.Time - last - dt) > .0001)) throw new InvalidDataException("Missing physics step or capture bound exceeded");
                writer.WriteLine(frame.Encode()); last = frame.Time; Count++;
            }
            catch { failed = true; throw; }
        }
        public void Complete(params string[] additionalFiles)
        {
            if (closed) throw new InvalidOperationException("Capture already closed");
            try
            {
                writer.Dispose(); closed = true;
                if (failed || Count < 2) throw new InvalidDataException("Incomplete or empty trajectory");
                ArtifactSeal.Complete(directory, new[] { "trajectory.meta", "trajectory.tsv" }.Concat(additionalFiles).ToArray());
            }
            catch { failed = true; throw; }
        }
        public void Dispose() { if (!closed) { failed = true; closed = true; writer.Dispose(); } }
    }
}
