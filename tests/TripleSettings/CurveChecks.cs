using Dbce.TripleScreen;
using Dbce.Wheel.Display;

// [Triple] CurveRadiusMm: the measured eye distance is to the plane through the panel edges (STD-047), so the curve
// leaves the chord frustums alone and only warps the redrawn picture; its validation and the F6 draft.
internal static class CurveChecks
{
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string why) { if (!ok) throw new Exception(why); checks++; }

        // Float-exact values, as a single-precision store returns them.
        var flat = new TripleRig((float)708.4, (float)398.5, 660, 70, 8);
        var curved = flat with { CurveRadiusMm = 1500 };
        Check(flat.Validation() == null && curved.Validation() == null, "Flat or 1500R rig refused");
        Check(flat.CurveDepth() == 0 && curved.CurveDepth() > 40 && curved.CurveDepth() < 45, "1500R sagitta of a 708 mm chord is not about 42 mm");
        foreach (var bad in new[] { flat with { CurveRadiusMm = 300 }, flat with { CurveRadiusMm = 20000 }, flat with { CurveRadiusMm = double.NaN },
            flat with { CurveRadiusMm = -1500 }, flat with { EyeDistanceMm = 100, PanelWidthMm = 3000, CurveRadiusMm = 1500 } })
            Check(bad.Validation() != null, "Unusable curve accepted: " + bad);
        Check(TriplePanels.Compute(flat).SequenceEqual(TriplePanels.Compute(flat with { CurveRadiusMm = 0 })), "Radius 0 is not the flat rig");

        var flatViews = TriplePanels.Compute(flat);
        var curvedViews = TriplePanels.Compute(curved);
        for (int i = 0; i < 3; i++)
            Check(Near(curvedViews[i], flatViews[i]), "Measured eye: the curve moved a chord off the panel-edge plane (STD-047)");
        Check(Math.Abs(curvedViews[0].YawDegrees + 70) < 1e-9, "Measured eye: side yaw left the panel angle");
        foreach (var rig in new[] { flat, curved })
            Check(Math.Abs(TriplePanels.VerticalFov(TriplePanels.Compute(rig, 60)[1]) - 60) < 1e-9, "Game framing lost the game's vertical FOV on the chord");

        // The warped route: flat rigs are untouched. A curved panel's edges lie on its chord and the glass bows away, so
        // the eye rays through the glass meet the chord plane inside it: the centre source is the chord itself (the game
        // vertical FOV stays on it) and the redraw samples inward, magnifying the glass middle; a side panel's source
        // covers its chord (equal to it for this rig).
        var flatSource = TriplePanels.ComputeWarped(flat, 60, out var flatWarps);
        Check(flatSource.SequenceEqual(TriplePanels.Compute(flat, 60)) && flatWarps.All(w => w.IsFlat), "Warped route changed a flat rig");
        var curvedSource = TriplePanels.ComputeWarped(curved, 60, out var curvedWarps);
        var curvedChord = TriplePanels.Compute(curved, 60);
        Check(curvedWarps.All(w => !w.IsFlat && w.RadiusMm == 1500), "Curved warps are not 1500R");
        // Game framing with the redraw is framed as without it (STD-047: the eye to the panel-edge plane, no curve depth
        // added or taken off, as art of rally), and the warp stays the real eye's (STD-041): its panel is the measured
        // rig's, and it samples the virtual views exactly as the measured-eye warp does.
        for (int i = 0; i < 3; i++)
            Check(Near(curvedSource[i], curvedChord[i]), "Warped game framing differs from the chord framing (a curve depth crept in)");
        Check(Math.Abs(TriplePanels.VerticalFov(curvedSource[1]) - 60) < 1e-9, "Warped game framing lost the game's vertical FOV on the chord");
        Check(Math.Abs(-curvedWarps[1].Panel.Center.Z - 660) < 1e-9, "The warp is not the real eye's (measured rig)");
        var measured = TriplePanels.ComputeWarped(curved, null, out var measuredWarps);
        curvedWarps[1].SourceUv(.25, 1, out double gu, out double gv);
        measuredWarps[1].SourceUv(.25, 1, out double mu, out double mv);
        Check(Math.Abs(gu - mu) < 1e-12 && Math.Abs(gv - mv) < 1e-12, "Game framing's warp is not the real eye's mapping");
        // Every physical ray lands inside the drawn source, for the measured eye and for game lenses either side of it
        // (from DRIVE Rally's camera-set checks), and the zoom limits still frame.
        foreach (double? lens in new double?[] { null, 35, 65, 80 })
        {
            TriplePanels.ComputeWarped(curved, lens, out var lensWarps);
            for (int i = 0; i < 3; i++)
            {
                Check(Math.Abs(lensWarps[i].Panel.Center.Z - measuredWarps[i].Panel.Center.Z) < 1e-9, "Warp left the physical rig at lens " + lens);
                for (int y = 0; y <= 36; y++) for (int x = 0; x <= 64; x++)
                {
                    lensWarps[i].SourceUv(x / 64d, y / 36d, out double u, out double v);
                    if (!(double.IsFinite(u) && double.IsFinite(v) && u >= -1e-9 && u <= 1 + 1e-9 && v >= -1e-9 && v <= 1 + 1e-9))
                        throw new Exception($"Physical ray outside the drawn source at lens {lens}, panel {i}, sample {x},{y}");
                }
            }
        }
        Check(TriplePanels.Compute(flat, 1).Length == 3 && TriplePanels.Compute(flat, 179).Length == 3, "The zoom limits (1 and 179 degrees) do not frame");
        var measuredChord = TriplePanels.Compute(curved);
        Check(Math.Abs(-measuredWarps[1].Panel.Center.Z - 660) < 1e-9, "Measured-eye warped chord left the panel-edge plane");
        for (int i = 0; i < 3; i++)
            Check(Near(measured[i], measuredChord[i]) && curvedSource[i].YawDegrees == curvedChord[i].YawDegrees, "Measured-eye warped frustum differs from the chord frustum");
        curvedWarps[1].SourceUv(.5, .5, out double su, out double sv);
        Check(Math.Abs(su - .5) < 1e-9 && Math.Abs(sv - .5) < 1e-9, "Panel middle does not sample the source middle");
        curvedWarps[1].SourceUv(0, 0, out su, out sv);
        Check(Math.Abs(su) < 1e-9 && Math.Abs(sv) < 1e-9, "Panel corner (on the chord) does not sample the source corner");
        curvedWarps[1].SourceUv(.25, 1, out su, out sv);
        Check(su > .25 && su < .5 && sv < 1 && sv > .9, "Glass quarter does not sample inward toward the middle");

        TripleSettings? saved = null;
        var draft = new TripleSettingsDraft(new TripleSettings(TripleMode.Off, false, flat, true), s => { saved = s; return null; }, () => true, () => "",
            new TripleDraftText("", "", "", "saved", ""), storeAsSinglePrecision: true);
        Check(draft.CurveText == "0" && !draft.HasDraft, "Opening the page edited the radius");
        draft.SetCurve("1500");
        Check(draft.HasDraft && draft.Validation == null && draft.Save() && saved!.Value.Rig.CurveRadiusMm == 1500, "F6 did not save the radius");
        draft.SetCurve("100");
        Check(draft.Validation != null && !draft.Save(), "F6 saved a radius under half the panel width");
        return checks;
    }

    // Projecting a panel's chord source surface (the chord itself for an eye in front) matches the chord to rounding.
    private static bool Near(PanelFrustum a, PanelFrustum b) =>
        Math.Abs(a.YawDegrees - b.YawDegrees) < 1e-9 && Math.Abs(a.Left - b.Left) < 1e-9 && Math.Abs(a.Right - b.Right) < 1e-9 &&
        Math.Abs(a.Bottom - b.Bottom) < 1e-9 && Math.Abs(a.Top - b.Top) < 1e-9;
}
