// Fake-sink checks for shared/ffb/SharedFfbOutput.cs. C# 7.3, no native load, device, Unity or game.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Dbce.Wheel.Ffb;
using Dbce.Wheel.Ffb.Output;

internal static class Program
{
    private sealed class Fake
    {
        public int Opens, Stops;
        public bool Ready, RefuseOpen, RefuseWrite, ThrowWrite, ThrowStop;
        public Guid Target;
        public int Window;
        public readonly List<int> Commands = new List<int>();
        public FfbSink Sink => new FfbSink((guid, window) => { Opens++; Target = guid; Window = window; return Ready = !RefuseOpen; }, command =>
        {
            Commands.Add(command);
            if (ThrowWrite) throw new IOException("fake write failure");
            return !RefuseWrite;
        }, () => { Stops++; Ready = false; if (ThrowStop) throw new IOException("fake cleanup failure"); }, () => Ready);
    }

    private static int _checks;
    private static void Check(bool ok, string why) { if (!ok) throw new Exception(why); _checks++; }
    private static FfbSource Source(double tick, float request = -.25f, string epoch = "carA") => new FfbSource(true, request, epoch, tick);

    private static int Main()
    {
        try
        {
            Gates(); Delivery(); Ticks(); Failures(); Pair();
            Check(WheelFfbNative.LoadedFrom == null && !WheelFfbNative.Ready, "Fixture loaded or initialized the shared native binding");
            Console.WriteLine("SHARED_FFB_OUTPUT_PASS " + _checks + " checks; fake sink only, no module load/device/game.");
            return 0;
        }
        catch (Exception ex) { Console.WriteLine("SHARED_FFB_OUTPUT_FAIL after " + _checks + " checks: " + ex.Message); return 1; }
    }

    private static readonly Guid WheelA = Guid.Parse("11111111-1111-1111-1111-111111111111");
    private static readonly Guid WheelB = Guid.Parse("22222222-2222-2222-2222-222222222222");

    private static void Gates()
    {
        foreach (var flags in new[] { (false, false), (false, true), (true, true) })
        {
            var fake = new Fake();
            using (var output = new FfbOutput(fake.Sink, flags.Item1, flags.Item2))
            {
                var d = output.Update(Source(1), true, WheelA, 42, 1);
                Check(fake.Opens == 0 && fake.Commands.Count == 0 && !d.Attempted && d.ModelRequest == -.25f, "Off/muted output touched sink or lost request");
                Check(d.Status == (flags.Item2 ? "Process force mute" : "Off"), "Off/mute status");
            }
        }
        var bad = new[] { new FfbSource(false, -.25f, "carA", 1), new FfbSource(true, float.NaN, "carA", 1),
            new FfbSource(true, 1.1f, "carA", 1), new FfbSource(true, -.25f, "carA", double.NaN),
            new FfbSource(true, -.25f, "carA", double.PositiveInfinity), new FfbSource(true, -.25f, "", 1), new FfbSource(true, -.25f, null, 1) };
        foreach (var source in bad)
        {
            var fake = new Fake();
            using (var output = new FfbOutput(fake.Sink, true, false))
                Check(!output.Update(source, true, WheelA, 42, 1).Attempted && fake.Opens == 0, "Unavailable/corrupt source opened output");
        }
        foreach (var context in new[] { (false, (Guid?)WheelA, 42), (true, (Guid?)null, 42), (true, (Guid?)Guid.Empty, 42), (true, (Guid?)WheelA, 0) })
        {
            var fake = new Fake();
            using (var output = new FfbOutput(fake.Sink, true, false))
            {
                output.Update(Source(1), context.Item1, context.Item2, context.Item3, 1);
                Check(fake.Opens == 0, "Gate/identity/window absence opened a fallback wheel");
            }
        }
        bool threw = false;
        try { new FfbOutput(null, true, false).Dispose(); } catch (ArgumentNullException) { threw = true; }
        Check(threw, "Null sink admitted");
    }

    private static void Delivery()
    {
        var fake = new Fake(); var output = new FfbOutput(fake.Sink, true, false);
        var receipt = output.Update(Source(1), true, WheelB, 42, 1);
        Check(fake.Target == WheelB && fake.Window == 42 && fake.Commands.SequenceEqual(new[] { -2500 }) && receipt.ModelRequest == -.25f &&
            receipt.Accepted && receipt.Attempted && receipt.Command == -2500 && receipt.Status == "Native command accepted; physical output unverified",
            "Wrong exact wheel, window, native scaling or delivery receipt");
        var repeat = output.Update(Source(1), true, WheelB, 42, 1.01);
        Check(fake.Commands.Count == 1 && !repeat.Attempted && repeat.Status == "No new completed physics tick", "Repeated tick re-sent or wrong status");
        output.Update(Source(1), true, WheelB, 42, 1.11);
        Check(fake.Stops == 1 && output.Last.Status.StartsWith("Completed physics source stale") && output.Last.Status.EndsWith("zero/stop/shutdown requested"),
            "Stale source did not request stop");
        output.Update(Source(1), true, WheelB, 42, 1.12);
        Check(fake.Opens == 1, "Stale source reopened the device");
        output.Update(Source(1.02), true, WheelB, 42, 1.13);
        Check(fake.Opens == 2 && fake.Commands.Count == 2, "Fresh source failed to recover after stop");
        output.Suspend("F6 opening");
        Check(fake.Stops == 2 && output.Last.Status == "F6 opening; zero/stop/shutdown requested", "Host suspend did not stop output");
        output.Update(Source(1.04), true, WheelA, 42, 1.14);
        Check(fake.Target == WheelA && fake.Opens == 3, "Exact target change did not reopen deliberately");
        output.Update(Source(1.06), true, WheelA, 43, 1.15);
        Check(fake.Window == 43 && fake.Opens == 4 && fake.Stops == 3, "Window change did not stop and reopen");
        output.Update(Source(1.08, epoch: "carB"), true, WheelA, 43, 1.16);
        Check(fake.Opens == 5 && fake.Stops == 4 && fake.Commands.Count == 5, "Epoch change did not stop and reopen");
        output.Update(Source(1.10), false, WheelA, 43, 1.17);
        Check(fake.Stops == 5, "Eligibility loss retained device output");
        output.Update(Source(1.12, 1), true, WheelA, 43, 1.18);
        Check(fake.Commands.Last() == 10000, "Full request did not map to ForceMax");
        output.LatchMute("shared input stopped");
        int opens = fake.Opens;
        output.Update(Source(1.14), true, WheelA, 43, 1.19);
        Check(fake.Opens == opens && !output.CanAttempt && output.Last.Status == "Process force mute", "Latched mute resumed force");
        output.Dispose(); output.Dispose();
        output.Update(Source(1.16), true, WheelA, 43, 1.2);
        Check(fake.Opens == opens && output.Last.Status == "Stopped", "Disposed output reopened");
    }

    private static void Ticks()
    {
        var fake = new Fake();
        using (var output = new FfbOutput(fake.Sink, true, false))
        {
            output.Update(Source(10), true, WheelA, 42, 1);
            var back = output.Update(Source(5), true, WheelA, 42, 1.01);
            Check(!back.Attempted && fake.Stops == 1 && back.Status.StartsWith("Physics tick moved backward"), "Backward tick did not suspend");
            var rebase = output.Update(Source(5), true, WheelA, 42, 1.02);
            Check(rebase.Accepted && fake.Opens == 2 && fake.Commands.Count == 2, "Next fresh tick after a backward tick did not rebase");
            Check(!output.Update(Source(5), true, WheelA, 42, 1.03).Attempted && fake.Commands.Count == 2, "Rebased tick was sent twice");
            Check(output.Update(Source(6), true, WheelA, 42, 1.04).Accepted && fake.Commands.Count == 3, "Ticks after rebase stopped");
            // Integer tick identities (Over the Hill TickId) convert exactly.
            Check(output.Update(Source(int.MaxValue), true, WheelA, 42, 1.05).Accepted &&
                !output.Update(Source(int.MaxValue), true, WheelA, 42, 1.06).Attempted, "Large integer tick identity");
        }
        {
            var clock = new Fake();
            var output = new FfbOutput(clock.Sink, true, false);
            output.Update(Source(1), true, WheelA, 42, 1);
            output.Update(Source(2), true, WheelA, 42, .5);
            Check(!output.CanAttempt && clock.Stops == 1 && output.Last.Status.StartsWith("Output clock invalid"), "Backward clock did not latch output off");
            var nan = new Fake(); var other = new FfbOutput(nan.Sink, true, false);
            other.Update(Source(1), true, WheelA, 42, double.NaN);
            Check(!other.CanAttempt && nan.Opens == 0, "Nonfinite clock did not latch output off");
        }
    }

    private static void Failures()
    {
        foreach (int failure in new[] { 0, 1, 2, 3 })
        {
            var fake = new Fake { RefuseOpen = failure == 0, RefuseWrite = failure == 1, ThrowWrite = failure == 2, ThrowStop = failure == 3 };
            using (var output = new FfbOutput(fake.Sink, true, false))
            {
                var receipt = output.Update(Source(1), true, WheelA, 42, 1);
                if (failure == 3) output.Suspend("cleanup fixture");
                Check(failure == 3 ? output.Last.Status.Contains("cleanup call failed") && !output.CanAttempt : !receipt.Accepted && fake.Stops == 1,
                    "Open/write/cleanup failure lost its truthful status");
                if (failure == 0) Check(!receipt.Attempted && output.Last.Status.StartsWith("Shared output open refused"), "Refused open status");
                if (failure == 1) Check(receipt.Attempted && receipt.Command == -2500 && receipt.Status == "Native command refused", "Refused write receipt");
                if (failure == 2) Check(receipt.Attempted && receipt.Command == -2500 && receipt.Status.Contains("unknown"), "Throwing native write was reported as unattempted");
                if (failure != 3)
                {
                    var wait = output.Update(Source(1.02), true, WheelA, 42, 1.1);
                    Check(fake.Opens == 1 && wait.Status == "Waiting after output failure", "Output failure retried immediately");
                    output.Update(Source(1.04), true, WheelA, 42, 6.01);
                    Check(fake.Opens == 2, "Output did not retry after the 5 s wait");
                }
            }
        }
        var lost = new Fake();
        using (var output = new FfbOutput(lost.Sink, true, false))
        {
            output.Update(Source(1), true, WheelA, 42, 1);
            lost.Ready = false;
            output.Update(Source(1.02), true, WheelA, 42, 1.02);
            Check(lost.Stops == 1 && !output.Last.Attempted && output.Last.Status.StartsWith("Output device lost"), "Lost native device retained output");
        }
    }

    private static void Pair()
    {
        string pin = new string('A', 64);
        foreach (var args in new[] { ("", pin, pin, 901), ("x", "", pin, 901), ("x", pin, new string('A', 63), 901),
            ("x", pin, new string('G', 64), 901), ("x", pin, pin, 0) })
        {
            bool refused = false;
            try { SharedFfbPair.CreateSink(args.Item1, args.Item2, args.Item3, args.Item4); }
            catch (ArgumentException) { refused = true; }
            Check(refused, "Invalid pair arguments admitted");
        }
        string directory = Path.Combine(Path.GetTempPath(), "dbce-shared-ffb-output-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        try
        {
            string fakeNative = Path.Combine(directory, "WheelFfb.dll");
            File.WriteAllBytes(fakeNative, new byte[] { 1, 2, 3 });
            Check(SharedFfbPair.Sha256(fakeNative) == "039058C6F2C0CB492C533B0A4D14EF77CC0F78ABCCCED5287D84A1A2011CFB81", "SHA-256 is not uppercase hex of the bytes");
            var sink = SharedFfbPair.CreateSink(directory, pin, pin, 901);
            Check(!sink.Ready() && WheelFfbNative.LoadedFrom == null, "Creating the sink loaded or reported a device");
            sink.Stop();
            Check(WheelFfbNative.LoadedFrom == null, "Stop before an owned attempt touched the native binding");
            Check(!sink.Open(WheelA, 42) && WheelFfbNative.LoadedFrom == null && !sink.Ready(), "Mismatched native pin loaded the module");
            string large = Path.Combine(directory, "large.bin");
            using (var stream = File.Create(large)) stream.SetLength(SharedFfbPair.MaxFileBytes + 1);
            bool limited = false;
            try { SharedFfbPair.Sha256(large); } catch (InvalidDataException) { limited = true; }
            Check(limited, "Oversized dependency was hashed");
        }
        finally { Directory.Delete(directory, true); }
        Check(UnityForegroundWindow.Focused() == 0, "A console process reported a Unity output window");
    }
}
