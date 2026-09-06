using System;
namespace Dbce.Wheel.Telemetry
{
    public enum SignalSource : byte { Unavailable = 0, HudOcr = 1, NumericHud = 2, GameMemory = 3, Derived = 4 }
    public enum SignalQuality : byte { Invalid = 0, Fresh = 1, Held = 2 }
    public enum SignalUnit : byte { Unknown = 0, MetresPerSecond = 1, Rpm = 2, NormalizedForce = 3, Radians = 4 }
    /// <summary>Scalar provenance alongside TelemetryFrame; never added implicitly to a Forza packet.</summary>
    public struct SignalSample
    {
        public const uint Schema = 1;
        public double Value, Seconds;
        public ulong Frame;
        public SignalSource Source;
        public SignalQuality Quality;
        public SignalUnit Unit;
        private static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
        public bool Usable(double now, double maxAge) =>
            (Quality == SignalQuality.Fresh || Quality == SignalQuality.Held) &&
            Source >= SignalSource.HudOcr && Source <= SignalSource.Derived &&
            Unit >= SignalUnit.MetresPerSecond && Unit <= SignalUnit.Radians &&
            Finite(Value) && Finite(Seconds) && Seconds >= 0 && Finite(now) &&
            now >= Seconds && Finite(maxAge) && maxAge >= 0 && now - Seconds <= maxAge;
    }
}
