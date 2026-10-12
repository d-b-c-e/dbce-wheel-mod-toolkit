using System.Globalization;

internal sealed record CaptureOptions(string Process, string Output, bool List, int Width, long? Window, bool Burst, double Duration, int MaxFrames)
{
    internal const string Usage = "usage: dbce-window-capture <process-name|pid> <out.png> [--width N] [--hwnd 0xHEX]\n" +
        "       dbce-window-capture <process-name|pid> <fresh-folder> --duration SECONDS [--max-frames N] [--width N] [--hwnd 0xHEX]\n" +
        "       dbce-window-capture <process-name|pid> --list";
    internal static CaptureOptions Parse(string[] args)
    {
        if (args.Length < 2) throw new ArgumentException(Usage);
        int width = 0, maxFrames = 300; long? window = null; double duration = 5;
        bool list = args[1] == "--list", burst = false;
        var seen = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 2; i < args.Length; i++)
        {
            if (list || i + 1 >= args.Length || !seen.Add(args[i])) throw new ArgumentException("invalid or duplicate options");
            string option = args[i], value = args[++i];
            if (option == "--width" && int.TryParse(value, out int size) && size > 0) width = size;
            else if (option == "--hwnd" && long.TryParse(value.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? value[2..] : value,
                value.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? NumberStyles.HexNumber : NumberStyles.None,
                CultureInfo.InvariantCulture, out long handle) && handle > 0) window = handle;
            else if (option == "--duration" && double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double seconds) &&
                double.IsFinite(seconds) && seconds > 0 && seconds <= 30) { duration = seconds; burst = true; }
            else if (option == "--max-frames" && int.TryParse(value, out int count) && count > 0 && count <= 300) { maxFrames = count; burst = true; }
            else throw new ArgumentException("invalid option/value: " + option);
        }
        return new(args[0], args[1], list, width, window, burst, duration, burst ? maxFrames : 1);
    }
}
