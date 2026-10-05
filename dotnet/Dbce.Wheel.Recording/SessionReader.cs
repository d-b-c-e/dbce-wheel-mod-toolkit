using System;
using System.Collections.Generic;
using System.IO;

namespace Dbce.Wheel.Recording
{
    public enum SessionReadError { InvalidJson, InvalidSchema, UnsupportedVersion, InvalidRecord, InvalidOrder, InvalidFooter, MissingFooter, TruncatedLine, RecordTooLarge }

    public sealed class SessionReadException : IOException
    {
        public SessionReadError Error { get; }
        public long LineNumber { get; }
        internal SessionReadException(SessionReadError error, long line, string message, Exception inner = null)
            : base("Session line " + line + ": " + message, inner) { Error = error; LineNumber = line; }
    }

    /// <summary>Streaming validation for replay consumers. Enumerate to the end to validate completion.</summary>
    public static class SessionReader
    {
        /// <summary>Yields independent DTOs. Invalid, unsupported or interrupted files throw SessionReadException.
        /// A footer is only yielded after checking that it is the final line.</summary>
        public static IEnumerable<SessionRecord> Read(string path)
        {
            using (var stream = new BufferedStream(new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite), 16384))
            {
                long line = 0, bytesBeforeFooter = 0, sampleCount = 0, markerCount = 0;
                double lastTime = 0, lastSampleTime = -1;
                bool headerSeen = false;
                SessionRecord footer = null;
                while (true)
                {
                    byte[] bytes = ReadLine(stream, line + 1, out int byteCount);
                    if (bytes == null) break;
                    line++;
                    if (footer != null) throw Error(SessionReadError.InvalidFooter, line, "Data follows the footer.");
                    SessionRecord record;
                    try { record = JsonLines.Decode(bytes); }
                    catch (Exception error) when (error is System.Runtime.Serialization.SerializationException || error is System.Xml.XmlException ||
                        error is ArgumentException || error is FormatException || error is OverflowException)
                    { throw new SessionReadException(SessionReadError.InvalidJson, line, "Malformed JSON record.", error); }
                    if (record == null) throw Error(SessionReadError.InvalidRecord, line, "Null record.");
                    int payloads = (record.Metadata == null ? 0 : 1) + (record.Sample == null ? 0 : 1) + (record.Marker == null ? 0 : 1) + (record.Footer == null ? 0 : 1);
                    if (payloads != 1) throw Error(SessionReadError.InvalidRecord, line, "Exactly one matching payload is required.");
                    if (!headerSeen)
                    {
                        if (record.Kind != SessionRecordKind.Metadata || record.Metadata == null || record.Schema != SessionRecord.CurrentSchema)
                            throw Error(SessionReadError.InvalidSchema, line, "Expected a dbce.wheel.session metadata header.");
                        if (record.Version != SessionRecord.CurrentVersion) throw Error(SessionReadError.UnsupportedVersion, line, "Unsupported schema version " + record.Version + ".");
                        if (!Validation.Metadata(record.Metadata)) throw Error(SessionReadError.InvalidRecord, line, "Invalid metadata.");
                        headerSeen = true;
                    }
                    else
                    {
                        if (record.Schema != null || record.Version != 0) throw Error(SessionReadError.InvalidRecord, line, "Schema/version belong only in the header.");
                        double elapsed;
                        switch (record.Kind)
                        {
                            case SessionRecordKind.Sample:
                                var sample = record.Sample;
                                if (sample == null || !Validation.Channels(sample.Channels, Validation.MaxEntries)) throw Error(SessionReadError.InvalidRecord, line, "Invalid sample channels.");
                                if (sample.Sequence != sampleCount || sample.ElapsedSeconds <= lastSampleTime) throw Error(SessionReadError.InvalidOrder, line, "Sample sequence must be contiguous from zero and sample time must increase.");
                                elapsed = sample.ElapsedSeconds; lastSampleTime = elapsed; sampleCount++; break;
                            case SessionRecordKind.Marker:
                                var marker = record.Marker;
                                if (marker == null || !Validation.Name(marker.Name) || !Validation.Text(marker.Detail)) throw Error(SessionReadError.InvalidRecord, line, "Invalid marker.");
                                elapsed = marker.ElapsedSeconds; markerCount++; break;
                            case SessionRecordKind.Footer:
                                if (record.Footer == null) throw Error(SessionReadError.InvalidRecord, line, "Missing footer payload.");
                                elapsed = record.Footer.ElapsedSeconds;
                                ValidateFooter(record.Footer, sampleCount, markerCount, bytesBeforeFooter, lastTime, line);
                                footer = record; break;
                            default: throw Error(SessionReadError.InvalidOrder, line, "Duplicate or misplaced metadata header.");
                        }
                        if (!Validation.Time(elapsed) || elapsed < lastTime) throw Error(SessionReadError.InvalidOrder, line, "Elapsed time is nonfinite, negative or out of order.");
                        lastTime = elapsed;
                    }
                    if (footer == null) { bytesBeforeFooter += byteCount; yield return record; }
                }
                if (footer == null) throw Error(SessionReadError.MissingFooter, line + 1, "Missing footer; recording is interrupted or incomplete.");
                yield return footer;
            }
        }

        private static void ValidateFooter(SessionFooter footer, long samples, long markers, long bytes, double time, long line)
        {
            var c = footer.Counts;
            bool normal = footer.StopReason == "stopped";
            bool limited = footer.StopReason == "durationLimit" || footer.StopReason == "fileSizeLimit" || footer.StopReason == "recordSizeLimit";
            if (c == null || (!normal && !limited) || footer.Completed != normal || footer.ElapsedSeconds != time)
                throw Error(SessionReadError.InvalidFooter, line, "Invalid completion status or footer time.");
            foreach (long value in new[] { c.AcceptedSamples, c.AcceptedMarkers, c.WrittenSamples, c.WrittenMarkers, c.DroppedSamples, c.DroppedMarkers,
                c.QueueFullCount, c.ContentionCount, c.InvalidCount, c.LimitCount, c.ErrorCount, c.RejectedAfterStop, c.DataBytesWritten })
                if (value < 0) throw Error(SessionReadError.InvalidFooter, line, "Negative counter.");
            if (c.WrittenSamples != samples || c.WrittenMarkers != markers || c.DataBytesWritten != bytes || c.AcceptedSamples < samples || c.AcceptedMarkers < markers ||
                c.DroppedSamples < c.AcceptedSamples - samples || c.DroppedMarkers < c.AcceptedMarkers - markers || c.ErrorCount != 0 ||
                (normal ? c.LimitCount != 0 : c.LimitCount != 1) || (normal && (c.AcceptedSamples != samples || c.AcceptedMarkers != markers)))
                throw Error(SessionReadError.InvalidFooter, line, "Footer counts disagree with the recording.");
            // Decimal arithmetic avoids overflow on corrupt Int64 counters.
            decimal rejected = (decimal)c.DroppedSamples + c.DroppedMarkers - (c.AcceptedSamples - samples) - (c.AcceptedMarkers - markers);
            decimal knownRejections = (decimal)c.QueueFullCount + c.ContentionCount + c.InvalidCount;
            if (rejected < knownRejections || rejected > knownRejections + (footer.StopReason == "durationLimit" ? 1 : 0))
                throw Error(SessionReadError.InvalidFooter, line, "Drop counters disagree with rejection reasons.");
        }

        private static byte[] ReadLine(Stream stream, long line, out int byteCount)
        {
            byteCount = 0;
            using (var buffer = new MemoryStream())
            {
                while (true)
                {
                    int value = stream.ReadByte();
                    if (value == -1)
                    {
                        if (byteCount == 0) return null;
                        throw Error(SessionReadError.TruncatedLine, line, "Unterminated JSONL line; recording may have been interrupted.");
                    }
                    byteCount++;
                    if (value == '\n')
                    {
                        byte[] bytes = buffer.ToArray();
                        if (bytes.Length > 0 && bytes[bytes.Length - 1] == '\r') Array.Resize(ref bytes, bytes.Length - 1);
                        return bytes;
                    }
                    if (buffer.Length >= JsonLines.MaxLineBytes) throw Error(SessionReadError.RecordTooLarge, line, "JSONL record exceeds the 1 MiB schema limit.");
                    buffer.WriteByte((byte)value);
                }
            }
        }

        private static SessionReadException Error(SessionReadError kind, long line, string message) => new SessionReadException(kind, line, message);
    }
}
