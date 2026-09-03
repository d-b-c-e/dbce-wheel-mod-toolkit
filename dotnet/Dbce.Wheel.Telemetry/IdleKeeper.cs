using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>
    /// Keeps a dashboard alive while no game is driving: emits zeroed frames at a
    /// low rate so gauges rest at zero instead of freezing on the last value.
    /// </summary>
    /// <remarks>
    /// <para>
    /// SimHub and most dash software hold the last received values when a stream
    /// stops, so a game that exits (or crashes) leaves the dash stuck. Lifted from
    /// cruisn-collection's <c>ForzaKeeper</c> (2026-08), which learned two things
    /// the hard way, both encoded here:
    /// </para>
    /// <list type="bullet">
    /// <item>The keeper frame must carry the <b>same engine constants</b> as the
    /// game's frames. A fully zero packet (<c>EngineMaxRpm = 0</c>) is degenerate
    /// and can wedge SimHub's normalisation state so it ignores the real stream
    /// afterwards.</item>
    /// <item><c>Gear</c> is 1, because 0 renders as reverse and Forza has no code
    /// for neutral.</item>
    /// </list>
    /// <para>
    /// <see cref="SourceSentinel"/> (<c>'K'</c>) is stamped into the unused trailing
    /// byte of the Horizon layout so a probe can tell keeper frames from game frames.
    /// The keeper is inert while <see cref="GameActive"/> is set.
    /// </para>
    /// </remarks>
    public sealed class IdleKeeper : IDisposable
    {
        /// <summary>Marker written to byte 323 of Horizon frames sent by the keeper.</summary>
        public const byte SourceSentinel = 0x4B;

        private readonly List<IPEndPoint> _targets = new List<IPEndPoint>();
        private readonly ForzaLayout _layout;
        private readonly int _intervalMs;
        private readonly TelemetryFrame _template;
        private UdpClient _client;
        private Thread _thread;
        private volatile bool _stop;

        /// <summary>Set while a game is emitting its own frames; the keeper stays quiet.</summary>
        public readonly ManualResetEvent GameActive = new ManualResetEvent(false);

        /// <summary>Frames sent so far.</summary>
        public long FramesSent { get; private set; }

        /// <param name="targets">"host:port" entries. Empty host means 127.0.0.1.</param>
        /// <param name="engineMaxRpm">Must match what the game's frames carry.</param>
        /// <param name="engineIdleRpm">Must match what the game's frames carry.</param>
        /// <param name="layout">Packet layout the dash expects.</param>
        /// <param name="rateHz">Emission rate while idle; 10 Hz is plenty.</param>
        public IdleKeeper(IEnumerable<string> targets, float engineMaxRpm, float engineIdleRpm,
                          ForzaLayout layout = ForzaLayout.Horizon324, int rateHz = 10)
        {
            if (targets == null) throw new ArgumentNullException(nameof(targets));
            foreach (var t in targets)
            {
                if (string.IsNullOrWhiteSpace(t)) continue;
                var parts = t.Trim().Split(':');
                string host = parts.Length > 1 && parts[0].Length > 0 ? parts[0] : "127.0.0.1";
                int port = int.Parse(parts[parts.Length - 1]);
                _targets.Add(new IPEndPoint(IPAddress.Parse(host == "localhost" ? "127.0.0.1" : host), port));
            }
            _layout = layout;
            _intervalMs = Math.Max(20, 1000 / Math.Max(1, rateHz));
            _template = new TelemetryFrame
            {
                IsRaceOn = true,          // the dash stays live, showing zeros
                EngineMaxRpm = engineMaxRpm,
                EngineIdleRpm = engineIdleRpm,
                Gear = 1,
            };
        }

        /// <summary>Builds the exact bytes one keeper frame carries, for tests and probes.</summary>
        public byte[] BuildFrame(uint timestampMs)
        {
            var f = _template;
            f.TimestampMs = timestampMs;
            var buf = new byte[ForzaPacket.SizeOf(_layout)];
            ForzaPacket.Write(f, buf, _layout);
            if (_layout == ForzaLayout.Horizon324) buf[ForzaPacket.OffSourceSentinel] = SourceSentinel;
            return buf;
        }

        /// <summary>Starts the background emitter. No-op without targets.</summary>
        public void Start()
        {
            if (_targets.Count == 0 || _thread != null) return;
            _client = new UdpClient();
            _thread = new Thread(Loop) { IsBackground = true, Name = "Dbce.Wheel.IdleKeeper" };
            _thread.Start();
        }

        private void Loop()
        {
            uint ms = 0;
            while (!_stop)
            {
                if (!GameActive.WaitOne(0))
                {
                    ms = unchecked(ms + (uint)_intervalMs);
                    var buf = BuildFrame(ms);
                    foreach (var ep in _targets)
                    {
                        try { _client.Send(buf, buf.Length, ep); FramesSent++; }
                        catch (SocketException) { /* target absent; keep going */ }
                        catch (ObjectDisposedException) { return; }
                    }
                }
                Thread.Sleep(_intervalMs);
            }
        }

        public void Dispose()
        {
            _stop = true;
            try { _client?.Close(); } catch { }
            try { _thread?.Join(500); } catch { }
            GameActive.Dispose();
        }
    }
}
