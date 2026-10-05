using System;
using System.Collections.Concurrent;
using System.IO;
using System.Text;
using System.Threading;

namespace Dbce.Wheel.Playback
{
    /// <summary>Bounded producer queue. Overflow or disk errors fail the capture instead of losing rows.</summary>
    public sealed class BufferedTapeWriter : TextWriter
    {
        private readonly BlockingCollection<string> queue;
        private readonly Thread worker;
        private readonly StreamWriter sink;
        private Exception failure;
        private bool disposed;
        public override Encoding Encoding => Encoding.UTF8;
        public BufferedTapeWriter(string path, int capacity = 4096)
        {
            queue = new BlockingCollection<string>(capacity);
            sink = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
            worker = new Thread(WriteLoop) { IsBackground = true, Name = "Playback tape writer" };
            worker.Start();
        }
        private void WriteLoop()
        {
            try
            {
                foreach (var line in queue.GetConsumingEnumerable())
                    if (line == null) sink.Flush(); else sink.WriteLine(line);
            }
            catch (Exception ex) { Interlocked.CompareExchange(ref failure, ex, null); }
            finally { try { sink.Dispose(); } catch (Exception ex) { Interlocked.CompareExchange(ref failure, ex, null); } }
        }
        private void Enqueue(string line)
        {
            if (disposed) throw new ObjectDisposedException(nameof(BufferedTapeWriter));
            if (failure != null) throw new IOException("Tape writer failed", failure);
            if (line != null && line.Length > 262144) throw new IOException("Tape row exceeds size limit");
            if (!queue.TryAdd(line)) throw new IOException("Tape writer queue overflow");
        }
        public override void WriteLine(string value) => Enqueue(value ?? "");
        public override void Flush() => Enqueue(null);
        protected override void Dispose(bool disposing)
        {
            if (disposed) return;
            disposed = true; queue.CompleteAdding();
            if (!worker.Join(10000)) throw new IOException("Tape writer did not finish within 10 seconds");
            queue.Dispose();
            if (failure != null) throw new IOException("Tape writer failed", failure);
            base.Dispose(disposing);
        }
    }
}
