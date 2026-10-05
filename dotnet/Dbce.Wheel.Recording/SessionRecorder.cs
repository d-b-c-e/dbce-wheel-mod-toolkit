using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;

[assembly: InternalsVisibleTo("Dbce.Wheel.Recording.Tests")]

namespace Dbce.Wheel.Recording
{
    /// <summary>Bounded, best-effort diagnostic capture. Producer calls never wait for the writer.</summary>
    public sealed class SessionRecorder : IDisposable
    {
        private readonly object gate = new object();
        private readonly ConcurrentQueue<SessionRecord> queue = new ConcurrentQueue<SessionRecord>();
        private readonly AutoResetEvent wake;
        private readonly RecordingOptions options;
        private readonly Stream output;
        private readonly byte[] header;
        private readonly Thread worker;
        private readonly Stopwatch clock = Stopwatch.StartNew();
        private int accepting = 1, activeCalls, pending, status = (int)RecorderStatus.Recording;
        private string stopReason = "stopped", lastError;
        private double lastQueuedTime = -1, lastSampleTime = -1, lastWrittenTime;
        private long acceptedSamples, acceptedMarkers, writtenSamples, writtenMarkers, droppedSamples, droppedMarkers;
        private long queueFullCount, contentionCount, invalidCount, limitCount, errorCount, rejectedAfterStop, dataBytesWritten;

        /// <summary>Creates a new file; never overwrites an existing recording. Parent directory must exist.</summary>
        public SessionRecorder(string filePath, SessionMetadata metadata, RecordingOptions options = null)
            : this(filePath, null, metadata, options) { }

        // Stream injection permits deterministic slow/failing disk tests without exposing a second public API.
        internal SessionRecorder(Stream stream, SessionMetadata metadata, RecordingOptions options = null)
            : this(null, stream, metadata, options) { }

        private SessionRecorder(string filePath, Stream stream, SessionMetadata metadata, RecordingOptions options)
        {
            this.options = (options ?? new RecordingOptions()).Snapshot();
            header = JsonLines.Encode(new SessionRecord
            {
                Kind = SessionRecordKind.Metadata, Schema = SessionRecord.CurrentSchema, Version = SessionRecord.CurrentVersion,
                Metadata = Validation.Snapshot(metadata)
            });
            if (header.Length > this.options.MaxRecordBytes || header.Length + 1L > this.options.MaxFileBytes - JsonLines.FooterReserveBytes)
                throw new ArgumentException("Metadata exceeds the recording's size limit.", nameof(metadata));
            output = stream ?? new FileStream(filePath, FileMode.CreateNew, FileAccess.Write, FileShare.Read, 16384);
            try
            {
                if (!output.CanWrite) throw new ArgumentException("Stream must be writable.", nameof(stream));
                wake = new AutoResetEvent(false);
                worker = new Thread(WriteLoop) { IsBackground = true, Name = "Dbce session writer" };
                worker.Start();
            }
            catch { output.Dispose(); wake?.Dispose(); throw; }
        }

        public RecorderStatus Status => (RecorderStatus)Volatile.Read(ref status);
        public string LastError => Volatile.Read(ref lastError);
        public bool IsRecording => Volatile.Read(ref accepting) != 0;
        public long WrittenSamples => Interlocked.Read(ref writtenSamples);
        public long DroppedSamples => Interlocked.Read(ref droppedSamples);
        public string Error => LastError;
        public string StopReason => LastError != null ? "error" : IsRecording ? "recording" : Volatile.Read(ref stopReason);

        /// <summary>Copies finite channel values. Time must be nonnegative and monotonic across samples/markers;
        /// sample times must strictly increase. Do not mutate the dictionary concurrently with this call.</summary>
        public bool TryRecord(double elapsedSeconds, IDictionary<string, double> channels) => TryAdd(elapsedSeconds, channels, null, null, true);

        public bool TryMark(double elapsedSeconds, string name, string detail) => TryAdd(elapsedSeconds, null, name, detail, false);

        private bool TryAdd(double elapsed, IDictionary<string, double> channels, string name, string detail, bool sample)
        {
            if (Volatile.Read(ref accepting) == 0) { Interlocked.Increment(ref rejectedAfterStop); return false; }
            Interlocked.Increment(ref activeCalls);
            bool entered = false;
            try
            {
                if (Volatile.Read(ref accepting) == 0) { Interlocked.Increment(ref rejectedAfterStop); return false; }
                entered = Monitor.TryEnter(gate);
                if (!entered)
                {
                    Interlocked.Increment(ref contentionCount); Drop(sample); return false;
                }
                if (accepting == 0) { Interlocked.Increment(ref rejectedAfterStop); return false; }
                if (!Validation.Time(elapsed) || elapsed < lastQueuedTime || (sample && elapsed <= lastSampleTime))
                    return Invalid(sample);
                if (elapsed > options.MaxDurationSeconds || clock.Elapsed.TotalSeconds >= options.MaxDurationSeconds)
                {
                    Drop(sample); CloseAcceptance("durationLimit"); return false;
                }
                if (Volatile.Read(ref pending) >= options.QueueCapacity)
                {
                    Interlocked.Increment(ref queueFullCount); Drop(sample); return false;
                }
                SessionRecord record;
                if (sample)
                {
                    // Copy before enqueue. All work here is bounded by the entry/name limits.
                    if (channels == null || channels.Count == 0 || channels.Count > options.MaxChannelsPerSample) return Invalid(true);
                    var copy = new Dictionary<string, double>(channels.Count, StringComparer.Ordinal);
                    try
                    {
                        foreach (var pair in channels)
                        {
                            if (copy.Count >= options.MaxChannelsPerSample || !Validation.Name(pair.Key) || !Validation.Finite(pair.Value)) return Invalid(true);
                            copy.Add(pair.Key, pair.Value);
                        }
                    }
                    catch (InvalidOperationException) { return Invalid(true); }
                    catch (ArgumentException) { return Invalid(true); }
                    if (copy.Count == 0) return Invalid(true);
                    record = new SessionRecord { Kind = SessionRecordKind.Sample, Sample = new SessionSample
                    { Sequence = acceptedSamples, ElapsedSeconds = elapsed, Channels = copy } };
                    Interlocked.Increment(ref acceptedSamples);
                    lastSampleTime = elapsed;
                }
                else
                {
                    if (!Validation.Name(name) || !Validation.Text(detail)) return Invalid(false);
                    record = new SessionRecord { Kind = SessionRecordKind.Marker, Marker = new SessionMarker
                    { ElapsedSeconds = elapsed, Name = name, Detail = detail } };
                    Interlocked.Increment(ref acceptedMarkers);
                }
                lastQueuedTime = elapsed;
                Interlocked.Increment(ref pending);
                queue.Enqueue(record);
                wake.Set();
                return true;
            }
            finally
            {
                if (entered) Monitor.Exit(gate);
                Interlocked.Decrement(ref activeCalls);
            }
        }

        private bool Invalid(bool sample) { Interlocked.Increment(ref invalidCount); Drop(sample); return false; }
        private void Drop(bool sample)
        {
            if (sample) Interlocked.Increment(ref droppedSamples);
            else Interlocked.Increment(ref droppedMarkers);
        }

        // Does not acquire the producer gate: even a blocked caller-owned dictionary
        // enumerator cannot make the GUI wait indefinitely in Stop.
        private void CloseAcceptance(string reason)
        {
            if (reason != "stopped" && Interlocked.CompareExchange(ref stopReason, reason, "stopped") == "stopped")
            {
                Interlocked.Increment(ref limitCount);
            }
            Volatile.Write(ref accepting, 0);
            Interlocked.CompareExchange(ref status, (int)RecorderStatus.Stopping, (int)RecorderStatus.Recording);
        }

        /// <summary>Stops acceptance immediately; waits at most 250 ms. If Status is still Stopping,
        /// the worker retains ownership of the file and continues draining. Check Status/Error before consuming it.</summary>
        public void Stop() => Stop(TimeSpan.FromMilliseconds(250));

        /// <summary>Returns true when drain/close finished (including failure), false on timeout.
        /// Timeout never abandons the queue or disposes a stream in use. Inspect Status/Error for success.</summary>
        public bool Stop(TimeSpan timeout)
        {
            if (!Validation.Time(timeout.TotalMilliseconds) || timeout.TotalMilliseconds > int.MaxValue)
                throw new ArgumentOutOfRangeException(nameof(timeout));
            CloseAcceptance("stopped");
            return worker.Join(timeout);
        }

        public void Dispose() => Stop();

        public RecordingCounters GetCounters() => new RecordingCounters
        {
            AcceptedSamples = Interlocked.Read(ref acceptedSamples), AcceptedMarkers = Interlocked.Read(ref acceptedMarkers),
            WrittenSamples = Interlocked.Read(ref writtenSamples), WrittenMarkers = Interlocked.Read(ref writtenMarkers),
            DroppedSamples = Interlocked.Read(ref droppedSamples), DroppedMarkers = Interlocked.Read(ref droppedMarkers),
            QueueFullCount = Interlocked.Read(ref queueFullCount), ContentionCount = Interlocked.Read(ref contentionCount),
            InvalidCount = Interlocked.Read(ref invalidCount), LimitCount = Interlocked.Read(ref limitCount),
            ErrorCount = Interlocked.Read(ref errorCount), RejectedAfterStop = Interlocked.Read(ref rejectedAfterStop),
            DataBytesWritten = Interlocked.Read(ref dataBytesWritten)
        };

        private void WriteData(byte[] bytes)
        {
            output.Write(bytes, 0, bytes.Length); output.WriteByte((byte)'\n');
            Interlocked.Add(ref dataBytesWritten, bytes.Length + 1L);
        }

        private void WriteLoop()
        {
            SessionRecord current = null;
            try
            {
                WriteData(header);
                bool discard = false;
                while (true)
                {
                    if (Volatile.Read(ref accepting) != 0 && clock.Elapsed.TotalSeconds >= options.MaxDurationSeconds)
                        lock (gate) { if (accepting != 0) CloseAcceptance("durationLimit"); }
                    if (queue.TryDequeue(out current))
                    {
                        Interlocked.Decrement(ref pending);
                        if (!discard)
                        {
                            byte[] bytes = JsonLines.Encode(current);
                            if (bytes.Length > options.MaxRecordBytes || bytes.Length + 1L > options.MaxFileBytes - JsonLines.FooterReserveBytes - dataBytesWritten)
                            {
                                lock (gate) CloseAcceptance(bytes.Length > options.MaxRecordBytes ? "recordSizeLimit" : "fileSizeLimit");
                                discard = true;
                            }
                            else
                            {
                                WriteData(bytes);
                                if (current.Kind == SessionRecordKind.Sample)
                                {
                                    Interlocked.Increment(ref writtenSamples); lastWrittenTime = current.Sample.ElapsedSeconds;
                                }
                                else { Interlocked.Increment(ref writtenMarkers); lastWrittenTime = current.Marker.ElapsedSeconds; }
                            }
                        }
                        if (discard) Drop(current.Kind == SessionRecordKind.Sample);
                        current = null;
                        continue;
                    }
                    if (Volatile.Read(ref accepting) == 0)
                    {
                        WaitForProducers();
                        if (queue.IsEmpty) break;
                        continue;
                    }
                    wake.WaitOne(100);
                }
                WaitForProducers();
                // All producers that passed the acceptance check have finished enqueuing.
                var footer = JsonLines.Encode(new SessionRecord { Kind = SessionRecordKind.Footer, Footer = new SessionFooter
                {
                    ElapsedSeconds = lastWrittenTime, Completed = stopReason == "stopped", StopReason = stopReason, Counts = GetCounters()
                } });
                if (footer.Length + 1 > JsonLines.FooterReserveBytes) throw new IOException("Footer exceeded its reserved space.");
                output.Write(footer, 0, footer.Length); output.WriteByte((byte)'\n');
                output.Flush();
            }
            catch (Exception error)
            {
                Fail(error);
                if (current != null) Drop(current.Kind == SessionRecordKind.Sample);
                while (queue.TryDequeue(out var remaining))
                {
                    Interlocked.Decrement(ref pending); Drop(remaining.Kind == SessionRecordKind.Sample);
                }
            }
            finally
            {
                try { output.Dispose(); } catch (Exception error) { Fail(error); }
                // No producer that can touch wake remains after acceptance closes and drains.
                WaitForProducers();
                wake.Dispose();
                Volatile.Write(ref status, (int)(errorCount != 0 ? RecorderStatus.Faulted : stopReason == "stopped" ? RecorderStatus.Completed : RecorderStatus.LimitReached));
            }
        }

        private void WaitForProducers()
        {
            var spin = new SpinWait();
            while (Volatile.Read(ref activeCalls) != 0) spin.SpinOnce();
        }

        private void Fail(Exception error)
        {
            lock (gate)
            {
                CloseAcceptance("stopped");
                Volatile.Write(ref lastError, error.GetType().Name + ": " + error.Message);
                Interlocked.Increment(ref errorCount);
            }
            WaitForProducers();
        }
    }
}
