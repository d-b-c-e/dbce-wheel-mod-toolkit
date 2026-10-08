using System.Globalization;

internal static class CommandDuration
{
    // The command help has always advertised ms=150. Keep plain numeric values
    // compatible, but never silently replace an invalid explicit duration.
    internal static int Read(string[] parts, int index, int fallback)
    {
        if (parts.Length <= index) return fallback;
        var value = parts[index];
        if (value.StartsWith("ms=", StringComparison.OrdinalIgnoreCase)) value = value[3..];
        if (!int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out var ms) || ms > 60000)
            throw new FormatException("Duration must be an integer from 0 to 60000 ms (plain or ms=).");
        return ms;
    }
}
