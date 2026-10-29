using Dbce.Wheel.Display;

// [Triple] CurveRadiusMm: the glass-to-chord eye correction the shared geometry applies, its validation and the F6
// draft. No within-panel warp: the picture is not changed, only where the eye is taken to be.
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
        Check(curvedViews[1].Top > flatViews[1].Top && curvedViews[1].YawDegrees == 0, "Measured eye: the chord is not one sagitta closer");
        Check(curvedViews[0] != flatViews[0] && Math.Abs(curvedViews[0].YawDegrees + 70) < 1e-9, "Measured eye: side yaw left the panel angle or the side frustum did not move");
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
        Check(curvedSource[1] == curvedChord[1] && Math.Abs(TriplePanels.VerticalFov(curvedSource[1]) - 60) < 1e-9, "Centre source is not the chord");
        for (int i = 0; i < 3; i += 2)
            Check(curvedSource[i].YawDegrees == curvedChord[i].YawDegrees && curvedSource[i].Left <= curvedChord[i].Left + 1e-9 && curvedSource[i].Right >= curvedChord[i].Right - 1e-9,
                "Side source does not cover its chord at the panel's yaw");
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
}
