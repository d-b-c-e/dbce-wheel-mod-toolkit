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
#if DBCE_INPUT_NET35
            var exportedBindings = draft.Active.Bindings;
            exportedBindings.Clear();
            var exportedChanges = draft.Changes;
            exportedChanges.Clear();
            Check(draft.Active.Bindings.ContainsKey("steer") && draft.HasDraft,
                "legacy IDictionary surfaces cannot mutate active bindings or the pending draft");
#endif
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
            checks += ExactSubset(binding);
            return checks;
        }
        finally
        {
            string expectedParent = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
            if (string.Equals(Path.GetDirectoryName(Path.GetFullPath(root)), expectedParent, StringComparison.OrdinalIgnoreCase) &&
                Path.GetFileName(root).StartsWith("shared-control-draft-", StringComparison.Ordinal)) Directory.Delete(root, true);
        }
    }

    // A scalar subset of an exact declaration that also declares a stick: hash is the exact text's, the stick's
    // component keys survive an edit untouched, and a subset line missing from the exact text is refused.
    static int ExactSubset(ControlBinding steer)
    {
        int checks = 0;
        void Check(bool ok, string why) { checks++; if (!ok) throw new Exception(why); }
        const string exact = "game truckful\ncontexts observed driving\ntruckful.look stick circle contexts=driving\ntruckful.slot digital contexts=driving\n";
        var bytes = System.Text.Encoding.ASCII.GetBytes(exact);
        var subset = new[] { new DeclaredControlExtra("truckful.slot", ControlShape.Digital, 1) };
        var extras = new DeclaredControlExtras("truckful", new[] { "driving" }, subset, bytes);
        string hash = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes));
        Check(extras.Hash == hash && extras.Declaration.SequenceEqual(bytes), "exact subset keeps the host's declaration bytes and hash");
        string button = ControlBinding.FromCapture(ControlShape.Digital, ControlKind.Button, 4, 1, 0, steer.Device, steer.Product, "Fixture").ToString();
        string look = "truckful.look.x = " + steer;
        string text = "[Controls]\nSchema = 1\nProfile = Fixture\nRevision = r\nTransmission = Automatic\nsteer = " + steer +
            "\n[Extras]\nSchema = 1\nGame = truckful\nDeclaration = " + hash + "\n" + look + "\n";
        var document = new ControlProfileDocument(text, extras);
        Check(!document.Bindings.ContainsKey("truckful.slot"), "unbound subset slot reads as unbound");
        string edited = document.Edit(new Dictionary<string, string?> { ["truckful.slot"] = button }, "r2");
        Check(edited.Contains(look + "\n", StringComparison.Ordinal) && edited.Contains("Declaration = " + hash + "\n", StringComparison.Ordinal) &&
            new ControlProfileDocument(edited, extras).Bindings["truckful.slot"].ToString() == button, "edit keeps stick components and the exact hash");
        bool refused = false;
        try { _ = new DeclaredControlExtras("truckful", new[] { "driving" }, subset, System.Text.Encoding.ASCII.GetBytes("game truckful\ncontexts observed driving\ntruckful.look stick circle contexts=driving\n")); }
        catch (ArgumentException) { refused = true; }
        Check(refused, "a subset line absent from the exact declaration is refused");
        return checks;
    }
}
