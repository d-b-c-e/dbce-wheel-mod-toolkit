using System;
using System.IO;
using System.Text;
using Dbce.Wheel.Support;
#if DBCE_SUPPORT_NET35
using InvalidDataException = System.IO.IOException;
#endif

internal static class Program
{
    private static int _checks;
    private static void Check(bool condition, string message)
    { if (!condition) throw new Exception(message); _checks++; }
    private static void Refuses<T>(Action action) where T : Exception
    { try { action(); } catch (T) { _checks++; return; } throw new Exception("Expected " + typeof(T).Name); }

    private static void Main()
    {
        string root = Path.Combine(Path.GetTempPath(), "dbce-support-fixture-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            string redacted = SupportReport.Redact("C:\\Users\\fixture\\game\\log.txt\nuser@example.com\n" +
                "Authorization: Bearer abc\n{\"password\":\"hidden\"}\nghp_abcdefghijklmnopqrstuv", "C:\\Users\\fixture");
            foreach (string privateValue in new[] { "fixture", "user@example.com", "Bearer abc", "hidden", "ghp_" })
                Check(!redacted.Contains(privateValue), "Redaction: " + privateValue);
            Check(redacted.Contains("%USERPROFILE%") && redacted.Contains("game\\log.txt"), "Useful path suffix retained");
            Check(SupportReport.ValidRepository("owner/repo-name"), "Valid packaged repository");
            foreach (string bad in new[] { "https://github.com/owner/repo", "owner/..", "owner/repo\n", "owner/repo?x=1", "owner/repo/extra" })
                Check(!SupportReport.ValidRepository(bad), "Refuse repository routing injection");

            Refuses<ArgumentException>(() => SupportReport.Prepare(product: new ReportProduct("Game", "1", "owner/repo"), title: "\t\u2003 ", description: "description"));
            Refuses<InvalidDataException>(() => SupportReport.SelectedLogs(new[] { "\t\u2003 " }));
#if DBCE_SUPPORT_NET35
            System.Collections.Generic.IList<string> legacyPaths = new System.Collections.Generic.List<string>();
            Refuses<InvalidDataException>(() => SupportReport.SelectedLogs(legacyPaths));
            Refuses<InvalidDataException>(() => SupportReport.Redact(new string('x', 50001), "C:\\Users\\fixture"));
            string legacyHome = Environment.GetEnvironmentVariable("USERPROFILE");
            if (!string.IsNullOrEmpty(legacyHome))
                Check(!SupportReport.Redact(legacyHome + "\\mod.log").Contains(legacyHome), "Legacy default home redaction");
#endif

            string utf8 = Path.Combine(root, "mod.log");
            File.WriteAllText(utf8, "earlier\nLatest steering sample\n", new UTF8Encoding(false));
            string[] paths = { utf8 };
            var product = new ReportProduct("Fixture game", "1.2.3", "owner/repo");
            Check(product.ModPage.AbsoluteUri == "https://github.com/owner/repo" &&
                product.WheelkitPage.AbsoluteUri == "https://github.com/d-b-c-e/dbce-wheelkit", "Standard Help links");
            var prepared = SupportReport.Prepare(product, "Steering & camera?", "Steps to reproduce", paths);
            paths[0] = Path.Combine(root, "missing.log");
            File.WriteAllText(utf8, "changed after preview");
            Check(prepared.Body.Contains("Latest steering sample") && !prepared.Body.Contains("changed after preview"), "Preview is a snapshot");
            Check(!prepared.Body.Contains("Wheelkit"), "Standalone report has no Wheelkit assumption");
            Refuses<InvalidOperationException>(() => prepared.CreateBrowserSubmission(false));
            var browser = prepared.CreateBrowserSubmission(true);
            Check(browser.Url.Host == "github.com" && browser.Url.AbsolutePath == "/owner/repo/issues/new", "Exact packaged issue route");
            Check(browser.Url.AbsoluteUri.Contains("title=Steering%20%26%20camera%3F"), "Title query escaping");
            Check(!browser.NeedsAttachment && Uri.UnescapeDataString(browser.Url.Query).Contains("Latest steering sample"), "Small reviewed body is prefilled");

            string reportPath = Path.Combine(root, "support-report.txt");
            prepared.WriteReviewFile(reportPath);
            Check(File.ReadAllText(reportPath) == prepared.Preview, "Export is exact reviewed snapshot");
            Refuses<IOException>(() => prepared.WriteReviewFile(reportPath));
            Check(File.ReadAllText(reportPath) == prepared.Preview, "Existing report preserved");
            Refuses<ArgumentException>(() => prepared.WriteReviewFile(Path.Combine(root, "config.json")));
            Refuses<InvalidDataException>(() => SupportReport.SelectedLogs(new string[0]));
            Refuses<InvalidDataException>(() => SupportReport.SelectedLogs(new[] { utf8, utf8 }));
            Refuses<InvalidDataException>(() => SupportReport.SelectedLogs(new[] { utf8, utf8, utf8, utf8, utf8, utf8 }));
            Refuses<InvalidDataException>(() => SupportReport.SelectedLogs(new[] { Path.Combine(root, "save.exe") }));

            string utf16 = Path.Combine(root, "unity.log");
            File.WriteAllText(utf16, new string('x', 5000) + "\nLatest UTF16 sample\n", Encoding.Unicode);
            string tail = SupportReport.LogTail(utf16, 4800);
            Check(tail.Contains("Latest UTF16 sample") && !tail.Contains("xxxxx") && !tail.Contains("\0"), "UTF16 tail aligns and omits partial first line");
            File.WriteAllText(utf16, new string('x', 5000) + "\nLatest BE sample\n", Encoding.BigEndianUnicode);
            Check(SupportReport.LogTail(utf16, 4800).Contains("Latest BE sample"), "UTF16 BE tail");
            File.WriteAllText(utf8, new string('\u00e9', 5000) + "\nLatest UTF8 sample\n", new UTF8Encoding(false));
            Check(SupportReport.LogTail(utf8, 257).Contains("Latest UTF8 sample"), "Split UTF8 first sequence omitted before decoding");
            File.WriteAllText(utf8, new string('s', 10000), new UTF8Encoding(false));
            Check(!SupportReport.LogTail(utf8, 256).Contains("ssss"), "No complete tail line: omit possible credential fragment");
            File.WriteAllBytes(utf8, new byte[] { 0, 1, 2, 3 });
            Refuses<InvalidDataException>(() => SupportReport.LogTail(utf8));
            File.WriteAllBytes(utf8, new byte[] { 0xff, 0xff, 0x61 });
            Refuses<InvalidDataException>(() => SupportReport.LogTail(utf8));

            // The attachment fallback contains setup and a meaningful description before any upload.
            string longDescription = "Camera loses center after restart. " + new string('d', 9000);
            var large = SupportReport.Prepare(product, "Camera issue", longDescription);
            var fallback = large.CreateBrowserSubmission(true);
            Check(fallback.NeedsAttachment && fallback.Url.AbsoluteUri.Length <= SupportReport.BrowserUrlLimit, "Bounded browser fallback");
            Check(fallback.IssueBody.Contains("Camera loses center") && fallback.IssueBody.Contains("Fixture game") &&
                fallback.IssueBody.Contains("1.2.3") && fallback.IssueBody.Contains("support-report.txt"), "Useful issue before attachment");
            Check(fallback.Instructions.Contains("uploads the file immediately"), "Honest attachment timing");
            Check(large.Body.Contains(new string('d', 9000)), "Fallback never truncates local full report");
            var secretDescription = SupportReport.Prepare(product, "Issue", "password=" + new string('a', 1000));
            Check(!secretDescription.Body.Contains("aaaa"), "Redact complete description before shortening summary");

            var five = new string[5];
            for (int i = 0; i < five.Length; i++)
            {
                five[i] = Path.Combine(root, "log-" + i + ".log");
                File.WriteAllText(five[i], new string('x', 25000) + "\nLatest " + i + "\n", new UTF8Encoding(false));
            }
            string selected = SupportReport.SelectedLogs(five);
            for (int i = 0; i < five.Length; i++) Check(selected.Contains("Latest " + i), "Each selected log gets bounded share");
            Check(selected.Length < 25000, "Five logs do not each receive the whole byte budget");
            Check(!selected.Contains(root), "Report uses basenames, never selected full paths");
            Console.WriteLine("Support report checks passed: " + _checks);
        }
        finally { Directory.Delete(root, true); }
    }
}
