using Dbce.Wheel.Input;

internal static class BindingCaptureFlowTests
{
    internal static int Run()
    {
        int checks = 0, delivered = 0;
        double now = 0;
        var wheel = Guid.Parse("11111111-1111-1111-1111-111111111111");
        var other = Guid.Parse("33333333-3333-3333-3333-333333333333");
        var product = Guid.Parse("22222222-2222-2222-2222-222222222222");
        string? resultAction = null;
        ControlBinding? result = null;
        void Check(bool ok, string why) { checks++; if (!ok) throw new Exception(why); }
        AcceptedBindingObservation Read(ulong seq, int axis = 511, bool button = false, ulong epoch = 1,
            string path = "opaque-path", Guid? instance = null, int max = 1023, double? seen = null) =>
            new(instance ?? wheel, product, "Fixture", path, epoch, seq, seen ?? now,
                new BindingCaptureSnapshot(new[] { axis }, new[] { button }, new[] { -1 }), new[] { 0 }, new[] { max });
        var flow = new SharedBindingCaptureFlow(() => now, (action, binding) => { delivered++; resultAction = action; result = binding; return null; });
        Check(!flow.Begin("steer", wheel, true), "no fabricated reader fallback");
        flow.Observe(Read(1), true);
        Check(flow.Begin("steer", wheel, true) && flow.SecondsLeft == 11, "axis capture uses qualified reader and 11-second deadline");
        now = .5; flow.Observe(Read(2, 800), true);
        Check(delivered == 0, "axis rest interval cannot capture movement");
        now = 1; flow.Observe(Read(3), true);
        now = 1.01; flow.Observe(Read(4, 800), true);
        Check(delivered == 1 && resultAction == "steer" && result is { Index: 0, Rest: 511, Travel: 1, Minimum: 0, Maximum: 1023 }, "shared canonical result carries measured range/rest/direction");
        Check(result!.Device == wheel && result.Product == product && flow.CapturingAction == null, "exact identities and one completed capture");
        flow.Observe(Read(5), true); Check(delivered == 1, "completed capture cannot deliver twice");
        Check(flow.Begin("confirm", wheel, true) && flow.SecondsLeft == 10, "digital deadline is 10 seconds");
        flow.Observe(Read(6, button: true), true); // Initially held.
        flow.Observe(Read(7), true);
        flow.Observe(Read(8, button: true), true);
        Check(delivered == 2 && resultAction == "confirm" && result!.Kind == ControlKind.Button, "held digital input requires release and repress");
        flow.Begin("confirm", wheel, true);
        flow.Remove(wheel, "epoch ended: explicit fixture reason");
        Check(flow.CapturingAction == null && flow.Message.Contains("epoch ended"), "Remove surfaces adapter reason and cancels selected device");
        Check(!flow.Observe(Read(8), true) && !flow.HasFreshObservation, "Remove retains sequence watermark against cached replay");
        flow.Observe(Read(9), true); flow.Begin("steer", wheel, true);
        now += .3; flow.Observe(Read(9), true);
        Check(flow.CapturingAction == null && flow.Observations.Count == 0, "duplicate sequence cannot refresh Seen");
        flow.Observe(Read(10), true); flow.Begin("steer", wheel, true);
        flow.Observe(Read(9), true); now += .3; flow.Refresh(true);
        Check(flow.CapturingAction == null, "regressed sequence cannot refresh Seen");
        flow.Observe(Read(11), true); flow.Begin("steer", wheel, true);
        flow.Observe(Read(1, epoch: 2), true);
        Check(flow.CapturingAction == null, "per-instance epoch change cancels before retargeting");
        flow.Begin("steer", wheel, true);
        flow.Observe(Read(1, epoch: 1, instance: other), true);
        Check(flow.CapturingAction == "steer" && flow.Observations.Count == 2, "another device may have the same or a lower epoch");
        flow.Remove(other, "other removed");
        Check(flow.CapturingAction == "steer", "another device loss does not cancel the selected capture");
        flow.Observe(Read(2, epoch: 2, path: "different-path"), true);
        Check(flow.CapturingAction == null, "opaque identity change cancels");
        flow.Begin("confirm", wheel, true);
        flow.Observe(Read(3, axis: 50000, button: true, epoch: 2, path: "different-path"), true);
        Check(flow.CapturingAction == null && delivered == 2 && flow.Observations.Count == 0, "invalid known-range axis rejects simultaneous button");
        flow.Observe(Read(4, axis: 0, epoch: 2, max: 0), true);
        Check(!flow.Begin("steer", wheel, true), "unknown ranges cannot qualify steering");
        flow.Begin("confirm", wheel, true); flow.Refresh(false);
        Check(flow.CapturingAction == null && flow.Message.Contains("focus"), "host edit-policy loss cancels");
        flow.Observe(Read(5, epoch: 2), true); flow.Begin("confirm", wheel, true);
        for (ulong seq = 6; seq <= 106; seq++) { now += .1; flow.Observe(Read(seq, epoch: 2), true); }
        Check(flow.CapturingAction == null && flow.Message.Contains("timed out") && delivered == 2, "deadline wins with continuing fresh unchanged observations");
        flow.Begin("steer", wheel, true); flow.ReaderStopped();
        Check(flow.CapturingAction == null && flow.Observations.Count == 0, "stopped reader clears device eligibility");
        Check(!flow.Observe(Read(106, epoch: 2), true) && !flow.HasFreshObservation, "stop retains sequence watermark against cached replay");
        Check(!flow.Observe(Read(1, epoch: 1), true), "recreated reader cannot reuse an old flow's lifetime");
        var recreated = new SharedBindingCaptureFlow(() => now, (_, _) => null);
        Check(recreated.Observe(Read(1), true) && recreated.Begin("steer", wheel, true), "new qualified reader lifetime gets a new flow and may start epoch 1");
        Check(!flow.Observe(Read(107, epoch: 2, seen: now + 1), true), "future Seen cannot fabricate freshness");
        var axes = new[] { 511 }; var minimum = new[] { 0 }; var maximum = new[] { 1023 };
        var copied = new AcceptedBindingObservation(wheel, product, "Fixture", "path", 3, 1, now,
            new BindingCaptureSnapshot(axes, new[] { false }, new[] { -1 }), minimum, maximum);
        axes[0] = 50000; minimum[0] = 50000; maximum[0] = 50000;
        Check(flow.Observe(copied, true) && flow.Begin("steer", wheel, true), "reader buffer reuse cannot alter owned observation");
        flow.Cancel(); Check(delivered == 2, "explicit Cancel never edits prior draft");
        var existing = ControlBinding.FromCapture("steer", ControlKind.Axis, 0, 1, 511, wheel, product, "Fixture", 0, 1023);
        var draft = new ControlProfileDraft(System.Text.Encoding.UTF8.GetBytes("[Controls]\nSchema = 1\nTransmission = Automatic\nsteer = " + existing + "\n"));
        bool admit = false;
        int attempts = 0;
        var refusal = new SharedBindingCaptureFlow(() => now, (action, binding) => {
            attempts++;
            if (!admit) return "fixture draft admission changed";
            draft.Set(action, binding.ToString()); return null;
        });
        refusal.Observe(Read(1), true); refusal.Begin("confirm", wheel, true);
        refusal.Observe(Read(2), true); refusal.Observe(Read(3, button: true), true);
        Check(attempts == 1 && refusal.CapturingAction == null && !draft.HasDraft && draft.Binding("steer")!.ToString() == existing.ToString(), "refused result keeps real prior draft and ends the attempt once");
        Check(refusal.Message.Contains("fixture draft admission changed") && !refusal.Message.Contains("captured into"), "refusal cannot become a captured-success message");
        admit = true; refusal.Observe(Read(4), true); refusal.Begin("confirm", wheel, true);
        refusal.Observe(Read(5), true); refusal.Observe(Read(6, button: true), true);
        Check(attempts == 2 && draft.HasDraft && draft.Binding("confirm")!.Kind == ControlKind.Button, "accepted callback applies the canonical result to the real draft");
        return checks;
    }
}
