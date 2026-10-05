using System;

namespace Dbce.Wheel.Recording
{
    public sealed class RecordingOptions
    {
        /// <summary>Queued records, plus at most one record being written.</summary>
        public int QueueCapacity { get; set; } = 2048;
        public double MaxDurationSeconds { get; set; } = 1800;
        /// <summary>Total on-disk cap, including the header, footer and line endings.</summary>
        public long MaxFileBytes { get; set; } = 256L * 1024 * 1024;
        /// <summary>Serialized UTF-8 size per header/sample/marker, excluding the newline.</summary>
        public int MaxRecordBytes { get; set; } = 256 * 1024;
        public int MaxChannelsPerSample { get; set; } = 256;

        internal RecordingOptions Snapshot()
        {
            var copy = (RecordingOptions)MemberwiseClone();
            if (copy.QueueCapacity < 1 || copy.QueueCapacity > 65536) throw new ArgumentOutOfRangeException(nameof(QueueCapacity));
            if (!Validation.Time(copy.MaxDurationSeconds) || copy.MaxDurationSeconds == 0) throw new ArgumentOutOfRangeException(nameof(MaxDurationSeconds));
            if (copy.MaxFileBytes < JsonLines.FooterReserveBytes * 2) throw new ArgumentOutOfRangeException(nameof(MaxFileBytes));
            if (copy.MaxRecordBytes < 1024 || copy.MaxRecordBytes > JsonLines.MaxLineBytes) throw new ArgumentOutOfRangeException(nameof(MaxRecordBytes));
            if (copy.MaxChannelsPerSample < 1 || copy.MaxChannelsPerSample > Validation.MaxEntries) throw new ArgumentOutOfRangeException(nameof(MaxChannelsPerSample));
            return copy;
        }
    }
}
