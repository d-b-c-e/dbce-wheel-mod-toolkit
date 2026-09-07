namespace Dbce.Wheel.Ffb
{
    /// <summary>Stable UI status codes; consumers may localize their own wording.</summary>
    public enum DeviceSelectionStatus { Automatic, Matched, NoDevices, Missing, Ambiguous, NoForceFeedback, VirtualDevice }

    /// <summary>A read-only selection decision, not evidence of a connected or acquired wheel.</summary>
    public sealed class DeviceSelectionResult
    {
        public DeviceSelectionStatus Status { get; }
        /// <summary>Position in the supplied snapshot; -1 on failure. Not necessarily the native device index.</summary>
        public int ArrayPosition { get; }
        public WheelFfbNative.DeviceInfo? Device { get; }
        public bool Success => ArrayPosition >= 0;
        /// <summary>Save only after a deliberate picker choice. Null means stable identity is unavailable.
        /// Resolving an old name or index alone is not permission to rewrite a saved preference.</summary>
        public string PersistentId => Device?.InstanceGuid?.ToString();
        public string Message
        {
            get
            {
                switch (Status)
                {
                    case DeviceSelectionStatus.Automatic: return "A force-feedback wheel is available. Select it to remember your choice.";
                    case DeviceSelectionStatus.Matched: return "Your selected wheel is available.";
                    case DeviceSelectionStatus.NoDevices: return "No wheels detected. Connect and power on your wheel, then refresh.";
                    case DeviceSelectionStatus.Missing: return "Your saved wheel is unavailable. Reconnect it and refresh, or choose a replacement.";
                    case DeviceSelectionStatus.Ambiguous: return "More than one device matches. Choose the specific wheel from the device list.";
                    case DeviceSelectionStatus.VirtualDevice: return "This is a virtual controller. Choose your physical force-feedback wheel.";
                    default: return "The selected device does not report force-feedback support. Choose a force-feedback wheel.";
                }
            }
        }
        internal DeviceSelectionResult(DeviceSelectionStatus status, int position = -1, WheelFfbNative.DeviceInfo? device = null)
        { Status = status; ArrayPosition = position; Device = device; }
    }
}
