using System;
using System.Collections.Generic;
using System.Linq;

namespace Dbce.Wheel.Playback
{
    /// <summary>A state-gated sequential stream. The adapter supplies state IDs and a monotonic clock.</summary>
    public class SegmentCursor<T>
    {
        private readonly List<List<T>> segments = new List<List<T>>();
        private readonly List<int> markers = new List<int>();
        private readonly Dictionary<int, string> names;
        private readonly string label;
        private readonly Func<float> clock;
        private readonly int earlyLimit;
        private readonly Func<string, bool> required;
        private readonly Dictionary<string, int> skippedBy = new Dictionary<string, int>();
        private int segment = -1, index, early;
        private float waitingSince = -1;
        public const int EarlyLimit = 10, SkipLimit = 3;
        public const float WaitSeconds = 120;

        public SegmentCursor(List<KeyValuePair<int, T>> rows, Dictionary<int, string> names, string label,
            Func<float> clock, int earlyLimit = EarlyLimit, Func<string, bool> required = null)
        {
            this.names = new Dictionary<int, string>(names);
            this.label = label; this.clock = clock; this.earlyLimit = earlyLimit; this.required = required ?? (_ => false);
            if (earlyLimit < 0) throw new ArgumentOutOfRangeException(nameof(earlyLimit));
            foreach (var row in rows)
            {
                if (!names.ContainsKey(row.Key)) throw new ArgumentException("Unknown marker " + row.Key);
                if (markers.Count == 0 || markers[markers.Count - 1] != row.Key)
                { markers.Add(row.Key); segments.Add(new List<T>()); }
                segments[segments.Count - 1].Add(row.Value);
            }
            Total = rows.Count;
        }
        public int Segment => segment;
        public int SegmentCount => segments.Count;
        public int Total { get; }
        public int Consumed { get; private set; }
        public int Skipped { get; private set; }
        public int CurrentMarker => segment >= 0 ? markers[segment] : -1;
        public int NextMarker => segment + 1 < markers.Count ? markers[segment + 1] : -1;
        public int RemainingInSegment => segment >= 0 ? segments[segment].Count - index : 0;
        public bool InLastSegment => segments.Count > 0 && segment == segments.Count - 1;
        public bool PlayedOut => segment >= 0 && index >= segments[segment].Count;
        public bool Finished => InLastSegment && PlayedOut && early == 0;
        public string Failed { get; private set; }
        // Includes ALL remaining segments; stopping early cannot pass a required-stream check.
        public int SkippedWhere(Func<string, bool> predicate)
        {
            int n = skippedBy.Where(p => predicate(p.Key)).Sum(p => p.Value);
            for (int s = Math.Max(0, segment); s < segments.Count; s++)
                if (predicate(names[markers[s]])) n += segments[s].Count - (s == segment ? index : 0);
            return n;
        }
        private bool Take(out T value)
        { value = segments[segment][index++]; Consumed++; waitingSince = -1; return true; }
        private void Advance()
        {
            if (segment >= 0 && !PlayedOut)
            {
                int n = RemainingInSegment; string name = names[markers[segment]];
                Skipped += n; skippedBy[name] = (skippedBy.TryGetValue(name, out var c) ? c : 0) + n;
            }
            segment++; index = 0;
        }
        public bool Next(int liveMarker, out T value)
        {
            value = default;
            if (segments.Count == 0 || Failed != null) return false;
            if (segment >= 0 && !PlayedOut)
            {
                if (markers[segment] == liveMarker) { early = 0; return Take(out value); }
                if (early > 0 && early < earlyLimit && segment > 0 && markers[segment - 1] == liveMarker)
                { early++; return Take(out value); }
            }
            for (int ahead = 1; ahead <= SkipLimit && segment + ahead < segments.Count; ahead++)
            {
                if (markers[segment + ahead] != liveMarker) continue;
                for (int s = Math.Max(0, segment); s < segment + ahead; s++)
                    if (required(names[markers[s]]) && segments[s].Count > (s == segment ? index : 0))
                    { Failed = label + " would skip required rows in '" + names[markers[s]] + "'"; return false; }
                for (int i = 0; i < ahead; i++) Advance();
                early = 0; return Take(out value);
            }
            if (earlyLimit > 0 && segment >= 0 && PlayedOut && markers[segment] == liveMarker && segment + 1 < segments.Count)
            { Advance(); early = 1; return Take(out value); }
            if (Finished) return false;
            if (waitingSince < 0) waitingSince = clock();
            else if (clock() - waitingSince > WaitSeconds)
            {
                int next = early > 0 ? segment : segment + 1;
                Failed = label + " waited " + WaitSeconds + " s for '" + (next < segments.Count ? names[markers[next]] : "end") + "'";
            }
            return false;
        }
    }
}
