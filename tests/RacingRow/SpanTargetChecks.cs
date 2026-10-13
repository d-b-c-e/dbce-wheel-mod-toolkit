using Dbce.Wheel.Display;

internal static class SpanTargetChecks
{
    public static void Run(Action<bool, string> check)
    {
        var layout = new List<MonitorRect> {
            new(-2560, 0, 2560, 1440, false), new(0, 0, 2560, 1440, true),
            new(2560, 0, 2560, 1440, false), new(314, -1080, 1920, 1080, false)
        };
        double clock = 50; int reads = 0; bool fail = false;
        var target = new SpanTarget(() => {
            reads++;
            if (fail) throw new InvalidOperationException("monitor query failed");
            return RacingRowPolicy.Select(layout);
        }, () => clock);
        check(target.Decide(false, true, out _) == null && reads == 0, "saved opt-out does not query displays");
        check(target.Decide(true, false, out _) == null && reads == 0, "triples-off does not query displays");
        check(target.Decide(true, true, out var changed) == (-2560, 0, 7680, 1440) && changed && reads == 1,
            "cached span excludes dashboard");
        for (int i = 1; i < 120; i++) {
            clock = 50 + i / 60.0;
            target.Decide(true, true, out changed);
            check(!changed && reads == 1, "per-frame callers reuse observation and do not repeat log");
        }
        clock = 52; fail = true;
        check(target.Decide(true, true, out changed) == null && changed && reads == 2,
            "failed refresh drops stale span");
        check(target.Reason.Contains("could not be read"), "query failure is reported");
        clock = 53; fail = false;
        check(target.Decide(true, true, out _) == null && reads == 2, "failure is cached too");
        clock = 54;
        check(target.Decide(true, true, out changed) == (-2560, 0, 7680, 1440) && changed && reads == 3,
            "next refresh recovers");
        layout[3] = new MonitorRect(314, -200, 1920, 1080, false); clock = 56;
        check(target.Decide(true, true, out changed) == null && changed, "overlapping dashboard invalidates span");
        layout.Clear(); layout.Add(new MonitorRect(0, 0, 7680, 1440, true)); clock = 58;
        check(target.Decide(true, true, out _) == null, "wide primary keeps consumer Surround path");
    }
}
