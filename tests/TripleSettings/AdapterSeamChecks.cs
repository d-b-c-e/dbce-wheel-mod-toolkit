using System.Globalization;
using Dbce.Wheel.Display;

internal static class AdapterSeamChecks
{
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string why) { checks++; if (!ok) throw new Exception(why); }
        var wording = new TripleDraftText("Edit triple settings", "Menu required", "No pending triple changes", "Restart for saved triples", "Triple draft discarded");
        var active = new TripleSettings(TripleMode.Off, false, new TripleRig(700.1234567890123, 350, 700, 60, 0), false);
        TripleSettings? stored = null;
        bool editable = true;
        var draft = new TripleSettingsDraft(active, value => { stored = value; return null; }, () => editable, () => "adapter status", wording, storeAsSinglePrecision: false);
        Check(!draft.HasDraft && !draft.MatchGameFov && draft.Active == active, "Consumer defaults were replaced");
        Check(draft.SaveStatus == wording.Initial && draft.Status == "adapter status", "Consumer initial/status text lost");
        Check(double.Parse(draft.WidthText, CultureInfo.InvariantCulture) == active.Rig.PanelWidthMm, "Double measurement did not round-trip on open");
        draft.SetWidth("700.1234567890124");
        Check(draft.HasDraft, "Double edit below float precision was lost");
        Check(draft.Save() && stored?.Rig.PanelWidthMm == 700.1234567890124, "Double store silently rounded to float");
        Check(draft.SaveStatus == wording.Saved && draft.SavedPendingRestart && draft.Active == active, "Save changed active settings or lost adapter text");
        Check(!draft.Save() && draft.SaveStatus == wording.NoChanges, "No-changes text was prescribed by the shared draft");
        draft.SetEye("710");
        editable = false;
        Check(!draft.Save() && draft.SaveStatus == wording.Locked && draft.HasDraft, "Locked Save lost draft or adapter text");
        draft.SetEye("720");
        Check(draft.EyeText == "710", "Locked setter accepted an edit");
        draft.Discard();
        Check(!draft.HasDraft && draft.SaveStatus == wording.Discarded, "Discard lost saved settings or adapter text");

        // A different consumer may keep the original ConfigEntry<float> semantics.
        var floatActive = active with { Rig = new TripleRig((float)active.Rig.PanelWidthMm, 350, 700, 60, 0) };
        var floatDraft = new TripleSettingsDraft(floatActive, value => { stored = value; return null; }, () => true, () => "", wording, storeAsSinglePrecision: true);
        Check(!floatDraft.HasDraft && float.Parse(floatDraft.WidthText, CultureInfo.InvariantCulture) == (float)active.Rig.PanelWidthMm, "Float store changed on open");
        floatDraft.SetWidth("700.1234567890124");
        Check(!floatDraft.HasDraft, "A float-equivalent edit became an unsaved change");
        floatDraft.SetWidth("701.1234567890124");
        Check(floatDraft.Save() && stored?.Rig.PanelWidthMm == (double)(float)701.1234567890124, "Float store did not retain its declared precision");

        var failed = new TripleSettingsDraft(active, _ => "Store conflict.", () => true, () => "", wording, storeAsSinglePrecision: false);
        failed.SetChoice(2);
        Check(!failed.Save() && failed.HasDraft && failed.Saved == active && failed.SaveStatus == "Store conflict. The draft was kept.", "Store failure changed saved state or lost the draft");
        return checks;
    }
}
