using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Xunit;

namespace Dbce.Wheel.Recording.Tests
{
    public sealed class SessionTests : IDisposable
    {
        private readonly string folder = Path.Combine(Path.GetTempPath(), "wheel-recording-test-" + Guid.NewGuid().ToString("N"));
        private string PathFor(string name = "session.jsonl") => Path.Combine(folder, name);
        public SessionTests() => Directory.CreateDirectory(folder);
        public void Dispose() => Directory.Delete(folder, true);

        private static SessionMetadata Metadata() => new SessionMetadata
        {
            Game = "Diagnostic fixture", PluginVersion = "test-1", ToolkitVersion = "0.12.0+unreleased.recording",
            StartedUtc = new DateTime(2026, 9, 12, 18, 0, 0, DateTimeKind.Utc).AddTicks(1234567),
            Properties = new Dictionary<string, string> { ["scenario"] = "Aragón\nFree Play", ["settings"] = "{\"gain\":0.6}" },
            ChannelUnits = new Dictionary<string, string> { ["steer"] = "normalized", ["speed"] = "m/s" }
        };
        private static Dictionary<string, double> Channels(double steer = 0.25) => new Dictionary<string, double> { ["steer"] = steer, ["speed"] = 12.5 };
        private static void Finish(SessionRecorder recorder)
        {
            Assert.True(recorder.Stop(TimeSpan.FromSeconds(5)), "Writer did not finalize.");
            Assert.Null(recorder.Error);
        }

        private List<SessionRecord> Fixture()
        {
            using (var recorder = new SessionRecorder(PathFor(), Metadata()))
            {
                Assert.True(recorder.TryRecord(0, Channels()));
                Assert.True(recorder.TryMark(0, "pause", "game paused"));
                Assert.True(recorder.TryRecord(0.02, Channels(-0.5)));
                Finish(recorder);
            }
            return SessionReader.Read(PathFor()).ToList();
        }

        private void WriteRecords(IEnumerable<SessionRecord> records)
        {
            using var stream = File.Create(PathFor());
            foreach (var record in records)
            {
                if (record.Footer != null) record.Footer.Counts.DataBytesWritten = stream.Position;
                byte[] bytes = JsonLines.Encode(record);
                stream.Write(bytes); stream.WriteByte((byte)'\n');
            }
        }

        [Fact]
        public void RoundtripRetainsMetadataMarkersSamplesAndValidatedFooter()
        {
            var rows = Fixture();
            Assert.Equal(new[] { SessionRecordKind.Metadata, SessionRecordKind.Sample, SessionRecordKind.Marker, SessionRecordKind.Sample, SessionRecordKind.Footer }, rows.Select(r => r.Kind));
            Assert.Equal(SessionRecord.CurrentSchema, rows[0].Schema);
            Assert.Equal(1, rows[0].Version);
            Assert.Equal(Metadata().StartedUtc, rows[0].Metadata.StartedUtc);
            Assert.Equal(DateTimeKind.Utc, rows[0].Metadata.StartedUtc.Kind);
            Assert.Equal(Metadata().Properties, rows[0].Metadata.Properties);
            Assert.Equal(Metadata().ChannelUnits, rows[0].Metadata.ChannelUnits);
            Assert.Equal("test-1", rows[0].Metadata.PluginVersion);
            Assert.Equal("0.12.0+unreleased.recording", rows[0].Metadata.ToolkitVersion);
            Assert.Equal(new long[] { 0, 1 }, rows.Where(r => r.Sample != null).Select(r => r.Sample.Sequence));
            Assert.Equal(-0.5, rows[3].Sample.Channels["steer"]);
            Assert.Equal("pause", rows[2].Marker.Name);
            Assert.Equal("game paused", rows[2].Marker.Detail);
            var footer = rows.Last().Footer;
            Assert.True(footer.Completed);
            Assert.Equal("stopped", footer.StopReason);
            Assert.Equal(2, footer.Counts.WrittenSamples);
            Assert.Equal(1, footer.Counts.WrittenMarkers);
            Assert.Equal(0, footer.Counts.DroppedSamples);
            Assert.Equal(0.02, footer.ElapsedSeconds);
            string json = File.ReadAllText(PathFor());
            Assert.Contains("\"kind\":\"sample\"", json);
            Assert.Contains("\"channels\":{", json);
            Assert.Contains("2026-09-12T18:00:00.1234567Z", json);
        }

        private void WriteHeaderFrame(List<SessionRecord> rows, byte[] header, bool crlf)
        {
            using var stream = File.Create(PathFor());
            byte[] ending = Encoding.UTF8.GetBytes(crlf ? "\r\n" : "\n");
            for (int i = 0; i < rows.Count; i++)
            {
                if (rows[i].Footer != null) rows[i].Footer.Counts.DataBytesWritten = stream.Position;
                byte[] bytes = i == 0 ? header : JsonLines.Encode(rows[i]);
                stream.Write(bytes); stream.Write(ending);
            }
        }

        [Theory]
        [InlineData(16382, false)]
        [InlineData(16383, false)]
        [InlineData(16384, false)]
        [InlineData(16385, false)]
        [InlineData(16382, true)]
        [InlineData(16383, true)] // CR and LF fall in different blocks.
        [InlineData(16384, true)]
        [InlineData(49155, true)]
        [InlineData(1048576, false)]
        [InlineData(1048575, true)] // The size bound includes CR, but excludes LF.
        public void BlockBoundariesPreserveRecordsAndExactFooterBytes(int contentLength, bool crlf)
        {
            var rows = Fixture();
            byte[] json = JsonLines.Encode(rows[0]);
            byte[] padded = Enumerable.Repeat((byte)' ', contentLength).ToArray();
            Array.Copy(json, padded, json.Length);
            WriteHeaderFrame(rows, padded, crlf);
            var decoded = SessionReader.Read(PathFor()).ToList();
            Assert.Equal(rows.Count, decoded.Count);
            Assert.Equal(rows[0].Metadata.Properties, decoded[0].Metadata.Properties);
            Assert.Equal(rows[3].Sample.Channels, decoded[3].Sample.Channels);
            Assert.Equal(rows.Last().Footer.Counts.DataBytesWritten, decoded.Last().Footer.Counts.DataBytesWritten);
        }

        [Theory]
        [InlineData(false)]
        [InlineData(true)]
        public void Utf8SpanningBlocksIsDecodedStrictly(bool corrupt)
        {
            var rows = Fixture();
            byte[] json = JsonLines.Encode(rows[0]);
            int accented = Array.IndexOf(json, (byte)0xc3); // ó in Aragón.
            Assert.True(accented >= 0);
            int padding = 16383 - accented;
            var header = Enumerable.Repeat((byte)' ', padding).Concat(json).ToArray();
            if (corrupt) header[16384] = 0xff;
            WriteHeaderFrame(rows, header, true);
            if (corrupt)
            {
                var error = Assert.Throws<SessionReadException>(() => SessionReader.Read(PathFor()).ToList());
                Assert.Equal(SessionReadError.InvalidJson, error.Error);
                Assert.Equal(1, error.LineNumber);
            }
            else Assert.Equal(rows[0].Metadata.Properties, SessionReader.Read(PathFor()).First().Metadata.Properties);
        }

        [Theory]
        [InlineData(1048576, false, SessionReadError.TruncatedLine)]
        [InlineData(1048577, false, SessionReadError.RecordTooLarge)]
        [InlineData(1048577, true, SessionReadError.RecordTooLarge)]
        public void PartialAndOversizedBlockLinesKeepTheirDistinctErrors(int length, bool terminated, SessionReadError expected)
        {
            File.WriteAllText(PathFor(), new string(' ', length) + (terminated ? "\n" : ""), new UTF8Encoding(false));
            var error = Assert.Throws<SessionReadException>(() => SessionReader.Read(PathFor()).ToList());
            Assert.Equal(expected, error.Error);
            Assert.Equal(1, error.LineNumber);
        }

        [Fact]
        public void TrailingBlankLineAfterFooterIsStillRefusedAcrossBlockBoundary()
        {
            var rows = Fixture();
            byte[] header = JsonLines.Encode(rows[0]);
            WriteHeaderFrame(rows, header.Concat(Enumerable.Repeat((byte)' ', 32768)).ToArray(), true);
            File.AppendAllText(PathFor(), "\n");
            var yielded = new List<SessionRecord>();
            var error = Assert.Throws<SessionReadException>(() =>
            {
                foreach (var row in SessionReader.Read(PathFor())) yielded.Add(row);
            });
            Assert.Equal(SessionReadError.InvalidFooter, error.Error);
            Assert.Equal(6, error.LineNumber);
            Assert.DoesNotContain(yielded, row => row.Footer != null);
        }

        [Fact]
        public void InterleavedEnumerationsAndReturnedDictionariesStayIndependent()
        {
            Fixture();
            var recording = SessionReader.Read(PathFor());
            using var first = recording.GetEnumerator();
            using var second = recording.GetEnumerator();
            Assert.True(first.MoveNext()); Assert.True(second.MoveNext());
            first.Current.Metadata.Properties["scenario"] = "changed by caller";
            Assert.Equal("Aragón\nFree Play", second.Current.Metadata.Properties["scenario"]);
            Assert.True(first.MoveNext()); Assert.True(second.MoveNext());
            var retained = first.Current.Sample;
            retained.Channels["steer"] = 0.9;
            Assert.Equal(0.25, second.Current.Sample.Channels["steer"]);
            Assert.True(first.MoveNext()); Assert.True(first.MoveNext());
            Assert.Equal(-0.5, first.Current.Sample.Channels["steer"]);
            Assert.Equal(0.9, retained.Channels["steer"]);
            while (second.MoveNext()) { }
            while (first.MoveNext()) { }
            Assert.True(SessionReader.Read(PathFor()).Last().Footer.Completed);
        }

        [Theory]
        [InlineData("fr-FR")]
        [InlineData("tr-TR")]
        [InlineData("ar-SA")]
        public void WireNumbersAndDatesAreCultureIndependent(string culture)
        {
            var previous = CultureInfo.CurrentCulture;
            var previousUi = CultureInfo.CurrentUICulture;
            try
            {
                CultureInfo.CurrentCulture = CultureInfo.CurrentUICulture = CultureInfo.GetCultureInfo(culture);
                var rows = Fixture();
                Assert.Equal(12.5, rows[1].Sample.Channels["speed"]);
                Assert.Contains("12.5", File.ReadAllText(PathFor()));
            }
            finally { CultureInfo.CurrentCulture = previous; CultureInfo.CurrentUICulture = previousUi; }
        }

        [Fact]
        public void InputsAndOptionsAreSnapshottedBeforeCallerMutatesThem()
        {
            using var stream = new ControlledStream();
            var metadata = Metadata();
            var channels = Channels();
            var options = new RecordingOptions { QueueCapacity = 1 };
            using var recorder = new SessionRecorder(stream, metadata, options);
            Assert.True(stream.Entered.Wait(5000));
            try
            {
                Assert.True(recorder.TryRecord(0, channels));
                channels["steer"] = double.NaN; channels.Clear();
                metadata.Properties["scenario"] = "changed"; metadata.ChannelUnits.Clear(); metadata.Game = "changed";
                options.QueueCapacity = 100; options.MaxFileBytes = 1;
                Assert.False(recorder.TryRecord(0.02, Channels()));
            }
            finally { stream.Release.Set(); }
            Finish(recorder);
            File.WriteAllBytes(PathFor(), stream.ToArray());
            var rows = SessionReader.Read(PathFor()).ToList();
            Assert.Equal("Diagnostic fixture", rows[0].Metadata.Game);
            Assert.Equal("Aragón\nFree Play", rows[0].Metadata.Properties["scenario"]);
            Assert.Equal(2, rows[0].Metadata.ChannelUnits.Count);
            Assert.Equal(0.25, rows[1].Sample.Channels["steer"]);
            Assert.Equal(1, rows.Last().Footer.Counts.QueueFullCount);
            Assert.Equal(1, rows.Last().Footer.Counts.DroppedSamples);
        }

        [Fact]
        public async Task FullQueueAndSlowDiskNeverBlockProducerOrGuiStopAndEventuallyDrain()
        {
            using var stream = new ControlledStream();
            using var recorder = new SessionRecorder(stream, Metadata(), new RecordingOptions { QueueCapacity = 2 });
            Assert.True(stream.Entered.Wait(5000));
            try
            {
                var producer = Task.Run(() => new[] { recorder.TryRecord(0, Channels()), recorder.TryMark(0.01, "start", ""), recorder.TryRecord(0.02, Channels()) });
                Assert.Equal(new[] { true, true, false }, await producer.WaitAsync(TimeSpan.FromSeconds(2)));
                Assert.Equal(1, recorder.DroppedSamples);
                recorder.Stop();
                Assert.False(recorder.IsRecording);
                Assert.Equal(RecorderStatus.Stopping, recorder.Status);
                Assert.False(recorder.Stop(TimeSpan.FromMilliseconds(5)));
                Assert.False(recorder.TryRecord(0.03, Channels()));
            }
            finally { stream.Release.Set(); }
            Finish(recorder);
            Assert.Equal(RecorderStatus.Completed, recorder.Status);
            Assert.Equal(1, recorder.WrittenSamples);
            Assert.Equal(1, recorder.GetCounters().WrittenMarkers);
            Assert.Equal(1, recorder.GetCounters().RejectedAfterStop);
            recorder.Dispose(); recorder.Stop(); // harmless after worker-owned handles close
            File.WriteAllBytes(PathFor(), stream.ToArray());
            Assert.True(SessionReader.Read(PathFor()).Last().Footer.Completed);
        }

        [Fact]
        public async Task StopReturnsWithinItsBudgetEvenWhenCallerDictionaryBlocks()
        {
            using var dictionary = new PausedDictionary();
            using var recorder = new SessionRecorder(PathFor(), Metadata());
            var producer = Task.Run(() => recorder.TryRecord(0, dictionary));
            Assert.True(dictionary.Entered.Wait(5000));
            try
            {
                var watch = Stopwatch.StartNew();
                Assert.False(recorder.Stop(TimeSpan.FromMilliseconds(10)));
                Assert.True(watch.Elapsed < TimeSpan.FromSeconds(2));
                Assert.False(recorder.TryRecord(1, Channels()));
            }
            finally { dictionary.Release.Set(); }
            await producer.WaitAsync(TimeSpan.FromSeconds(5));
            Finish(recorder);
            Assert.Equal(recorder.GetCounters().AcceptedSamples, recorder.WrittenSamples);
            Assert.True(SessionReader.Read(PathFor()).Last().Footer.Completed);
        }

        [Theory]
        [InlineData(double.NaN)]
        [InlineData(double.PositiveInfinity)]
        [InlineData(double.NegativeInfinity)]
        public void NonfiniteInputsAreRejectedAndCountedWithoutPoisoningCapture(double value)
        {
            using var recorder = new SessionRecorder(PathFor(), Metadata());
            Assert.False(recorder.TryRecord(0, Channels(value)));
            Assert.False(recorder.TryRecord(value, Channels()));
            Assert.False(recorder.TryMark(value, "marker", ""));
            Assert.True(recorder.TryRecord(0, Channels()));
            Finish(recorder);
            var counts = SessionReader.Read(PathFor()).Last().Footer.Counts;
            Assert.Equal(3, counts.InvalidCount);
            Assert.Equal(2, counts.DroppedSamples);
            Assert.Equal(1, counts.DroppedMarkers);
            Assert.Equal(1, counts.WrittenSamples);
        }

        [Fact]
        public void InvalidNamesChannelsAndBackwardsOrDuplicateSampleTimesAreRejected()
        {
            using var recorder = new SessionRecorder(PathFor(), Metadata());
            Assert.False(recorder.TryRecord(-1, Channels()));
            Assert.False(recorder.TryRecord(0, null));
            Assert.False(recorder.TryRecord(0, new Dictionary<string, double>()));
            Assert.False(recorder.TryRecord(0, new Dictionary<string, double> { [" "] = 1 }));
            Assert.False(recorder.TryMark(0, "", ""));
            Assert.False(recorder.TryMark(0, "marker", null));
            Assert.True(recorder.TryRecord(0.02, Channels()));
            Assert.False(recorder.TryRecord(0.02, Channels()));
            Assert.False(recorder.TryMark(0.01, "earlier", ""));
            Assert.True(recorder.TryMark(0.02, "pause", ""));
            Assert.True(recorder.TryMark(0.03, "paused", ""));
            Assert.False(recorder.TryRecord(0.025, Channels()));
            Assert.True(recorder.TryRecord(0.04, Channels()));
            Finish(recorder);
            Assert.Equal(9, SessionReader.Read(PathFor()).Last().Footer.Counts.InvalidCount);
        }

        [Fact]
        public void SampleDurationLimitClosesCaptureWithAnHonestFooter()
        {
            using var recorder = new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { MaxDurationSeconds = 30 });
            Assert.True(recorder.TryRecord(0, Channels()));
            Assert.False(recorder.TryRecord(31, Channels()));
            Finish(recorder);
            Assert.Equal(RecorderStatus.LimitReached, recorder.Status);
            Assert.Equal("durationLimit", recorder.StopReason);
            var footer = SessionReader.Read(PathFor()).Last().Footer;
            Assert.False(footer.Completed);
            Assert.Equal(1, footer.Counts.LimitCount);
            Assert.Equal(1, footer.Counts.DroppedSamples);
        }

        [Fact]
        public void IdleRecorderAlsoStopsAtWallClockLimit()
        {
            using var recorder = new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { MaxDurationSeconds = 0.05 });
            Assert.True(SpinWait.SpinUntil(() => recorder.Status == RecorderStatus.LimitReached, 5000));
            Finish(recorder);
            var footer = SessionReader.Read(PathFor()).Last().Footer;
            Assert.Equal("durationLimit", footer.StopReason);
            Assert.Equal(0, footer.Counts.DroppedSamples);
        }

        [Theory]
        [InlineData(false)]
        [InlineData(true)]
        public void ByteAndRecordLimitsIncludeFooterAndAccountForQueuedDiscards(bool recordLimit)
        {
            using var stream = new ControlledStream();
            var options = new RecordingOptions { MaxFileBytes = 16384, MaxRecordBytes = recordLimit ? 1024 : 8192, QueueCapacity = 20 };
            using var recorder = new SessionRecorder(stream, Metadata(), options);
            Assert.True(stream.Entered.Wait(5000));
            try
            {
                Assert.True(recorder.TryRecord(0, Channels()));
                for (int i = 0; i < 8; i++) Assert.True(recorder.TryMark(i, "large", new string('x', 4096)));
            }
            finally { stream.Release.Set(); }
            Finish(recorder);
            Assert.Equal(RecorderStatus.LimitReached, recorder.Status);
            Assert.True(stream.ToArray().LongLength <= options.MaxFileBytes);
            File.WriteAllBytes(PathFor(), stream.ToArray());
            var footer = SessionReader.Read(PathFor()).Last().Footer;
            Assert.Equal(recordLimit ? "recordSizeLimit" : "fileSizeLimit", footer.StopReason);
            Assert.False(footer.Completed);
            Assert.Equal(footer.Counts.AcceptedMarkers, footer.Counts.WrittenMarkers + footer.Counts.DroppedMarkers);
            Assert.Equal(1, footer.Counts.LimitCount);
            Assert.Equal(1, footer.Counts.WrittenSamples);
        }

        [Fact]
        public void WriterFailureIsObservableAndDropsAllUnwrittenAcceptedRecords()
        {
            using var stream = new ControlledStream { FailWriteNumber = 2 };
            using var recorder = new SessionRecorder(stream, Metadata());
            Assert.True(stream.Entered.Wait(5000));
            try
            {
                Assert.True(recorder.TryRecord(0, Channels()));
                Assert.True(recorder.TryRecord(0.02, Channels()));
                Assert.True(recorder.TryMark(0.02, "marker", ""));
            }
            finally { stream.Release.Set(); }
            Assert.True(recorder.Stop(TimeSpan.FromSeconds(5)));
            Assert.Equal(RecorderStatus.Faulted, recorder.Status);
            Assert.Contains("injected disk failure", recorder.Error);
            Assert.Equal("error", recorder.StopReason);
            Assert.Equal(2, recorder.DroppedSamples);
            Assert.Equal(1, recorder.GetCounters().DroppedMarkers);
            Assert.Equal(1, recorder.GetCounters().ErrorCount);
            Assert.Equal(0, recorder.WrittenSamples);
            File.WriteAllBytes(PathFor(), stream.ToArray());
            Assert.Equal(SessionReadError.MissingFooter, Assert.Throws<SessionReadException>(() => SessionReader.Read(PathFor()).ToList()).Error);
        }

        [Fact]
        public void ExistingFileIsNeverOverwrittenAndInvalidMetadataDoesNotCreateAFile()
        {
            File.WriteAllText(PathFor(), "original");
            Assert.Throws<IOException>(() => new SessionRecorder(PathFor(), Metadata()));
            Assert.Equal("original", File.ReadAllText(PathFor()));
            var metadata = Metadata(); metadata.StartedUtc = DateTime.SpecifyKind(metadata.StartedUtc, DateTimeKind.Unspecified);
            Assert.Throws<ArgumentException>(() => new SessionRecorder(PathFor("bad.jsonl"), metadata));
            Assert.False(File.Exists(PathFor("bad.jsonl")));
        }

        [Fact]
        public void ConsumerCanCapture1024ChannelsAndRejectLargerFrames()
        {
            using var recorder = new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { QueueCapacity = 512, MaxChannelsPerSample = 1024 });
            var channels = Enumerable.Range(0, 1024).ToDictionary(i => "channel." + i, i => i / 1024.0);
            Assert.True(recorder.TryRecord(0, channels));
            channels.Add("too.many", 0);
            Assert.False(recorder.TryRecord(0.02, channels));
            Finish(recorder);
            var rows = SessionReader.Read(PathFor()).ToList();
            Assert.Equal(1024, rows[1].Sample.Channels.Count);
            Assert.Equal(1, rows.Last().Footer.Counts.InvalidCount);
        }

        [Theory]
        [InlineData("NaN")]
        [InlineData("Infinity")]
        [InlineData("-Infinity")]
        public void ReaderRejectsQuotedNonfiniteNumbersAcceptedByDataContractSerializer(string number)
        {
            Fixture();
            string json = File.ReadAllText(PathFor());
            string modified = json.Replace("\"steer\":0.25", "\"steer\":\"" + number + "\"");
            Assert.NotEqual(json, modified);
            File.WriteAllText(PathFor(), modified);
            Assert.Equal(SessionReadError.InvalidRecord, Assert.Throws<SessionReadException>(() => SessionReader.Read(PathFor()).ToList()).Error);
        }

        [Fact]
        public void ReaderCountsCrLfBytesAndSupportsEmptyCompletedSession()
        {
            using (var recorder = new SessionRecorder(PathFor(), Metadata())) Finish(recorder);
            var rows = SessionReader.Read(PathFor()).ToList();
            Assert.Equal(2, rows.Count);
            var header = Encoding.UTF8.GetString(JsonLines.Encode(rows[0])) + "\r\n";
            rows[1].Footer.Counts.DataBytesWritten = Encoding.UTF8.GetByteCount(header);
            File.WriteAllText(PathFor(), header + Encoding.UTF8.GetString(JsonLines.Encode(rows[1])) + "\r\n");
            Assert.Equal(0, SessionReader.Read(PathFor()).Last().Footer.Counts.WrittenSamples);
        }

        [Fact]
        public void OptionsAndMetadataBoundsFailBeforeStartingWriter()
        {
            Assert.Throws<ArgumentOutOfRangeException>(() => new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { QueueCapacity = 0 }));
            Assert.Throws<ArgumentOutOfRangeException>(() => new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { MaxFileBytes = 100 }));
            Assert.Throws<ArgumentOutOfRangeException>(() => new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { MaxDurationSeconds = double.NaN }));
            var metadata = Metadata(); metadata.Properties["large"] = new string('x', 4096);
            Assert.Throws<ArgumentException>(() => new SessionRecorder(PathFor(), metadata, new RecordingOptions { MaxRecordBytes = 1024 }));
            Assert.False(File.Exists(PathFor()));
        }

        [Fact]
        public async Task ConcurrentStopAndProducerKeepFinalCountsConsistent()
        {
            using var recorder = new SessionRecorder(PathFor(), Metadata(), new RecordingOptions { QueueCapacity = 4096 });
            using var go = new ManualResetEventSlim();
            var producers = Enumerable.Range(0, 4).Select(_ => Task.Run(() =>
            {
                go.Wait();
                for (int i = 0; i < 500; i++) recorder.TryMark(0, "concurrent", "");
            })).ToArray();
            go.Set();
            await Task.WhenAll(producers).WaitAsync(TimeSpan.FromSeconds(5));
            var stops = Enumerable.Range(0, 4).Select(_ => Task.Run(() => recorder.Stop(TimeSpan.FromSeconds(5)))).ToArray();
            Assert.All(await Task.WhenAll(stops).WaitAsync(TimeSpan.FromSeconds(6)), Assert.True);
            var footer = SessionReader.Read(PathFor()).Last().Footer;
            Assert.Equal(2000, footer.Counts.AcceptedMarkers + footer.Counts.DroppedMarkers);
            Assert.Equal(footer.Counts.AcceptedMarkers, footer.Counts.WrittenMarkers);
        }

        [Theory]
        [InlineData("schema", SessionReadError.InvalidSchema)]
        [InlineData("version", SessionReadError.UnsupportedVersion)]
        [InlineData("duplicateHeader", SessionReadError.InvalidOrder)]
        [InlineData("sequence", SessionReadError.InvalidOrder)]
        [InlineData("time", SessionReadError.InvalidOrder)]
        [InlineData("nonfiniteTime", SessionReadError.InvalidJson)]
        [InlineData("nonfiniteChannel", SessionReadError.InvalidJson)]
        [InlineData("payload", SessionReadError.InvalidRecord)]
        [InlineData("footerCount", SessionReadError.InvalidFooter)]
        [InlineData("footerDropped", SessionReadError.InvalidFooter)]
        [InlineData("footerCompleted", SessionReadError.InvalidFooter)]
        [InlineData("footerTime", SessionReadError.InvalidFooter)]
        [InlineData("footerError", SessionReadError.InvalidFooter)]
        [InlineData("trailingRecord", SessionReadError.InvalidFooter)]
        public void ReaderRejectsCorruptContracts(string corruption, SessionReadError expected)
        {
            var rows = Fixture();
            switch (corruption)
            {
                case "schema": rows[0].Schema = "other"; break;
                case "version": rows[0].Version = 2; break;
                case "duplicateHeader": rows.Insert(1, rows[0]); rows[0].Schema = null; rows[0].Version = 0; // restore independent header
                    rows[0] = new SessionRecord { Kind = SessionRecordKind.Metadata, Schema = SessionRecord.CurrentSchema, Version = 1, Metadata = Metadata() }; break;
                case "sequence": rows[1].Sample.Sequence = 1; break;
                case "time": rows[3].Sample.ElapsedSeconds = -1; break;
                case "nonfiniteTime": rows[3].Sample.ElapsedSeconds = double.NaN; break;
                case "nonfiniteChannel": rows[1].Sample.Channels["steer"] = double.PositiveInfinity; break;
                case "payload": rows[1].Marker = new SessionMarker { ElapsedSeconds = 0, Name = "bad", Detail = "" }; break;
                case "footerCount": rows.Last().Footer.Counts.WrittenSamples++; break;
                case "footerDropped": rows.Last().Footer.Counts.DroppedSamples = long.MaxValue; break;
                case "footerCompleted": rows.Last().Footer.Completed = false; break;
                case "footerTime": rows.Last().Footer.ElapsedSeconds = 100; break;
                case "footerError": rows.Last().Footer.Counts.ErrorCount = 1; break;
                case "trailingRecord": rows.Add(rows[1]); break;
            }
            WriteRecords(rows);
            var error = Assert.Throws<SessionReadException>(() => SessionReader.Read(PathFor()).ToList());
            Assert.Equal(expected, error.Error);
            Assert.True(error.LineNumber > 0);
        }

        [Theory]
        [InlineData("empty", SessionReadError.MissingFooter)]
        [InlineData("noFooter", SessionReadError.MissingFooter)]
        [InlineData("partialFooter", SessionReadError.TruncatedLine)]
        [InlineData("noFinalNewline", SessionReadError.TruncatedLine)]
        [InlineData("badJson", SessionReadError.InvalidJson)]
        [InlineData("trailingJson", SessionReadError.InvalidJson)]
        [InlineData("missingField", SessionReadError.InvalidJson)]
        [InlineData("unknownKind", SessionReadError.InvalidJson)]
        [InlineData("badUtf8", SessionReadError.InvalidJson)]
        [InlineData("oversize", SessionReadError.RecordTooLarge)]
        public void ReaderReportsInterruptedOrMalformedJsonl(string corruption, SessionReadError expected)
        {
            Fixture();
            var lines = File.ReadAllLines(PathFor());
            string json = File.ReadAllText(PathFor());
            switch (corruption)
            {
                case "empty": File.WriteAllText(PathFor(), ""); break;
                case "noFooter": File.WriteAllText(PathFor(), string.Join("\n", lines.Take(lines.Length - 1)) + "\n"); break;
                case "partialFooter": File.WriteAllText(PathFor(), json.Substring(0, json.Length - 20)); break;
                case "noFinalNewline": File.WriteAllText(PathFor(), json.TrimEnd('\n')); break;
                case "badJson": File.WriteAllText(PathFor(), "{garbage}\n"); break;
                case "trailingJson": File.WriteAllText(PathFor(), lines[0] + "{}\n"); break;
                case "missingField":
                    string missing = json.Replace(",\"sequence\":0", "");
                    Assert.NotEqual(json, missing); File.WriteAllText(PathFor(), missing); break;
                case "unknownKind": File.WriteAllText(PathFor(), json.Replace("\"kind\":\"sample\"", "\"kind\":\"unknown\"")); break;
                case "badUtf8": File.WriteAllBytes(PathFor(), new byte[] { 0xff, 0x0a }); break;
                case "oversize": File.WriteAllText(PathFor(), new string('x', JsonLines.MaxLineBytes + 1) + "\n"); break;
            }
            Assert.Equal(expected, Assert.Throws<SessionReadException>(() => SessionReader.Read(PathFor()).ToList()).Error);
        }

        [Fact]
        public void ReaderCanStreamPrefixButDoesNotYieldFooterIfTrailingDataExists()
        {
            Fixture();
            File.AppendAllText(PathFor(), "{}\n");
            Assert.Equal(SessionRecordKind.Metadata, SessionReader.Read(PathFor()).First().Kind);
            var yielded = new List<SessionRecord>();
            Assert.Throws<SessionReadException>(() => { foreach (var row in SessionReader.Read(PathFor())) yielded.Add(row); });
            Assert.DoesNotContain(yielded, r => r.Kind == SessionRecordKind.Footer);
        }

        private sealed class ControlledStream : MemoryStream
        {
            internal readonly ManualResetEventSlim Entered = new ManualResetEventSlim();
            internal readonly ManualResetEventSlim Release = new ManualResetEventSlim();
            internal int FailWriteNumber;
            private int writes;
            public override void Write(byte[] buffer, int offset, int count)
            {
                int call = Interlocked.Increment(ref writes);
                if (call == 1)
                {
                    Entered.Set();
                    if (!Release.Wait(10000)) throw new IOException("Test did not release blocked writer.");
                }
                if (call == FailWriteNumber) throw new IOException("injected disk failure");
                base.Write(buffer, offset, count);
            }
        }

        private sealed class PausedDictionary : Dictionary<string, double>, IDictionary<string, double>, IDisposable
        {
            internal readonly ManualResetEventSlim Entered = new ManualResetEventSlim();
            internal readonly ManualResetEventSlim Release = new ManualResetEventSlim();
            internal PausedDictionary() { Add("steer", 0); }
            IEnumerator<KeyValuePair<string, double>> IEnumerable<KeyValuePair<string, double>>.GetEnumerator()
            {
                Entered.Set();
                if (!Release.Wait(10000)) throw new InvalidOperationException("Test did not release dictionary.");
                return base.GetEnumerator();
            }
            public void Dispose() { Release.Set(); Entered.Dispose(); Release.Dispose(); }
        }
    }
}
