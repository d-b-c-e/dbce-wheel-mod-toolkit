using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests
{
    public class FrameRateMonitorTests
    {
        [Fact]
        public void WindowReportsAverageOnePercentLowAndWorstFrame()
        {
            // 990 frames at 60 fps and 10 hitches of 50 ms: 1000 frames in 17 s.
            var frames = new float[1000];
            for (int i = 0; i < frames.Length; i++) frames[i] = i % 100 == 0 ? .05f : 1f / 60f;
            var r = FrameRateWindow.Summarize(frames, frames.Length);
            Assert.Equal(1000, r.Frames);
            Assert.InRange(r.AverageFps, 58.7, 58.9);
            Assert.InRange(r.LowFps, 19.9, 20.1);
            Assert.InRange(r.WorstMs, 49.9, 50.1);
            Assert.Equal(0, FrameRateWindow.Summarize(frames, 0).Frames);
            Assert.Equal(0, FrameRateWindow.Summarize(null, 5).Frames);
            Assert.InRange(FrameRateWindow.Summarize(new[] { .02f }, 1).LowFps, 49.9, 50.1);
        }

        [Fact]
        public void MonitorCompletesTenSecondWindowsAndSmoothsTheCounter()
        {
            var m = new FrameRateMonitor();
            Assert.Equal("measuring", m.Summary);
            Assert.Equal("", m.OverlayText);
            double t = 0; bool completed = false;
            while (t < 10.5) { m.Tick(t, 1f / 50f); completed |= m.WindowCompleted; t += .02; }
            Assert.True(completed);
            Assert.InRange(m.Last.AverageFps, 49.9, 50.1);
            Assert.StartsWith("50 fps average, 1% low 50 fps, worst frame 20 ms", m.Summary);
            Assert.StartsWith("50 fps", m.OverlayText);
            m.Tick(double.NaN, .02f);                                   // ignored
            m.Tick(t, float.NaN);                                       // ignored frame time
            Assert.False(m.WindowCompleted);
        }
    }
}
