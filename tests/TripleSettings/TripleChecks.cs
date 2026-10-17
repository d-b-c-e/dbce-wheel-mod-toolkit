using Dbce.Wheel.Display;

internal static class TripleChecks
{
    // Qualified Truckful defaults are fixture data, never shared player defaults.
    private static readonly TripleRig FixtureRig = new(708.4, 398.5, 660, 70, 8);
    private static readonly TripleSettings FixtureSettings = new(TripleMode.Off, false, FixtureRig, true);
    private static readonly TripleDraftText FixtureText = new(
        "Changes stay in a draft until Save screens.",
        "Save needs focused settings in a menu. The draft was kept.",
        "No screen changes to save.",
        "Screens saved for next start. Restart the game to use them.",
        "Unsaved screen edits discarded. The current view is unchanged.");
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string why) { if (!ok) throw new Exception(why); checks++; }
        static bool Near(double a, double b, double tolerance = 1e-9) => Math.Abs(a - b) <= tolerance;
        static double Deg(double tangent) => Math.Atan(tangent) * 180 / Math.PI;

        // Physical owner rig: 3x 32" 16:9 (708.4 x 398.5 mm visible), eye 660 mm, sides turned 70 degrees, 8 mm bezel.
        var rig = FixtureRig;
        Check(rig.Validation() == null, "Owner rig defaults refused");
        var views = TriplePanels.Compute(rig);
        var (left, center, right) = (views[0], views[1], views[2]);
        Check(Near(center.YawDegrees, 0) && Near(center.Left, -center.Right) && Near(center.Bottom, -center.Top),
            "Center view is not straight ahead and symmetric");
        Check(Near(center.Right, rig.PanelWidthMm / 2 / rig.EyeDistanceMm) && Near(center.Top, rig.PanelHeightMm / 2 / rig.EyeDistanceMm),
            "Center frustum differs from the physical panel over the eye distance");
        Check(Near(TriplePanels.VerticalFov(center), 2 * Deg(rig.PanelHeightMm / 2 / rig.EyeDistanceMm)), "Physical vertical FOV differs");
        Check(Near(right.YawDegrees, -left.YawDegrees) && right.YawDegrees > 0 && left.YawDegrees < 0,
            "Side views do not turn outward symmetrically (left negative, right positive in Unity camera space)");
        Check(Near(left.Left, -right.Right) && Near(left.Right, -right.Left) && Near(left.Top, right.Top) && Near(left.Bottom, right.Bottom),
            "Left and right frustums are not mirror images");
        // The scene behind a bezel is skipped: the right panel's inner edge looks further right than the center's edge.
        double centerEdge = Deg(center.Right), rightInner = right.YawDegrees + Deg(right.Left);
        Check(rightInner > centerEdge && rightInner - centerEdge < 5, "Bezel gap is not skipped as a small angular step");

        // Flat, gapless rig: one continuous plane.
        var flat = TriplePanels.Compute(rig with { SideAngle = 0, BezelMm = 0 });
        Check(Near(flat[2].YawDegrees, 0) && Near(flat[2].Left, flat[1].Right) && Near(flat[0].Right, flat[1].Left),
            "Flat gapless rig does not continue the center plane");
        // Turned, gapless rig: the seam rays meet exactly.
        var seamless = TriplePanels.Compute(rig with { BezelMm = 0 });
        Check(Near(seamless[2].YawDegrees + Deg(seamless[2].Left), Deg(seamless[1].Right), 1e-7), "Gapless seam is not continuous");

        // Matching the game's chase framing keeps its vertical FOV on the center panel, with the same rig proportions.
        foreach (double fov in new[] { 40.0, 60.0, 75.5 })
        {
            var matched = TriplePanels.Compute(rig, fov);
            Check(Near(TriplePanels.VerticalFov(matched[1]), fov, 1e-7), "Center does not keep the game's vertical FOV " + fov);
            Check(Near(matched[1].Right / matched[1].Top, rig.PanelWidthMm / rig.PanelHeightMm), "Center aspect is not the panel aspect");
            Check(Near(matched[2].YawDegrees, views[2].YawDegrees, 5) && matched[2].YawDegrees > 0, "Matched framing changed the side turn direction");
        }
        foreach (double bad in new[] { 0.0, 1.0, 179.0, double.NaN })
        {
            bool refused = false; try { TriplePanels.Compute(rig, bad); } catch (ArgumentOutOfRangeException) { refused = true; }
            Check(refused, "Unusable game FOV accepted: " + bad);
        }

        // Unity off-center matrix: a symmetric frustum is Unity's ordinary perspective; an offset one shears.
        var p = OffCenterProjection.From(center, .3, 1000);
        Check(Near(p.M02, 0) && Near(p.M12, 0) && Near(p.M11, 1 / center.Top) && Near(p.M00, 1 / center.Right) &&
            Near(p.M22, -(1000 + .3) / (1000 - .3)) && Near(p.M23, -2 * 1000 * .3 / (1000 - .3)), "Symmetric projection differs from Unity's perspective");
        var side = OffCenterProjection.From(right, .3, 1000);
        Check(Math.Abs(side.M02) > 1e-6 && Near(side.M12, 0), "Side projection is not horizontally off-center only");
        bool badPlanes = false; try { OffCenterProjection.From(center, 1, 1); } catch (ArgumentOutOfRangeException) { badPlanes = true; }
        Check(badPlanes, "Degenerate clip planes accepted");

        // Saved geometry bounds (Wheelkit catalog ranges; the angle stays below 90 for the projection).
        Check(rig with { SideAngle = 89 } is { } edge && edge.Validation() == null, "89-degree side angle refused");
        foreach (var bad in new[] { rig with { PanelWidthMm = 99 }, rig with { PanelHeightMm = 2001 }, rig with { EyeDistanceMm = 3001 },
            rig with { SideAngle = 90 }, rig with { SideAngle = -1 }, rig with { BezelMm = 101 }, rig with { PanelWidthMm = double.NaN } })
            Check(bad.Validation() != null, "Out-of-range rig accepted: " + bad);

        // Activation gate (Auto needs a window three screens wide).
        Check(TripleGate.Inactive(TripleMode.Off, 7680, 1440) == "turned off", "Off did not keep the stock view");
        Check(TripleGate.Inactive(TripleMode.Auto, 2560, 1440) != null && TripleGate.Inactive(TripleMode.Auto, 7680, 1440) == null &&
            TripleGate.Inactive(TripleMode.Auto, 5760, 1080) == null, "Auto width test differs");
        Check(TripleGate.Inactive(TripleMode.On, 2560, 1440) == null && TripleGate.Inactive(TripleMode.On, 0, 0) != null, "On does not force views");

        // STD-022 selector over the shared keys: Off, Surround, Separate monitors; a hand-set On is kept.
        Check(TripleLayoutChoice.Labels.SequenceEqual(new[] { "Off", "Surround", "Separate monitors" }), "Selector labels differ");
        foreach (var (choice, mode, span) in new[] { (0, TripleMode.Off, false), (1, TripleMode.Auto, false), (2, TripleMode.Auto, true) })
        {
            Check(TripleLayoutChoice.Apply(choice, TripleMode.Off) == (mode, span), "Choice mapping differs: " + choice);
            Check(TripleLayoutChoice.Current(mode, span) == choice, "Choice round trip differs: " + choice);
        }
        Check(TripleLayoutChoice.Apply(2, TripleMode.On) == (TripleMode.On, true) && TripleLayoutChoice.Current(TripleMode.On, false) == 1,
            "Hand-set On was not kept");

        // STD-015 span policy on the owner's monitors: three 2560x1440 panels plus a 1920x1080 display above the row.
        var owner = RacingRowPolicy.Select(new[] { new MonitorRect(-2560, 0, 2560, 1440, false), new MonitorRect(0, 0, 2560, 1440, true),
            new MonitorRect(2560, 0, 2560, 1440, false), new MonitorRect(314, -1080, 1920, 1080, false) });
        var layout = new DesktopLayout(owner, 2560, 1440);
        Check(layout.SeparateTriple && layout.X == -2560 && layout.Width == 7680 && layout.Height == 1440 && layout.Monitors == 3,
            "Owner racing row differs");
        Check(SpanPolicy.Decide(true, layout, 2560, 1440, false, "") == SpanAction.ApplySpan, "Fullscreen primary did not apply the span");
        Check(SpanPolicy.Decide(true, layout, 7680, 1440, true, "") == SpanAction.PlaceOnly, "Restored span was not placed only");
        Check(SpanPolicy.Decide(false, layout, 7680, 1440, true, "7680x1440") == SpanAction.RestoreFullscreen, "Turning off did not return the mod's span");
        Check(SpanPolicy.Decide(false, layout, 1920, 1080, true, "7680x1440") == SpanAction.ForgetMarker, "A player-chosen window was touched");
        Check(SpanPolicy.Decide(false, layout, 2560, 1440, false, "") == SpanAction.None, "Off with no span changed the window");
        var surround = new DesktopLayout(RacingRowPolicy.Select(new[] { new MonitorRect(0, 0, 7680, 1440, true) }), 7680, 1440);
        Check(!surround.SeparateTriple && SpanPolicy.Decide(true, surround, 7680, 1440, false, "") == SpanAction.None, "Surround desktop was spanned");
        Check(SpanPolicy.Marker(7680, 1440) == "7680x1440", "Span marker format differs");

        // F6 draft: saved for next start, focused-menu edits only, validation before save.
        bool editable = true; TripleSettings? saved = null;
        var draft = new TripleSettingsDraft(FixtureSettings with { Rig = Stored(rig) }, s => { saved = s; return null; }, () => editable, () => "status", FixtureText, storeAsSinglePrecision: true);
        Check(!draft.HasDraft && draft.Choice == 0 && draft.MatchGameFov, "Fresh draft is not the saved default");
        draft.SetChoice(2); draft.SetAngle("55");
        Check(draft.HasDraft && draft.Validation == null && draft.Save() && saved is { Mode: TripleMode.Auto, Span: true } s1 && Near(s1.Rig.SideAngle, 55),
            "Separate monitors with a new angle did not save as Auto + span");
        Check(draft.SavedPendingRestart && !draft.HasDraft, "Saved choice is not pending the next start");
        draft.SetBezel("abc");
        Check(draft.Validation != null && !draft.Save(), "Non-numeric measurement saved");
        draft.Discard();
        Check(!draft.HasDraft && draft.BezelText == "8", "Discard did not restore the saved measurement");
        editable = false; draft.SetChoice(0);
        Check(draft.Choice == 2, "Edit accepted outside a focused menu");
        editable = true; draft.SetWidth("50");
        Check(draft.Validation == "Panel width must be 100 to 3000 mm." && !draft.Save(), "Out-of-range width saved");
        Check(draft.HoldClose() && draft.CloseWarning && !draft.HoldClose(), "Unsaved screen edits did not warn once before closing");
        // A precise saved measurement opens unchanged (no edit on open, no silent geometry rewrite).
        var precise = new TripleSettingsDraft(FixtureSettings with { Rig = Stored(rig with { PanelWidthMm = 700.1234 }) }, _ => null, () => true, () => "", FixtureText, storeAsSinglePrecision: true);
        Check(!precise.HasDraft && float.Parse(precise.WidthText, System.Globalization.CultureInfo.InvariantCulture) == 700.1234f,
            "A precise saved measurement opened as an unsaved edit");
        return checks;
    }

    // The config file stores floats; drafts compare against what the file holds.
    private static TripleRig Stored(TripleRig r) =>
        new((float)r.PanelWidthMm, (float)r.PanelHeightMm, (float)r.EyeDistanceMm, (float)r.SideAngle, (float)r.BezelMm);
}
