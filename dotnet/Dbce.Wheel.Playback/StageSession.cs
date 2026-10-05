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
        // Continuous playback eligibility; unlike Ready, allows adapter-owned replay state.
        bool PlaybackAllowed { get; }
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
        private double seconds, armedAt, lastStepAt, captureOrigin, fixedDeltaTime;
        public string Status { get; private set; } = "idle";
        public bool Active => armed || recorder != null || player != null;
        // Accepted replay, including its armed wait. False for recording and
        // every terminal state; consumers must not infer mode from status text.
        public bool ReplayActive => Active && !recording;
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
            try
            {
                if (armed) { if (now - armedAt > 180) Abort("stage was not ready within 180 seconds"); return; }
                if (!Active) return;
                // Pause can stop FixedUpdate entirely. Release ownership from Update
                // immediately instead of waiting for another physics callback.
                if (adapter.Scenario != scenario) { Abort("stage or vehicle changed"); return; }
                if (!SameTimestep()) { Abort("physics timestep changed"); return; }
                if (player != null && !adapter.PlaybackAllowed) { Abort("playback eligibility lost"); return; }
                if (recorder != null && !adapter.Ready) { Stop(); return; }
                if (now - lastStepAt > 30) Abort("no physics callback for 30 seconds");
            }
            catch (Exception ex) { Abort("heartbeat: " + ex.Message); }
        }
        private bool SameTimestep()
        {
            double current = adapter.FixedDeltaTime;
            return !double.IsNaN(current) && current >= .001 && current <= .1 && Math.Abs(current - fixedDeltaTime) <= 1e-7;
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
                    fixedDeltaTime = adapter.FixedDeltaTime;
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
                if (!SameTimestep()) { Abort("physics timestep changed"); return; }
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
                    if (!adapter.PlaybackAllowed) { Abort("playback eligibility lost"); return; }
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
            try
            {
                if (directory != null && Directory.Exists(directory)) File.WriteAllText(Path.Combine(directory, recording ? "incomplete.txt" : "result.txt"), Status);
            }
            catch (Exception ex) { Status += "; could not persist failure: " + ex.Message; }
        }
        public void Dispose() { if (Active) Stop(); }
    }
}
