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
        public Guid? InstanceGuid;

        public bool IsEmpty => !InstanceGuid.HasValue && Vid < 0 && Index < 0 && string.IsNullOrEmpty(NameContains);

        public static DevicePreference Parse(string text)
        {
            var p = new DevicePreference();
            if (string.IsNullOrWhiteSpace(text)) return p;
            text = text.Trim();
            if (Guid.TryParse(text, out Guid instance)) { p.InstanceGuid = instance; return p; }
            if (text.StartsWith("#") && int.TryParse(text.Substring(1), NumberStyles.Integer, CultureInfo.InvariantCulture, out int idx) && idx >= 0) { p.Index = idx; return p; }
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
            InstanceGuid.HasValue ? InstanceGuid.Value.ToString() : Vid >= 0 ? Vid.ToString("x4") + ":" + Pid.ToString("x4") : Index >= 0 ? "#" + Index : NameContains;

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
        /// Returns an array position. An explicit preference must match exactly one
        /// FFB device; missing/ambiguous preferences never fall back to another wheel.
        /// Without a preference, picks the first FFB-capable non-virtual device.
        /// VID/PID requires identity metadata supplied by the caller; the legacy DLL
        /// enumeration exposes instance GUIDs but does not expose product IDs.
        /// </summary>
        public int Choose(WheelFfbNative.DeviceInfo[] devices) => Resolve(devices).ArrayPosition;

        /// <summary>Explains a selection for setup UI without loading or acquiring a device.
        /// Success identifies a candidate only; native strict selection must still enforce identity at open.</summary>
        public DeviceSelectionResult Resolve(WheelFfbNative.DeviceInfo[] devices)
        {
            if (devices == null || devices.Length == 0)
                return new DeviceSelectionResult(IsEmpty ? DeviceSelectionStatus.NoDevices : DeviceSelectionStatus.Missing);
            int selected = -1;
            if (!IsEmpty)
            {
                for (int i = 0; i < devices.Length; i++)
                {
                    var d = devices[i];
                    bool matches = InstanceGuid.HasValue ? d.InstanceGuid == InstanceGuid
                        : Vid >= 0 ? d.VendorId == Vid && d.ProductId == Pid
                        : Index >= 0 ? i == Index
                        : d.Name != null && d.Name.IndexOf(NameContains, StringComparison.OrdinalIgnoreCase) >= 0;
                    if (!matches) continue;
                    if (selected >= 0) return new DeviceSelectionResult(DeviceSelectionStatus.Ambiguous);
                    selected = i;
                }
                if (selected < 0) return new DeviceSelectionResult(DeviceSelectionStatus.Missing);
                if (IsVirtualDevice(devices[selected].Name))
                    return new DeviceSelectionResult(DeviceSelectionStatus.VirtualDevice);
                if (!devices[selected].ForceFeedback)
                    return new DeviceSelectionResult(DeviceSelectionStatus.NoForceFeedback);
            }
            else
            {
                for (int i = 0; i < devices.Length; i++)
                    if (devices[i].ForceFeedback && !IsVirtualDevice(devices[i].Name)) { selected = i; break; }
                if (selected < 0) return new DeviceSelectionResult(DeviceSelectionStatus.NoForceFeedback);
            }
            return new DeviceSelectionResult(IsEmpty ? DeviceSelectionStatus.Automatic : DeviceSelectionStatus.Matched,
                selected, devices[selected]);
        }
    }
}
