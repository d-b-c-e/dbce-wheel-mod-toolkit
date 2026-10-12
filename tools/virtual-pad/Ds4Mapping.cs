// The DualShock 4 target's mapping of the pad's command words (opt-in --target ds4). Pure: no ViGEm reference, tested in
// tests/Ds4Tests.cs. A ViGEm DualShock 4 is a plain HID gamepad, which background DirectInput readers see like a wheel.
// The ViGEm Xbox 360 target's input reached XInput but not a background DirectInput reader (hub
// artifacts/outrun-shim-pad-input-20261012, C1b).
internal static class Ds4Mapping
{
    // The report's sticks are 0..255 with 128 at rest; Y grows downward (0 is up), so a command's +y (up) is inverted.
    internal static byte Stick(double value, bool vertical)
    {
        if (double.IsNaN(value)) throw new FormatException("stick value is not a number");
        double v = Math.Clamp(value, -1, 1);
        return (byte)Math.Clamp(Math.Round(128 + (vertical ? -v : v) * 127), 1, 255);
    }
    // Both axes computed and validated before either is set (sana 41924): an invalid Y never leaves X moved.
    internal static void Stick(double x, double y, Action<byte, byte> set)
    {
        byte bx = Stick(x, false), by = Stick(y, true);
        set(bx, by);
    }
    internal static byte Trigger(double value)
    {
        if (double.IsNaN(value)) throw new FormatException("trigger value is not a number");
        return (byte)Math.Round(Math.Clamp(value, 0, 1) * 255);
    }
    // The Xbox words the pad already takes, mapped to the DualShock 4 button in the same place.
    internal static readonly IReadOnlyDictionary<string, string> Buttons = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
    {
        ["A"] = "Cross", ["B"] = "Circle", ["X"] = "Square", ["Y"] = "Triangle",
        ["Start"] = "Options", ["Back"] = "Share", ["LB"] = "ShoulderLeft", ["RB"] = "ShoulderRight",
        ["LS"] = "ThumbLeft", ["RS"] = "ThumbRight",
    };
    internal const string Guide = "Ps";   // the special button
    internal static readonly IReadOnlySet<string> DPad = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "Up", "Down", "Left", "Right" };
    // The hat from the held d-pad words, by DualShock4DPadDirection name. Opposite words cancel.
    internal static string Hat(bool up, bool down, bool left, bool right)
    {
        int y = (up ? 1 : 0) - (down ? 1 : 0), x = (right ? 1 : 0) - (left ? 1 : 0);
        return (x, y) switch
        {
            (0, 1) => "North", (1, 1) => "Northeast", (1, 0) => "East", (1, -1) => "Southeast",
            (0, -1) => "South", (-1, -1) => "Southwest", (-1, 0) => "West", (-1, 1) => "Northwest", _ => "None",
        };
    }
}
