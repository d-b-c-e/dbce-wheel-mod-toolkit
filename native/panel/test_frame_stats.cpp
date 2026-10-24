// Fixtures for dbce_frame_stats.hpp (STD-023). Build: build_test.ps1.
#include "dbce_frame_stats.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_checks, g_failed;
static void check(bool ok, const char *what) { ++g_checks; if (!ok) { ++g_failed; printf("FAIL %s\n", what); } }
static bool near(double a, double b, double tol) { return fabs(a - b) <= tol; }

int main()
{
    using dbce::panel::FrameWindow;
    dbce::panel::SmoothedFps smooth;
    check(smooth.Value() == 0, "counter waits for a measured frame");
    smooth.Add(20); smooth.Add(10);
    check(near(smooth.Value(), 1000.0 / 19.0, 1e-9), "counter smooths current frame time");
    smooth.Add(6000);
    check(smooth.Value() == 0, "pause gap clears stale current fps");
    smooth.Add(10);
    check(near(smooth.Value(), 100, 1e-9), "counter restarts after pause");
    static FrameWindow w;   // large buffer: not on the stack
    check(!w.Last().valid, "nothing before the first window");

    // A steady 50 fps for 10 s closes one window: 500 frames, 50 fps, low 50, worst 20 ms.
    int closed = 0;
    for (int i = 0; i < 500; ++i) closed += w.Add(20.0);
    check(closed == 1 && w.Last().valid && w.Last().frames == 500, "one window after 10 s at 50 fps");
    check(near(w.Last().avgFps, 50.0, 1e-9) && near(w.Last().lowFps, 50.0, 1e-9) && near(w.Last().worstMs, 20.0, 1e-9), "steady 50 fps");

    // 594 frames at 10 ms, 6 at 100 ms, then 10 ms frames to 10 s: 946 frames, 94.6 fps, worst 100 ms.
    closed = 0;
    for (int i = 0; i < 594 && !closed; ++i) closed += w.Add(10.0);
    for (int i = 0; i < 6; ++i) closed += w.Add(100.0);
    check(closed == 0, "window still open at 6.54 s");
    while (!closed) closed += w.Add(10.0);   // to 10 s
    const auto &s = w.Last();
    const int n = s.frames;
    check(n == 946, "946 frames in the second window");
    check(near(s.avgFps, 94.6, 1e-9), "average over the whole window");
    // slowest 1% of 946 = 9 frames: the six 100 ms and three 10 ms -> 630 / 9 = 70 ms -> 14.29 fps
    check(near(s.lowFps, 1000.0 * 9 / 630.0, 0.01) && near(s.worstMs, 100.0, 1e-9), "1% low and worst frame");
    check(!w.Add(6000.0) && !w.Last().valid, "pause expires the last completed window");

    // A gap (pause, minimise, load) restarts the window instead of counting as a frame.
    FrameWindow *g = new FrameWindow;
    for (int i = 0; i < 500; ++i) g->Add(10.0);
    check(!g->Add(6000.0) && !g->Last().valid, "a 6 s gap is not a frame");
    for (int i = 0; i < 999; ++i) g->Add(10.0);
    check(!g->Last().valid, "the window restarted after the gap");
    check(g->Add(10.0) && g->Last().frames == 1000 && near(g->Last().worstMs, 10.0, 1e-9), "and closes 10 s after it");
    check(!g->Add(0.0) && !g->Add(-1.0), "zero and negative times ignored");
    delete g;

    // A very fast game fills the buffer before 10 s: the window closes early.
    FrameWindow *f = new FrameWindow;
    int c = 0;
    for (int i = 0; i < FrameWindow::kMax; ++i) c += f->Add(0.5);
    check(c == 1 && f->Last().frames == FrameWindow::kMax && near(f->Last().avgFps, 2000.0, 0.01), "full buffer closes the window");
    delete f;

    // The line: the triples suffix as before, the mod's display text instead when it gives one; never overrun.
    {
        using dbce::panel::FormatFrameLine;
        dbce::panel::FrameSummary sum;sum.valid = true;sum.avgFps = 59.6;sum.lowFps = 41.2;sum.worstMs = 33.34;
        char line[160];
        FormatFrameLine(line, sizeof line, sum, nullptr, -1);
        check(!strcmp(line, "60 fps average, 1% low 41 fps, worst frame 33.3 ms"), "no display and no triples: the numbers only");
        FormatFrameLine(line, sizeof line, sum, nullptr, 1);
        check(!strcmp(line, "60 fps average, 1% low 41 fps, worst frame 33.3 ms; triples on"), "triples on, as before");
        FormatFrameLine(line, sizeof line, sum, nullptr, 0);
        check(!strcmp(line, "60 fps average, 1% low 41 fps, worst frame 33.3 ms; triples off"), "triples off, as before");
        FormatFrameLine(line, sizeof line, sum, "Surround span, one wide view", 1);
        check(!strcmp(line, "60 fps average, 1% low 41 fps, worst frame 33.3 ms; Surround span, one wide view"), "the display text replaces triples on");
        FormatFrameLine(line, sizeof line, sum, "", 0);
        check(!strcmp(line, "60 fps average, 1% low 41 fps, worst frame 33.3 ms; triples off"), "an empty display text falls back to triples");
        char longText[400];memset(longText, 'x', sizeof longText - 1);longText[sizeof longText - 1] = 0;
        FormatFrameLine(line, sizeof line, sum, longText, 1);
        check(strlen(line) < sizeof line && !strncmp(line, "60 fps average", 14), "a long display text is cut, never overruns");
        char tiny[12];memset(tiny, '#', sizeof tiny);
        FormatFrameLine(tiny, 8, sum, "Surround span", 1);
        check(!strcmp(tiny, "60 fps ") && tiny[8] == '#', "a small buffer is truncated within its size");
    }

    if (g_failed) printf("FAIL %d of %d frame-rate checks\n", g_failed, g_checks);
    else printf("PASS %d frame-rate checks\n", g_checks);
    return g_failed ? 1 : 0;
}
