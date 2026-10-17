using Dbce.Wheel.Ui;
int checks = 0;
void Check(bool good, string reason) { checks++; if (!good) throw new Exception(reason); }

// Palette follows Wheelkit's dark theme.
Check(Math.Abs(F6Palette.Accent.R - 0x55 / 255f) < 1e-6 && Math.Abs(F6Palette.Accent.B - 0xE8 / 255f) < 1e-6, "accent is Wheelkit #55B9E8");
Check(F6Palette.Text.A == 1f && F6Palette.Text.WithAlpha(.5f).A == .5f, "opaque tokens, alpha override");

// Rows: two columns when they fit, the fixed editor column, stacked when narrow.
var wide = F6RowLayout.For(820, F6Metrics.ControlColumn);
Check(!wide.Stacked && wide.EditorWidth == 340 && wide.LabelWidth == 820 - 340 - F6Metrics.ColumnGap, "wide row: label + 340 editor");
var wideChoice = F6RowLayout.For(820, 420);
Check(!wideChoice.Stacked && wideChoice.EditorWidth == 420, "an editor that measures wider than the column widens it");
Check(F6RowLayout.For(700, F6Metrics.ControlColumn).Stacked, "below 720 usable units rows stack (review)");
var narrow = F6RowLayout.For(440, F6Metrics.ControlColumn);
Check(narrow.Stacked && narrow.LabelWidth == 440 && narrow.EditorWidth == 440, "narrow body stacks label over a full-width editor");
var tooWide = F6RowLayout.For(820, 900);
Check(tooWide.Stacked && tooWide.EditorWidth == 820, "an editor wider than the body stacks and never exceeds it");
Check(F6RowLayout.For(float.NaN, 300).Stacked, "an invalid width stacks instead of throwing");

// Number editor: fixed parts take their metrics, the slider takes the rest (review: slider >= 120).
var editor = F6NumberEditor.For(F6Metrics.ControlColumn);
Check(editor.SliderWidth == 340 - (2 * 36 + 80 + 3 * 6) && editor.MinusX == editor.SliderWidth + 6 && editor.PlusX + 36 == 340, "340 editor: slider 170, parts end at the edge");
var withDefault = F6NumberEditor.For(436, 90);
Check(withDefault.DefaultX + 90 == 436 && withDefault.SliderWidth == 436 - (2 * 36 + 80 + 3 * 6 + 96), "a Default command takes its width plus a gap");
Check(F6NumberEditor.For(100).SliderWidth == F6Metrics.SliderMinWidth && F6NumberEditor.Needs() == 120 + 2 * 36 + 80 + 18, "the slider never shrinks below its minimum");

// Flow: items wrap into lines and never exceed the available width.
int count;
var lines = F6Flow.Lines(new[] { 100f, 100f, 100f }, 320, 6, out count);
Check(count == 1 && lines.SequenceEqual(new[] { 0, 0, 0 }), "three 100s fit in 320 with gaps");
lines = F6Flow.Lines(new[] { 100f, 150f, 120f }, 260, 6, out count);
Check(count == 2 && lines.SequenceEqual(new[] { 0, 0, 1 }), "the third item wraps to a second line");
lines = F6Flow.Lines(new[] { 500f, 50f }, 300, 6, out count);
Check(count == 2 && lines[0] == 0 && lines[1] == 1, "an item wider than the line takes a line of its own");
lines = F6Flow.Lines(new float[0], 300, 6, out count);
Check(count == 0 && lines.Length == 0, "no items, no lines");
lines = F6Flow.Lines(null!, 300, 6, out count);
Check(count == 0 && lines.Length == 0, "null items retain the empty layout behavior");

var save = new BindingSaveStatus();
save.Record(false, "Reader disconnected; draft kept.");
Check(BindingPresentation.Footer(true, true, save.SaveFailure) == "Bindings not saved — Reader disconnected; draft kept.", "persistent save failure outranks draft/restart status");
Check(BindingPresentation.Footer(false, false, save.SaveFailure).Contains("Reader disconnected"), "unrelated host status does not clear save failure");
save.Discard();
Check(BindingPresentation.Footer(false, false, save.SaveFailure) == "", "discarded unsaved-only draft has no restart claim");
Check(BindingPresentation.Footer(true, true, save.SaveFailure) == BindingPresentation.Unsaved, "new draft outranks prior saved revision");
save.Record(true, "");
Check(BindingPresentation.Footer(false, true, save.SaveFailure) == BindingPresentation.SavedForNextStart, "saved is explicitly next-start, not active");
Check(BindingPresentation.Footer(false, false, " \t") == "", "blank failure is not an error");
foreach (var prefix in new[] { "Bindings not saved:", "Bindings not saved —", "bindings not saved -" })
    Check(BindingPresentation.Footer(true, false, prefix + " Reader disconnected.") == "Bindings not saved — Reader disconnected.", "existing writer prefix appears once");
Check(BindingPresentation.Footer(true, false, "Bindings not saved:") == "Bindings not saved", "empty labeled reason has no dangling separator");
Check(BindingPresentation.SetupGuidance(true).Contains("Find devices") && !BindingPresentation.SetupGuidance(false).Contains("Find devices"), "discovery guidance is capability dependent");
Check(BindingPresentation.SetupGuidance(false).Contains("Set Up Controls.bat") && BindingPresentation.SetupGuidance(false).Contains("Wheelkit is optional"), "non-discovery hosts retain the standalone setup route");
Console.WriteLine($"F6Style: {checks} checks passed");
