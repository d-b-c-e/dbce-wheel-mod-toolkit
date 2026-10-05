using System;
using System.Collections.Generic;

namespace Dbce.Wheel.Recording
{
    internal static class Validation
    {
        internal const int MaxEntries = 1024;
        internal const int MaxTextLength = 4096;
        internal const int MaxNameLength = 256;
        internal static bool Finite(double value) => !double.IsNaN(value) && !double.IsInfinity(value);
        internal static bool Time(double value) => Finite(value) && value >= 0;
        internal static bool Name(string value) => !string.IsNullOrWhiteSpace(value) && value.Length <= MaxNameLength;
        internal static bool Text(string value) => value != null && value.Length <= MaxTextLength;

        internal static SessionMetadata Snapshot(SessionMetadata source)
        {
            if (source == null) throw new ArgumentNullException(nameof(source));
            var copy = new SessionMetadata
            {
                Game = source.Game, PluginVersion = source.PluginVersion, ToolkitVersion = source.ToolkitVersion,
                StartedUtc = source.StartedUtc,
                Properties = CopyStrings(source.Properties), ChannelUnits = CopyStrings(source.ChannelUnits)
            };
            if (!Metadata(copy)) throw new ArgumentException("Metadata requires game/version names, a UTC timestamp and bounded, non-null dictionaries.", nameof(source));
            return copy;
        }

        private static Dictionary<string, string> CopyStrings(Dictionary<string, string> source)
        {
            if (!Strings(source)) throw new ArgumentException("Invalid metadata dictionary.");
            return new Dictionary<string, string>(source, StringComparer.Ordinal);
        }

        private static bool Strings(Dictionary<string, string> values)
        {
            if (values == null || values.Count > MaxEntries) return false;
            foreach (var item in values) if (!Name(item.Key) || !Text(item.Value)) return false;
            return true;
        }

        internal static bool Metadata(SessionMetadata value) => value != null && Name(value.Game) && Name(value.PluginVersion) &&
            Name(value.ToolkitVersion) && value.StartedUtc.Kind == DateTimeKind.Utc && value.StartedUtc != DateTime.MinValue &&
            Strings(value.Properties) && Strings(value.ChannelUnits);

        internal static bool Channels(IDictionary<string, double> channels, int maximum)
        {
            if (channels == null || channels.Count == 0 || channels.Count > maximum) return false;
            int count = 0;
            foreach (var item in channels)
                if (++count > maximum || !Name(item.Key) || !Finite(item.Value)) return false;
            return count != 0;
        }
    }
}
