using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace Dbce.Wheel.Playback
{
    /// <summary>Explicit expiring local commands. No game launch, network, device or Unity dependency.</summary>
    public sealed class FileSessionControl
    {
        private readonly string root;
        private readonly StageSession session;
        private double nextPoll;
        private string requestId = "none", error;
        public FileSessionControl(string root, StageSession session)
        { this.root = root; this.session = session; Directory.CreateDirectory(root); }
        public void Poll(double now)
        {
            session.Heartbeat(now);
            if (now < nextPoll) return; nextPoll = now + .25;
            string path = Path.Combine(root, "request.txt");
            if (File.Exists(path))
            {
                try
                {
                    if (new FileInfo(path).Length > 8192) throw new InvalidDataException("Request too large");
                    var fields = new Dictionary<string, string>();
                    foreach (var line in File.ReadAllLines(path))
                    {
                        int eq = line.IndexOf('=');
                        if (eq <= 0 || fields.ContainsKey(line.Substring(0, eq))) throw new InvalidDataException("Malformed request");
                        fields.Add(line.Substring(0, eq), line.Substring(eq + 1));
                    }
                    File.Delete(path); // One attempt only, including rejected/expired requests.
                    if (!fields.TryGetValue("id", out var id) || !Guid.TryParseExact(id, "N", out _)) throw new InvalidDataException("Invalid request ID");
                    requestId = id;
                    if (!fields.TryGetValue("expiresUtc", out var expiry) || !DateTimeOffset.TryParseExact(expiry, "O", CultureInfo.InvariantCulture, DateTimeStyles.None, out var until) ||
                        until < DateTimeOffset.UtcNow || until > DateTimeOffset.UtcNow.AddMinutes(5)) throw new InvalidDataException("Expired or invalid request deadline");
                    string action = fields["action"];
                    switch (action)
                    {
                        case "record": session.Record(fields["path"], double.Parse(fields["seconds"], CultureInfo.InvariantCulture), now); break;
                        case "replay": session.Replay(fields["path"], fields["output"], now); break;
                        case "stop": session.Stop(); break;
                        default: throw new InvalidDataException("Unknown session action");
                    }
                    error = null;
                }
                catch (Exception ex) { error = ex.Message; if (File.Exists(path)) File.Delete(path); }
            }
            string tmp = Path.Combine(root, "status.tmp"), status = Path.Combine(root, "status.txt");
            File.WriteAllLines(tmp, new[] { "id=" + requestId, "utc=" + DateTime.UtcNow.ToString("O"), "active=" + session.Active,
                "samples=" + session.Samples, "status=" + session.Status.Replace('\n', ' ').Replace('\r', ' '), "error=" + (error ?? "").Replace('\n', ' ').Replace('\r', ' ') });
            if (File.Exists(status)) File.Delete(status);
            File.Move(tmp, status);
        }
    }
}
