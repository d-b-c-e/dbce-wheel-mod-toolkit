using System;
using System.Runtime.InteropServices;

namespace Dbce.Wheel.Telemetry
{
    // Lineage: art-of-sim-rally ArtOfSimRally.Telemetry.ForzaPacket (offsets validated
    // live against SimHub via cruisn-collection, 2026-08); layouts added 2026-09-03.
    /// <summary>
    /// Writes the 324-byte Forza Horizon 4/5 "Data Out" UDP packet.
    /// </summary>
    /// <remarks>
    /// <para>
    /// Emitting a format consumers already understand is the whole point: SimHub,
    /// dashboards, motion rigs and bass shakers all speak Forza Data Out, so art of
    /// rally gains that entire ecosystem without anyone writing a plugin for it.
    /// </para>
    /// <para>
    /// The layout is the Forza Motorsport 7 "sled" (232 bytes) followed by a 12-byte
    /// block the Horizon titles insert, then the "dash" fields. That 12-byte insert
    /// is the whole reason FM7 dash offsets do not work here and why the packet is
    /// 324 rather than 311 bytes.
    /// </para>
    /// <para>
    /// Offsets are not guesses. <c>Speed</c> at 256 and <c>Gear</c> at 319 are the
    /// two that pin the layout, and both are copied from a harness in the sibling
    /// cruisn-collection project that was validated live against SimHub's Forza
    /// Horizon profile. Everything else follows from field order and sizes.
    /// See docs/TELEMETRY.md.
    /// </para>
    /// <para>
    /// All fields are little-endian, matching x86/x64 hosts. Unwritten bytes stay
    /// zero, which consumers read as "not supported" rather than as bad data.
    /// </para>
    /// </remarks>
    public static class ForzaPacket
    {
        /// <summary>Total FH4/FH5 Data Out packet length, in bytes.</summary>
        public const int Size = 324;

        /// <summary>Exact packet length for a layout. Receivers validate by length.</summary>
        public static int SizeOf(ForzaLayout layout)
        {
            switch (layout)
            {
                case ForzaLayout.Sled232: return 232;
                case ForzaLayout.Fm7Dash311: return 311;
                default: return Size;
            }
        }

        // --- sled (v1), offsets 0..231 -------------------------------------
        public const int OffIsRaceOn                  = 0;    // s32
        public const int OffTimestampMs               = 4;    // u32
        public const int OffEngineMaxRpm              = 8;    // f32
        public const int OffEngineIdleRpm             = 12;   // f32
        public const int OffCurrentEngineRpm          = 16;   // f32
        public const int OffAccelerationX             = 20;   // f32 x3
        public const int OffVelocityX                 = 32;   // f32 x3
        public const int OffAngularVelocityX          = 44;   // f32 x3
        public const int OffYaw                       = 56;   // f32
        public const int OffPitch                     = 60;   // f32
        public const int OffRoll                      = 64;   // f32
        public const int OffNormalizedSuspensionTravel= 68;   // f32 x4
        public const int OffTireSlipRatio             = 84;   // f32 x4
        public const int OffWheelRotationSpeed        = 100;  // f32 x4
        public const int OffWheelOnRumbleStrip        = 116;  // s32 x4
        public const int OffWheelInPuddleDepth        = 132;  // f32 x4
        public const int OffSurfaceRumble             = 148;  // f32 x4
        public const int OffTireSlipAngle             = 164;  // f32 x4
        public const int OffTireCombinedSlip          = 180;  // f32 x4
        public const int OffSuspensionTravelMeters    = 196;  // f32 x4
        public const int OffCarOrdinal                = 212;  // s32
        public const int OffCarClass                  = 216;  // s32
        public const int OffCarPerformanceIndex       = 220;  // s32
        public const int OffDrivetrainType            = 224;  // s32
        public const int OffNumCylinders              = 228;  // s32

        /// <summary>
        /// Start of the 12 bytes the Horizon titles insert between the sled and the
        /// dash section. Left zeroed; consumers skip it.
        /// </summary>
        public const int OffHorizonPadding            = 232;

        // --- dash (v2), offsets 244..323 -----------------------------------
        public const int OffPositionX                 = 244;  // f32 x3
        public const int OffSpeed                     = 256;  // f32, metres/second
        public const int OffPower                     = 260;  // f32, watts
        public const int OffTorque                    = 264;  // f32, newton-metres
        public const int OffTireTemp                  = 268;  // f32 x4
        public const int OffBoost                     = 284;  // f32
        public const int OffFuel                      = 288;  // f32
        public const int OffDistanceTraveled          = 292;  // f32
        public const int OffBestLap                   = 296;  // f32
        public const int OffLastLap                   = 300;  // f32
        public const int OffCurrentLap                = 304;  // f32
        public const int OffCurrentRaceTime           = 308;  // f32
        public const int OffLapNumber                 = 312;  // u16
        public const int OffRacePosition              = 314;  // u8
        public const int OffAccel                     = 315;  // u8  0..255
        public const int OffBrake                     = 316;  // u8  0..255
        public const int OffClutch                    = 317;  // u8  0..255
        public const int OffHandBrake                 = 318;  // u8  0..255
        public const int OffGear                      = 319;  // u8
        public const int OffSteer                     = 320;  // s8  -127..127
        public const int OffNormalizedDrivingLine     = 321;  // s8
        public const int OffNormalizedAIBrakeDiff     = 322;  // s8

        /// <summary>
        /// Byte 323 is unused by every Forza title. The cruisn-collection harness
        /// stamps a sentinel here so a listener can tell a synthetic packet from a
        /// real one; we do the same so a probe can prove packets came from this mod
        /// and not from another emitter on the same port.
        /// </summary>
        public const int OffSourceSentinel            = 323;

        /// <summary>Sentinel value written to <see cref="OffSourceSentinel"/>: ASCII 'R' for rally.</summary>
        public const byte SourceSentinel = 0x52;

        // Wheel ordering is front-left, front-right, rear-left, rear-right in
        // every Forza array. Feeding them in a different order silently produces
        // a dashboard that looks plausible and is wrong, so WheelValues names them.

        /// <summary>
        /// Serialises <paramref name="frame"/> into <paramref name="buffer"/>.
        /// </summary>
        /// <param name="frame">Physics snapshot to encode.</param>
        /// <param name="buffer">
        /// Destination, at least <see cref="Size"/> bytes. Fully overwritten, so the
        /// same buffer can be reused every frame without clearing it.
        /// </param>
        /// <exception cref="ArgumentNullException"><paramref name="buffer"/> is null.</exception>
        /// <exception cref="ArgumentException"><paramref name="buffer"/> is too small.</exception>
        public static void Write(in TelemetryFrame frame, byte[] buffer) => Write(frame, buffer, ForzaLayout.Horizon324);

        /// <summary>
        /// Serialises <paramref name="frame"/> in the given <paramref name="layout"/>.
        /// The sled (first 232 bytes) is identical in all three; the dash block sits
        /// at 232 in <see cref="ForzaLayout.Fm7Dash311"/> and at 244 in
        /// <see cref="ForzaLayout.Horizon324"/>; the source sentinel exists only in
        /// the Horizon layout's trailing byte.
        /// </summary>
        public static void Write(in TelemetryFrame frame, byte[] buffer, ForzaLayout layout)
        {
            if (buffer == null) throw new ArgumentNullException(nameof(buffer));
            int size = SizeOf(layout);
            if (buffer.Length < size)
                throw new ArgumentException(
                    $"Buffer must be at least {size} bytes for {layout}, was {buffer.Length}.", nameof(buffer));

            Array.Clear(buffer, 0, size);

            WriteInt32 (buffer, OffIsRaceOn,         frame.IsRaceOn ? 1 : 0);
            WriteUInt32(buffer, OffTimestampMs,      frame.TimestampMs);
            WriteSingle(buffer, OffEngineMaxRpm,     frame.EngineMaxRpm);
            WriteSingle(buffer, OffEngineIdleRpm,    frame.EngineIdleRpm);
            WriteSingle(buffer, OffCurrentEngineRpm, frame.CurrentEngineRpm);

            WriteVector3(buffer, OffAccelerationX,    frame.AccelerationX,    frame.AccelerationY,    frame.AccelerationZ);
            WriteVector3(buffer, OffVelocityX,        frame.VelocityX,        frame.VelocityY,        frame.VelocityZ);
            WriteVector3(buffer, OffAngularVelocityX, frame.AngularVelocityX, frame.AngularVelocityY, frame.AngularVelocityZ);

            WriteSingle(buffer, OffYaw,   frame.Yaw);
            WriteSingle(buffer, OffPitch, frame.Pitch);
            WriteSingle(buffer, OffRoll,  frame.Roll);

            WriteWheelSingles(buffer, OffNormalizedSuspensionTravel, frame.NormalizedSuspensionTravel);
            WriteWheelSingles(buffer, OffTireSlipRatio,              frame.TireSlipRatio);
            WriteWheelSingles(buffer, OffWheelRotationSpeed,         frame.WheelRotationSpeed);
            WriteWheelInt32s (buffer, OffWheelOnRumbleStrip,         frame.WheelOnRumbleStrip);
            WriteWheelSingles(buffer, OffWheelInPuddleDepth,         frame.WheelInPuddleDepth);
            WriteWheelSingles(buffer, OffSurfaceRumble,              frame.SurfaceRumble);
            WriteWheelSingles(buffer, OffTireSlipAngle,              frame.TireSlipAngle);
            WriteWheelSingles(buffer, OffTireCombinedSlip,           frame.TireCombinedSlip);
            WriteWheelSingles(buffer, OffSuspensionTravelMeters,     frame.SuspensionTravelMeters);

            WriteInt32(buffer, OffCarOrdinal,          frame.CarOrdinal);
            WriteInt32(buffer, OffCarClass,            frame.CarClass);
            WriteInt32(buffer, OffCarPerformanceIndex, frame.CarPerformanceIndex);
            WriteInt32(buffer, OffDrivetrainType,      frame.DrivetrainType);
            WriteInt32(buffer, OffNumCylinders,        frame.NumCylinders);

            if (layout == ForzaLayout.Sled232) return;

            // OffHorizonPadding..OffPositionX stays zero in the Horizon layout; the
            // FM7 dash has no insert, so every dash offset moves up by 12.
            int d = layout == ForzaLayout.Fm7Dash311 ? OffHorizonPadding - OffPositionX : 0;

            WriteVector3(buffer, OffPositionX + d, frame.PositionX, frame.PositionY, frame.PositionZ);

            WriteSingle(buffer, OffSpeed + d,  frame.Speed);
            WriteSingle(buffer, OffPower + d,  frame.Power);
            WriteSingle(buffer, OffTorque + d, frame.Torque);

            WriteWheelSingles(buffer, OffTireTemp + d, frame.TireTemp);

            WriteSingle(buffer, OffBoost + d,            frame.Boost);
            WriteSingle(buffer, OffFuel + d,             frame.Fuel);
            WriteSingle(buffer, OffDistanceTraveled + d, frame.DistanceTraveled);
            WriteSingle(buffer, OffBestLap + d,          frame.BestLap);
            WriteSingle(buffer, OffLastLap + d,          frame.LastLap);
            WriteSingle(buffer, OffCurrentLap + d,       frame.CurrentLap);
            WriteSingle(buffer, OffCurrentRaceTime + d,  frame.CurrentRaceTime);

            WriteUInt16(buffer, OffLapNumber + d, frame.LapNumber);

            buffer[OffRacePosition + d] = frame.RacePosition;
            buffer[OffAccel + d]        = frame.Accel;
            buffer[OffBrake + d]        = frame.Brake;
            buffer[OffClutch + d]       = frame.Clutch;
            buffer[OffHandBrake + d]    = frame.HandBrake;
            buffer[OffGear + d]         = frame.Gear;

            buffer[OffSteer + d]                 = unchecked((byte)frame.Steer);
            buffer[OffNormalizedDrivingLine + d] = unchecked((byte)frame.NormalizedDrivingLine);
            buffer[OffNormalizedAIBrakeDiff + d] = unchecked((byte)frame.NormalizedAIBrakeDifference);

            if (layout == ForzaLayout.Horizon324) buffer[OffSourceSentinel] = SourceSentinel;
        }

        /// <summary>Allocates a correctly sized packet buffer.</summary>
        public static byte[] CreateBuffer() => new byte[Size];

        // --- primitives ----------------------------------------------------
        // BitConverter.GetBytes would allocate on every call, and this runs 60+
        // times a second inside a Unity FixedUpdate; write in place instead.

        private static void WriteInt32(byte[] b, int off, int v) => WriteUInt32(b, off, unchecked((uint)v));

        private static void WriteUInt32(byte[] b, int off, uint v)
        {
            b[off]     = (byte)v;
            b[off + 1] = (byte)(v >> 8);
            b[off + 2] = (byte)(v >> 16);
            b[off + 3] = (byte)(v >> 24);
        }

        private static void WriteUInt16(byte[] b, int off, ushort v)
        {
            b[off]     = (byte)v;
            b[off + 1] = (byte)(v >> 8);
        }

        // Reinterpret float bits without `unsafe` (keeps the csproj plain) and
        // without BitConverter.SingleToInt32Bits, which does not exist on the
        // netstandard2.0 surface this has to run against under Unity's Mono.
        [StructLayout(LayoutKind.Explicit)]
        private struct FloatBits
        {
            [FieldOffset(0)] public float Single;
            [FieldOffset(0)] public uint  Bits;
        }

        private static void WriteSingle(byte[] b, int off, float v)
        {
            FloatBits u = default;
            u.Single = v;
            WriteUInt32(b, off, u.Bits);
        }

        private static void WriteVector3(byte[] b, int off, float x, float y, float z)
        {
            WriteSingle(b, off,     x);
            WriteSingle(b, off + 4, y);
            WriteSingle(b, off + 8, z);
        }

        private static void WriteWheelSingles(byte[] b, int off, WheelValues v)
        {
            WriteSingle(b, off,      v.FrontLeft);
            WriteSingle(b, off + 4,  v.FrontRight);
            WriteSingle(b, off + 8,  v.RearLeft);
            WriteSingle(b, off + 12, v.RearRight);
        }

        private static void WriteWheelInt32s(byte[] b, int off, WheelValues v)
        {
            WriteInt32(b, off,      (int)v.FrontLeft);
            WriteInt32(b, off + 4,  (int)v.FrontRight);
            WriteInt32(b, off + 8,  (int)v.RearLeft);
            WriteInt32(b, off + 12, (int)v.RearRight);
        }
    }
}
