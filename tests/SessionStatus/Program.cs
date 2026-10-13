using Dbce.Wheel.Ui;
int checks = 0;
void Check(bool good, string reason) { checks++; if (!good) throw new Exception(reason); }
Check(SessionStatusPresentation.Describe(SessionDisplayState.Off, 0) == null, "no banner without a session");
var recording = SessionStatusPresentation.Describe(SessionDisplayState.Recording, 65);
Check(recording.Text.Contains("REC  01:05") && recording.Text.Contains("F11 to stop and save"), "active reminder");
Check(!recording.Text.Contains("muted"), "ordinary recording does not claim muted output");
Check(SessionStatusPresentation.Describe(SessionDisplayState.Recording, 65, true).Text.Contains("Outputs muted"), "muted recording labeled");
Check(!SessionStatusPresentation.Describe(SessionDisplayState.Saving, 65).Text.Contains("saved"), "stop is not completion");
Check(SessionStatusPresentation.Describe(SessionDisplayState.DataWritten, 65).Text.Contains("Finalization pending") &&
    !SessionStatusPresentation.Describe(SessionDisplayState.DataWritten, 65).Text.Contains("saved"), "source data is not a sealed package");
Check(SessionStatusPresentation.Describe(SessionDisplayState.Recording, 65, recordingStopHint: "F6 > Telemetry > Stop recording")
    .Text.Contains("F6 > Telemetry > Stop recording") &&
    !SessionStatusPresentation.Describe(SessionDisplayState.Recording, 65, recordingStopHint: "F6 > Telemetry > Stop recording")
    .Text.Contains("stop and save"), "host-specific stop hint avoids promising package save");
Check(SessionStatusPresentation.Describe(SessionDisplayState.Saved, 65).Text.StartsWith("Recording saved"), "sealed completion");
var limited = SessionStatusPresentation.Describe(SessionDisplayState.Limited, 65);
Check(limited.Tone == SessionDisplayTone.Problem, "limit stop is visibly incomplete");
Check(limited.Text.Contains("Partial recording") && !limited.Text.Contains("Recording saved"), "a limit must not claim complete success");
foreach (var state in new[] { SessionDisplayState.Incomplete, SessionDisplayState.StartFailed })
{
    var line = SessionStatusPresentation.Describe(state, 65);
    Check(line.Tone == SessionDisplayTone.Problem && !line.Text.Contains("saved"), "failure cannot claim saved");
}
Check(SessionStatusPresentation.Describe(SessionDisplayState.Playback, 0).Text.Contains("F12 to take over"), "playback takeover");
Check(SessionStatusPresentation.Describe(SessionDisplayState.PlaybackEnded, 0).Text.Contains("remain muted"), "takeover does not promise forces");
Check(SessionStatusPresentation.Describe(SessionDisplayState.PlaybackFailed, 0).Tone == SessionDisplayTone.Problem, "playback failure visible");
Check(SessionStatusPresentation.Clock(double.NaN) == "00:00", "NaN time");
Check(SessionStatusPresentation.Clock(-1) == "00:00", "negative time");
Check(SessionStatusPresentation.Clock(double.PositiveInfinity) == "5999:59", "bounded time");
foreach (var size in new[] { (1920f, 1080f, false), (2560f, 1440f, false), (7680f, 1440f, true), (11520f, 2160f, true), (800f, 600f, false) })
{
    var box = SessionStatusPresentation.Layout(size.Item1, size.Item2, size.Item3);
    float left = size.Item3 ? size.Item1 / 3 : 0, right = size.Item3 ? size.Item1 * 2 / 3 : size.Item1;
    Check(box.X > left && box.X + box.Width < right, "banner inside center display");
    Check(Math.Abs(box.X + box.Width / 2 - size.Item1 / 2) < .01, "banner centered");
    Check(box.MinHeight >= 38 && box.Scale >= .8 && box.Scale <= 2, "legible height scale");
}
Console.WriteLine($"PASS: {checks} session status checks");
