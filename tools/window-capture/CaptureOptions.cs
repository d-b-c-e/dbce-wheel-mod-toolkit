using System.Globalization;

internal sealed record CaptureCrop(int X, int Y, int Width, int Height)
{
    internal void RequireFits(int sourceWidth, int sourceHeight)
    {
        if (X < 0 || Y < 0 || Width <= 0 || Height <= 0 || sourceWidth <= 0 || sourceHeight <= 0 ||
            (long)X + Width > sourceWidth || (long)Y + Height > sourceHeight)
            throw new ArgumentException("crop exceeds the native capture frame");
    }
}

internal sealed record CaptureOptions(string Process, string Output, bool List, int Width, long? Window, bool Burst, double Duration, int MaxFrames,
    bool Metrics = false, bool NoPng = false, double WhiteFraction = .1, CaptureCrop? Crop = null)
{
    internal const string Usage = "usage: dbce-window-capture <process-name|pid> <out.png> [--width N | --crop X,Y,WIDTH,HEIGHT] [--hwnd 0xHEX]\n" +
        "       dbce-window-capture <process-name|pid> <fresh-folder> --duration SECONDS [--max-frames N] [--width N | --crop X,Y,WIDTH,HEIGHT] [--hwnd 0xHEX]\n" +
        "       dbce-window-capture <process-name|pid> <fresh-folder> --metrics --duration SECONDS [--no-png] [--white-fraction 0.1] [--max-frames N] [--width N] [--hwnd 0xHEX]\n" +
        "       dbce-window-capture <process-name|pid> --list";
    internal static CaptureOptions Parse(string[] args)
    {
        if (args.Length < 2) throw new ArgumentException(Usage);
        int width = 0, maxFrames = 300; long? window = null; double duration = 5;
        bool list = args[1] == "--list", burst = false, metrics = false, noPng = false; double whiteFraction = .1; CaptureCrop? crop = null;
        var seen = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 2; i < args.Length; i++)
        {
            if (list || !seen.Add(args[i])) throw new ArgumentException("invalid or duplicate options");
            if (args[i] == "--metrics") { metrics = true; burst = true; continue; }
            if (args[i] == "--no-png") { noPng = true; continue; }
            if (i + 1 >= args.Length) throw new ArgumentException("missing option value");
            string option = args[i], value = args[++i];
            if (option == "--width" && int.TryParse(value, out int size) && size > 0) width = size;
            else if (option == "--hwnd" && long.TryParse(value.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? value[2..] : value,
                value.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? NumberStyles.HexNumber : NumberStyles.None,
                CultureInfo.InvariantCulture, out long handle) && handle > 0) window = handle;
            else if (option == "--duration" && double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double seconds) &&
                double.IsFinite(seconds) && seconds > 0 && seconds <= 30) { duration = seconds; burst = true; }
            else if (option == "--max-frames" && int.TryParse(value, out int count) && count > 0 && count <= 300) { maxFrames = count; burst = true; }
            else if (option == "--crop")
            {
                var parts = value.Split(',');
                if (parts.Length != 4 || !int.TryParse(parts[0], NumberStyles.None, CultureInfo.InvariantCulture, out int x) ||
                    !int.TryParse(parts[1], NumberStyles.None, CultureInfo.InvariantCulture, out int y) ||
                    !int.TryParse(parts[2], NumberStyles.None, CultureInfo.InvariantCulture, out int cropWidth) ||
                    !int.TryParse(parts[3], NumberStyles.None, CultureInfo.InvariantCulture, out int cropHeight) ||
                    cropWidth <= 0 || cropHeight <= 0) throw new ArgumentException("invalid crop rectangle");
                crop = new(x, y, cropWidth, cropHeight);
            }
            else if (option == "--white-fraction" && double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double fraction) &&
                double.IsFinite(fraction) && fraction > 0 && fraction <= 1) whiteFraction = fraction;
            else throw new ArgumentException("invalid option/value: " + option);
        }
        if (!metrics && (noPng || seen.Contains("--white-fraction"))) throw new ArgumentException("metrics options require --metrics");
        if (crop != null && (width != 0 || metrics)) throw new ArgumentException("crop requires native pixels and is not supported with metrics");
        if (metrics && !noPng && maxFrames < 2) throw new ArgumentException("metrics PNGs need --max-frames >=2 for first/last");
        return new(args[0], args[1], list, width, window, burst, duration, burst ? maxFrames : 1, metrics, noPng, whiteFraction, crop);
    }
}
