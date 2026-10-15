using Dbce.Wheel.Input;

internal static class DraftTests
{
    internal static int Run()
    {
        int checks = 0;
        void Check(bool ok, string why) { checks++; if (!ok) throw new Exception(why); }
        string root = Path.Combine(Path.GetTempPath(), "shared-control-draft-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            string path = Path.Combine(root, "dbce-controls.ini");
            var binding = ControlBinding.FromCapture("steer", ControlKind.Axis, 0, 1, 32767,
                new Guid("11111111-1111-1111-1111-111111111111"), new Guid("22222222-2222-2222-2222-222222222222"), "Fixture");
            string original = "[Controls]\nSchema = 1\nProfile = Fixture\nRevision = active\nTransmission = Automatic\nsteer = " + binding + "\nFuture = keep\n";
            File.WriteAllText(path, original, new System.Text.UTF8Encoding(false));
            var draft = new ControlProfileDraft(File.ReadAllBytes(path), 1024);
            draft.Set("steer", binding.WithInverted(true).ToString());
            Check(draft.HasDraft && !draft.Active.Bindings["steer"].Inverted, "edit remains a draft over frozen active input");
            Check(draft.Save(path, out var message), message);
            Check(!draft.HasDraft && draft.SavedPendingRestart && !draft.Active.Bindings["steer"].Inverted && draft.Saved.Bindings["steer"].Inverted,
                "Save promotes only next-start state");
            Check(File.ReadAllText(path).Contains("Future = keep", StringComparison.Ordinal) && Directory.GetFiles(root, "*.bindings-backup-*").Length == 1,
                "checked transaction preserves unknown lines and backup");
            draft.Set("steer", null);
            Check(draft.Changes.ContainsKey("steer") && draft.Binding("steer") == null, "Clear is explicit pending state");
            Check(!draft.Save(path, out message) && draft.HasDraft, "refused invalid profile retains the draft");
            draft.Discard();
            Check(!draft.HasDraft && draft.SavedPendingRestart, "Discard cannot erase saved pending state");
            draft.Set("steer", binding.ToString()); File.AppendAllText(path, new string('x', 2048));
            Check(!draft.Save(path, out message) && draft.HasDraft && message.Contains("size", StringComparison.Ordinal), "bounded changed-source refusal retains the draft");
            Check(Directory.GetFiles(root, "*.tmp").Length == 0, "failed Save leaves no temporary files");
            return checks;
        }
        finally
        {
            string expectedParent = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
            if (string.Equals(Path.GetDirectoryName(Path.GetFullPath(root)), expectedParent, StringComparison.OrdinalIgnoreCase) &&
                Path.GetFileName(root).StartsWith("shared-control-draft-", StringComparison.Ordinal)) Directory.Delete(root, true);
        }
    }
}
