using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;

namespace Dbce.Wheel.Playback
{
    public interface IStageAdapter : ITrajectoryTarget
    {
        string Game { get; }
        string GameHash { get; }
        string Scenario { get; }
        double FixedDeltaTime { get; }
        bool Ready { get; }
        // Must latch device/network suppression, without disabling signal sampling.
        void MuteOutputs();
        void StartSignals(string directory);
        // Flush and validate signals; return relative files to include in the seal.
        string[] FinishSignals();
    }

    /// <summary>Shared single-stage orchestration. The game adapter owns scene selection and physics hooks.</summary>
    public sealed class StageSession : IDisposable
    {
        private readonly IStageAdapter adapter;
        private TrajectoryRecorder recorder;
        private TrajectoryTape tape;
        private TrajectoryPlayer player;
        private string directory, scenario;
        private bool recording, armed, signalsStarted;
        private double seconds, armedAt, lastStepAt, captureOrigin;
        public string Status { get; private set; } = "idle";
        public bool Active => armed || recorder != null || player != null;
        public bool Playing => player != null;
        public bool Recording => recorder != null;
        public int Samples => recorder?.Count ?? player?.Applied ?? 0;
        public StageSession(IStageAdapter adapter) { this.adapter = adapter ?? throw new ArgumentNullException(nameof(adapter)); }
        public void Record(string path, double maximumSeconds, double now)
        {
            if (Active) throw new InvalidOperationException("A session is already active");
            adapter.MuteOutputs();
            if (double.IsNaN(maximumSeconds) || maximumSeconds < 1 || maximumSeconds > 1800) throw new ArgumentOutOfRangeException(nameof(maximumSeconds));
            if (Directory.Exists(path) || File.Exists(path)) throw new IOException("Capture already exists");
            directory = Path.GetFullPath(path); recording = true; seconds = maximumSeconds; armedAt = now; armed = true;
            Status = "armed: waiting for a live single-player stage";
        }
        public void Replay(string path, string output, double now)
        {
            if (Active) throw new InvalidOperationException("A session is already active");
            adapter.MuteOutputs();
            var candidate = TrajectoryTape.Read(path);
            if (candidate.Metadata["game"] != adapter.Game || candidate.Metadata["gameSha256"] != adapter.GameHash)
                throw new InvalidDataException("Recorded game build differs");
            if (Directory.Exists(output) || File.Exists(output)) throw new IOException("Playback result already exists");
            directory = Path.GetFullPath(output); Directory.CreateDirectory(directory);
            tape = candidate; recording = false; armedAt = now; armed = true;
            Status = "armed: select " + tape.Metadata["scenario"];
        }
        // A render-thread heartbeat detects missing hook/focus/scene progress too.
        public void Heartbeat(double now)
        {
            if (armed && now - armedAt > 180) Abort("stage was not ready within 180 seconds");
            else if ((recorder != null || player != null) && now - lastStepAt > 30) Abort("no physics callback for 30 seconds");
        }
        public void FixedStep(double now)
        {
            if (!Active) return;
            try
            {
                if (armed)
                {
                    if (!adapter.Ready) return;
                    if (!recording && tape.Metadata["scenario"] != adapter.Scenario) return;
                    scenario = adapter.Scenario;
                    if (recording)
                    {
                        var metadata = new Dictionary<string, string> {
                            ["contract"] = "dbce.trajectory@1", ["game"] = adapter.Game, ["gameSha256"] = adapter.GameHash,
                            ["scenario"] = scenario, ["fixedDeltaTime"] = adapter.FixedDeltaTime.ToString("R", CultureInfo.InvariantCulture),
                            ["captureSource"] = "live-physics", ["physicalOutput"] = "false", ["startedUtc"] = DateTime.UtcNow.ToString("O"),
                            ["scope"] = "single-stage trajectory; original signals stored separately; physical output muted"
                        };
                        recorder = new TrajectoryRecorder(directory, metadata);
                        captureOrigin = double.NaN;
                        signalsStarted = true; adapter.StartSignals(directory);
                    }
                    else
                    {
                        tape.ValidateTarget(adapter.Game, adapter.GameHash, scenario, adapter.FixedDeltaTime);
                        player = new TrajectoryPlayer(tape, adapter);
                    }
                    armed = false; Status = recording ? "recording" : "playing";
                }
                lastStepAt = now;
                if (adapter.Scenario != scenario) { Abort("stage or vehicle changed"); return; }
                if (recorder != null)
                {
                    if (!adapter.Ready || recorder.Count * adapter.FixedDeltaTime >= seconds) { Stop(); return; }
                    var frame = adapter.Read();
                    if (double.IsNaN(captureOrigin)) captureOrigin = frame.Time;
                    frame.Time -= captureOrigin;
                    recorder.Write(frame);
                }
                else if (player != null)
                {
                    if (!player.Step())
                    {
                        Status = "passed: " + player.Applied + " poses; max error " + player.MaximumPositionError.ToString("R", CultureInfo.InvariantCulture) +
                            " m; " + player.MaximumRotationErrorDegrees.ToString("R", CultureInfo.InvariantCulture) + " deg";
                        File.WriteAllText(Path.Combine(directory, "result.txt"), Status);
                        player = null;
                    }
                }
            }
            catch (Exception ex) { Abort(ex.Message); }
        }
        public void Stop()
        {
            if (!Active) return;
            try
            {
                if (recorder != null)
                {
                    string[] files = signalsStarted ? adapter.FinishSignals() : new string[0]; signalsStarted = false;
                    int count = recorder.Count; recorder.Complete(files); recorder = null;
                    Status = "recorded: " + count + " poses; complete seal written";
                }
                else if (player != null) { Abort("stopped before all poses were verified"); return; }
                else Status = "cancelled before stage start";
                armed = false;
            }
            catch (Exception ex) { Abort(ex.Message); }
        }
        public void Abort(string reason)
        {
            armed = false; Status = "failed: " + reason;
            try { player?.Dispose(); } catch (Exception ex) { Status += "; restore: " + ex.Message; }
            player = null;
            try { recorder?.Dispose(); } catch (Exception ex) { Status += "; close: " + ex.Message; }
            recorder = null;
            try { if (signalsStarted) adapter.FinishSignals(); } catch (Exception ex) { Status += "; signals: " + ex.Message; }
            signalsStarted = false;
            if (directory != null && Directory.Exists(directory)) File.WriteAllText(Path.Combine(directory, recording ? "incomplete.txt" : "result.txt"), Status);
        }
        public void Dispose() { if (Active) Stop(); }
    }
}
