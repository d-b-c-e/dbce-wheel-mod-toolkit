using System;
using System.Collections.Generic;
using System.Runtime.Serialization;

namespace Dbce.Wheel.Recording
{
    public enum SessionRecordKind { Metadata, Sample, Marker, Footer }
    public enum RecorderStatus { Recording, Stopping, Completed, LimitReached, Faulted }

    [DataContract]
    public sealed class SessionMetadata
    {
        [DataMember(Name = "game", IsRequired = true)] public string Game { get; set; }
        [DataMember(Name = "pluginVersion", IsRequired = true)] public string PluginVersion { get; set; }
        [DataMember(Name = "toolkitVersion", IsRequired = true)] public string ToolkitVersion { get; set; }
        [DataMember(Name = "startedUtc", IsRequired = true)] public DateTime StartedUtc { get; set; } = DateTime.UtcNow;
        [DataMember(Name = "properties", IsRequired = true)] public Dictionary<string, string> Properties { get; set; } = new Dictionary<string, string>();
        [DataMember(Name = "channelUnits", IsRequired = true)] public Dictionary<string, string> ChannelUnits { get; set; } = new Dictionary<string, string>();
    }

    [DataContract]
    public sealed class SessionSample
    {
        [DataMember(Name = "sequence", IsRequired = true)] public long Sequence { get; set; }
        [DataMember(Name = "elapsedSeconds", IsRequired = true)] public double ElapsedSeconds { get; set; }
        [DataMember(Name = "channels", IsRequired = true)] public Dictionary<string, double> Channels { get; set; }
    }

    [DataContract]
    public sealed class SessionMarker
    {
        [DataMember(Name = "elapsedSeconds", IsRequired = true)] public double ElapsedSeconds { get; set; }
        [DataMember(Name = "name", IsRequired = true)] public string Name { get; set; }
        [DataMember(Name = "detail", IsRequired = true)] public string Detail { get; set; }
    }

    /// <summary>A detached snapshot. During recording, fields can reflect adjacent instants.</summary>
    [DataContract]
    public sealed class RecordingCounters
    {
        [DataMember(Name = "acceptedSamples", IsRequired = true)] public long AcceptedSamples { get; set; }
        [DataMember(Name = "acceptedMarkers", IsRequired = true)] public long AcceptedMarkers { get; set; }
        [DataMember(Name = "writtenSamples", IsRequired = true)] public long WrittenSamples { get; set; }
        [DataMember(Name = "writtenMarkers", IsRequired = true)] public long WrittenMarkers { get; set; }
        [DataMember(Name = "droppedSamples", IsRequired = true)] public long DroppedSamples { get; set; }
        [DataMember(Name = "droppedMarkers", IsRequired = true)] public long DroppedMarkers { get; set; }
        [DataMember(Name = "queueFullCount", IsRequired = true)] public long QueueFullCount { get; set; }
        [DataMember(Name = "contentionCount", IsRequired = true)] public long ContentionCount { get; set; }
        [DataMember(Name = "invalidCount", IsRequired = true)] public long InvalidCount { get; set; }
        [DataMember(Name = "limitCount", IsRequired = true)] public long LimitCount { get; set; }
        [DataMember(Name = "errorCount", IsRequired = true)] public long ErrorCount { get; set; }
        /// <summary>Calls after acceptance closes; the footer freezes this at finalization.</summary>
        [DataMember(Name = "rejectedAfterStop", IsRequired = true)] public long RejectedAfterStop { get; set; }
        /// <summary>UTF-8 bytes including line endings before the footer (not the footer itself).</summary>
        [DataMember(Name = "dataBytesWritten", IsRequired = true)] public long DataBytesWritten { get; set; }
    }

    [DataContract]
    public sealed class SessionFooter
    {
        [DataMember(Name = "elapsedSeconds", IsRequired = true)] public double ElapsedSeconds { get; set; }
        /// <summary>True for an orderly explicit stop. Inspect drops even when true.</summary>
        [DataMember(Name = "completed", IsRequired = true)] public bool Completed { get; set; }
        /// <summary>stopped, durationLimit, fileSizeLimit, or recordSizeLimit.</summary>
        [DataMember(Name = "stopReason", IsRequired = true)] public string StopReason { get; set; }
        [DataMember(Name = "counts", IsRequired = true)] public RecordingCounters Counts { get; set; }
    }

    [DataContract]
    public sealed class SessionRecord
    {
        public const string CurrentSchema = "dbce.wheel.session";
        public const int CurrentVersion = 1;
        [IgnoreDataMember] public SessionRecordKind Kind { get; set; }
        [DataMember(Name = "kind", IsRequired = true, Order = 0)]
        private string WireKind
        {
            get
            {
                switch (Kind)
                {
                    case SessionRecordKind.Metadata: return "metadata";
                    case SessionRecordKind.Sample: return "sample";
                    case SessionRecordKind.Marker: return "marker";
                    case SessionRecordKind.Footer: return "footer";
                    default: throw new SerializationException("Unknown record kind.");
                }
            }
            set
            {
                switch (value)
                {
                    case "metadata": Kind = SessionRecordKind.Metadata; break;
                    case "sample": Kind = SessionRecordKind.Sample; break;
                    case "marker": Kind = SessionRecordKind.Marker; break;
                    case "footer": Kind = SessionRecordKind.Footer; break;
                    default: throw new SerializationException("Unknown record kind: " + value);
                }
            }
        }
        [DataMember(Name = "schema", EmitDefaultValue = false)] public string Schema { get; set; }
        [DataMember(Name = "version", EmitDefaultValue = false)] public int Version { get; set; }
        [DataMember(Name = "metadata", EmitDefaultValue = false)] public SessionMetadata Metadata { get; set; }
        [DataMember(Name = "sample", EmitDefaultValue = false)] public SessionSample Sample { get; set; }
        [DataMember(Name = "marker", EmitDefaultValue = false)] public SessionMarker Marker { get; set; }
        [DataMember(Name = "footer", EmitDefaultValue = false)] public SessionFooter Footer { get; set; }
    }
}
