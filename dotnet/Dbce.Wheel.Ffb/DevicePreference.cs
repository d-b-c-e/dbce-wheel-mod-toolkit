using System;
using System.Globalization;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// Which device the user meant. Accepts <c>vid:pid</c> hex (the precise,
    /// portable key — <c>346e:0006</c> is a MOZA R12), <c>#N</c> for an index, or a
    /// case-insensitive name substring. Also knows which "controllers" are virtual
    /// pads that must never be picked as a wheel.
    /// </summary>
    /// <remarks>
    /// vJoy reports 8 axes / 128 buttons like a MOZA R12, advertises force
    /// feedback, and often enumerates first; a game or mod that takes "first FFB
    /// device" picks it and every force is absorbed silently. That cost a long
    /// debugging session and a wrongly-filed upstream bug (OutRun2006Tweaks-FFB,
    /// commit 02e3028). Blocklist first, rank FFB-capable real devices, then apply
    /// the preference.
    /// </remarks>
    public sealed class DevicePreference
    {
        public int Vid = -1, Pid = -1;
        public int Index = -1;
        public string NameContains = "";

        public bool IsEmpty => Vid < 0 && Index < 0 && string.IsNullOrEmpty(NameContains);

        public static DevicePreference Parse(string text)
        {
            var p = new DevicePreference();
            if (string.IsNullOrWhiteSpace(text)) return p;
            text = text.Trim();
            if (text.StartsWith("#") && int.TryParse(text.Substring(1), NumberStyles.Integer, CultureInfo.InvariantCulture, out int idx)) { p.Index = idx; return p; }
            var parts = text.Split(':');
            if (parts.Length == 2
                && int.TryParse(parts[0], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out int vid)
                && int.TryParse(parts[1], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out int pid)
                && parts[0].Length <= 4 && parts[1].Length <= 4)
            { p.Vid = vid; p.Pid = pid; return p; }
            p.NameContains = text;
            return p;
        }

        public override string ToString() =>
            Vid >= 0 ? Vid.ToString("x4") + ":" + Pid.ToString("x4") : Index >= 0 ? "#" + Index : NameContains;

        /// <summary>DirectInput's <c>guidProduct.Data1</c> for a VID/PID pair: <c>(PID &lt;&lt; 16) | VID</c>.</summary>
        public static uint ProductKey(int vid, int pid) => ((uint)pid << 16) | (uint)(vid & 0xFFFF);

        private static readonly string[] VirtualMarkers = { "vjoy", "vigem", "xoutput", "vxbox", "vgamepad", "emulated", "virtual" };

        /// <summary>True for known virtual/emulated controllers that must not be treated as a wheel.</summary>
        public static bool IsVirtualDevice(string name)
        {
            if (string.IsNullOrEmpty(name)) return false;
            string n = name.ToLowerInvariant();
            foreach (var m in VirtualMarkers) if (n.Contains(m)) return true;
            return false;
        }

        /// <summary>
        /// Picks a device index from a list: the preference if it matches; otherwise
        /// the first force-feedback-capable real device; otherwise -1.
        /// </summary>
        public int Choose(WheelFfbNative.DeviceInfo[] devices)
        {
            if (devices == null || devices.Length == 0) return -1;
            if (Index >= 0 && Index < devices.Length) return Index;
            if (!string.IsNullOrEmpty(NameContains))
                for (int i = 0; i < devices.Length; i++)
                    if (devices[i].Name != null && devices[i].Name.IndexOf(NameContains, StringComparison.OrdinalIgnoreCase) >= 0) return i;
            for (int i = 0; i < devices.Length; i++)
                if (devices[i].ForceFeedback && !IsVirtualDevice(devices[i].Name)) return i;
            return -1;
        }
    }
}
