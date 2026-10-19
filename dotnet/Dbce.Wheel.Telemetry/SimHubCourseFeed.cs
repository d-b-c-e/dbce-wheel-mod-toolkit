using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>Wire types SimHub's external-sim parser reads (its BinaryDataType, subset used here).</summary>
    public enum SimHubWire : byte
    {
        UInt16,
        Int32,
        UInt32,
        Float32,
        Double,
        /// <summary>Fixed-length, zero-terminated, zero-padded UTF-8.</summary>
        Utf8,
    }

    /// <summary>One declared standard field: SimHub's StandardFieldType number, name and wire format.</summary>
    public struct SimHubField
    {
        public int StandardFieldType;
        public string Name;
        public SimHubWire Wire;
        public int Size;

        public SimHubField(int standardFieldType, string name, SimHubWire wire, int size)
        {
            StandardFieldType = standardFieldType;
            Name = name;
            Wire = wire;
            Size = size;
        }
    }

    /// <summary>SimHub's 55-byte packed external-sim packet header (SimHub 9.13.2 <c>TelemetryHeader</c>).</summary>
    public struct SimHubHeader
    {
        public uint GameSignature;
        public uint TelemetrySignature;
        public ushort LayoutMajorVersion;
        public ushort LayoutMinorVersion;
        public ulong EmitterInstanceId;
        public byte PacketId;
        public ulong PacketsCounter;
        public byte IsSessionRunning;
        public byte IsSessionPaused;
        public ulong SessionId;
        public byte IsReplay;
        public byte IsUserInControl;
        public byte IsAIInControl;
        public byte IsSpectator;
        public double SessionTimeSeconds;
        public uint PhysicsDiscontinuityCounter;
    }

    /// <summary>Text fields a dash shows beside the course: car and gear.</summary>
    public struct SimHubDashText
    {
        /// <summary>Display name of the car; empty when the game does not provide one.</summary>
        public string VehicleName;

        /// <summary>Stable car id (asset name); empty when unknown.</summary>
        public string VehicleId;

        /// <summary>"R", "N" or "1".."n" (SimHub's Gear is text); see <see cref="SimHubCourseFeed.GearText"/>.</summary>
        public string Gear;
    }

    /// <summary>The decoded values of one packet, for offline checks and tests.</summary>
    public sealed class SimHubCoursePacket
    {
        public SimHubHeader Header;
        public readonly Dictionary<string, double> Numbers = new Dictionary<string, double>();
        public readonly Dictionary<string, string> Texts = new Dictionary<string, string>();
    }

    /// <summary>
    /// Draft "dbce course + dash v1" layout for a SimHub external-sim definition, and its encoder.
    /// Nothing here opens a socket or touches SimHub's files.
    /// </summary>
    /// <remarks>
    /// <para>
    /// SimHub 9.11.5 added external-sim definitions (<c>.simdef</c>); this layout was read off
    /// SimHub 9.13.2. A packet is the 55-byte header followed by each declared field in
    /// definition order, little-endian, packed. SimHub rejects a packet whose length or
    /// signatures differ from the definition.
    /// </para>
    /// <para>
    /// Field order is ascending StandardFieldType because SimHub's editor sorts standard
    /// fields that way (<c>TelemetryDefinition.NormalizeOrder</c>). It also means a standard
    /// field added later lands in the middle of the packet, not at the end: that is a major
    /// layout change. So this draft declares the full dash set now (speed, rpm, pedals, gear,
    /// car, lap times) beside the course fields, and a course feed can carry a game's
    /// dashboard once SimHub selects the definition for that game's process.
    /// </para>
    /// <para>
    /// Signatures are not computed here. SimHub's editor computes them for the definition;
    /// the emitter copies them into <see cref="SimHubHeader"/>. The editor's own generated
    /// code is the contract this draft must be compared with before any live use.
    /// </para>
    /// </remarks>
    public static class SimHubCourseFeed
    {
        public const int HeaderSize = 55;
        public const ushort LayoutMajor = 1;
        public const ushort LayoutMinor = 0;

        /// <summary>Declared fields, wire types and sizes as SimHub 9.13.2 defines them.</summary>
        public static readonly IReadOnlyList<SimHubField> Fields = new[]
        {
            new SimHubField(0,  "YawDegrees",           SimHubWire.Float32, 4),
            new SimHubField(12, "VehiclePositionNorth", SimHubWire.Double,  8),
            new SimHubField(13, "VehiclePositionEast",  SimHubWire.Double,  8),
            new SimHubField(14, "VehiclePositionUp",    SimHubWire.Double,  8),
            new SimHubField(16, "SpeedKmh",             SimHubWire.Float32, 4),
            new SimHubField(18, "VehicleName",          SimHubWire.Utf8,    256),
            new SimHubField(19, "VehicleId",            SimHubWire.Utf8,    128),
            new SimHubField(20, "EnvironmentName",      SimHubWire.Utf8,    256),
            new SimHubField(21, "EnvironmentId",        SimHubWire.Utf8,    128),
            new SimHubField(25, "EngineRpm",            SimHubWire.Float32, 4),
            new SimHubField(26, "EngineMaxRpm",         SimHubWire.Float32, 4),
            new SimHubField(31, "Throttle",             SimHubWire.Float32, 4),
            new SimHubField(32, "Brake",                SimHubWire.Float32, 4),
            new SimHubField(33, "Clutch",               SimHubWire.Float32, 4),
            new SimHubField(34, "Handbrake",            SimHubWire.Float32, 4),
            new SimHubField(37, "TrackLength",          SimHubWire.Double,  8),
            new SimHubField(38, "Gear",                 SimHubWire.Utf8,    8),
            new SimHubField(46, "TrackProgression",     SimHubWire.Float32, 4),
            new SimHubField(49, "CompletedLaps",        SimHubWire.UInt32,  4),
            new SimHubField(52, "CurrentLapTime",       SimHubWire.Double,  8),
            new SimHubField(53, "LastLapTimeSeconds",   SimHubWire.Double,  8),
            new SimHubField(54, "BestLapTimeSeconds",   SimHubWire.Double,  8),
        };

        public static readonly int PacketSize = ComputePacketSize();

        public static byte[] CreateBuffer() => new byte[PacketSize];

        /// <summary>Gear as SimHub's text: negative = "R", 0 = "N", otherwise the number.</summary>
        /// <remarks>
        /// <see cref="TelemetryFrame.Gear"/> cannot be used directly: Forza reports both reverse
        /// and neutral as 0. The adapter converts the game's own gear to this signed form.
        /// </remarks>
        public static string GearText(int gear) =>
            gear < 0 ? "R" : gear == 0 ? "N" : gear.ToString(CultureInfo.InvariantCulture);

        /// <summary>
        /// Writes one packet. The header's session id, replay, control, spectator and
        /// discontinuity fields are taken from <paramref name="course"/> so the two cannot disagree.
        /// </summary>
        public static bool TryWrite(byte[] buffer, SimHubHeader header, CourseSample course, TelemetryFrame dash,
            SimHubDashText text, out string error)
        {
            if (buffer == null || buffer.Length != PacketSize)
            {
                error = "buffer must be exactly " + PacketSize + " bytes";
                return false;
            }
            if (!course.TryValidate(out error)) return false;

            header.SessionId = course.SessionId;
            header.IsReplay = course.IsReplay ? (byte)1 : (byte)0;
            header.IsUserInControl = course.IsUserInControl ? (byte)1 : (byte)0;
            header.IsSpectator = course.IsSpectating ? (byte)1 : (byte)0;
            header.PhysicsDiscontinuityCounter = course.DiscontinuityCount;

            var numbers = new Dictionary<string, double>
            {
                ["YawDegrees"] = course.YawDeg,
                ["VehiclePositionNorth"] = course.Position.North,
                ["VehiclePositionEast"] = course.Position.East,
                ["VehiclePositionUp"] = course.Position.Up,
                ["SpeedKmh"] = dash.Speed * 3.6,
                ["EngineRpm"] = dash.CurrentEngineRpm,
                ["EngineMaxRpm"] = dash.EngineMaxRpm,
                ["Throttle"] = dash.Accel / 255.0,
                ["Brake"] = dash.Brake / 255.0,
                ["Clutch"] = dash.Clutch / 255.0,
                ["Handbrake"] = dash.HandBrake / 255.0,
                ["TrackLength"] = course.LengthM,
                ["TrackProgression"] = course.Progress01,
                ["CompletedLaps"] = course.CompletedLaps,
                ["CurrentLapTime"] = course.LapTimeS,
                ["LastLapTimeSeconds"] = dash.LastLap,
                ["BestLapTimeSeconds"] = dash.BestLap,
            };
            var texts = new Dictionary<string, string>
            {
                ["VehicleName"] = text.VehicleName ?? "",
                ["VehicleId"] = text.VehicleId ?? "",
                ["EnvironmentName"] = course.CourseName ?? "",
                ["EnvironmentId"] = course.CourseId,
                ["Gear"] = text.Gear ?? "",
            };

            foreach (var field in Fields)
            {
                if (field.Wire == SimHubWire.Utf8)
                {
                    int bytes = Encoding.UTF8.GetByteCount(texts[field.Name]);
                    if (bytes > field.Size - 1)
                    {
                        error = field.Name + " is " + bytes + " UTF-8 bytes; at most " + (field.Size - 1) + " fit";
                        return false;
                    }
                }
                else if (!CourseVector.Finite(numbers[field.Name]))
                {
                    error = field.Name + " is not finite";
                    return false;
                }
            }

            Array.Clear(buffer, 0, buffer.Length);
            WriteHeader(buffer, header);
            int offset = HeaderSize;
            foreach (var field in Fields)
            {
                if (field.Wire == SimHubWire.Utf8)
                {
                    byte[] utf8 = Encoding.UTF8.GetBytes(texts[field.Name]);
                    Buffer.BlockCopy(utf8, 0, buffer, offset, utf8.Length); // the rest stays zero
                }
                else
                {
                    WriteNumber(buffer, offset, field.Wire, numbers[field.Name]);
                }
                offset += field.Size;
            }
            error = null;
            return true;
        }

        /// <summary>
        /// Decodes a packet the way SimHub's parser checks it: exact length, then game
        /// signature, then telemetry signature, then the fields in order.
        /// </summary>
        public static bool TryRead(byte[] packet, uint gameSignature, uint telemetrySignature,
            out SimHubCoursePacket decoded, out string error)
        {
            decoded = null;
            if (packet == null || packet.Length != PacketSize)
            {
                error = "wrong length";
                return false;
            }
            var result = new SimHubCoursePacket { Header = ReadHeader(packet) };
            if (result.Header.GameSignature != gameSignature) { error = "wrong game signature"; return false; }
            if (result.Header.TelemetrySignature != telemetrySignature) { error = "wrong telemetry signature"; return false; }
            int offset = HeaderSize;
            foreach (var field in Fields)
            {
                if (field.Wire == SimHubWire.Utf8)
                {
                    int end = Array.IndexOf(packet, (byte)0, offset, field.Size);
                    int length = (end < 0 ? offset + field.Size : end) - offset;
                    result.Texts[field.Name] = Encoding.UTF8.GetString(packet, offset, length);
                }
                else
                {
                    result.Numbers[field.Name] = ReadNumber(packet, offset, field.Wire);
                }
                offset += field.Size;
            }
            decoded = result;
            error = null;
            return true;
        }

        private static int ComputePacketSize()
        {
            int size = HeaderSize;
            foreach (var field in Fields) size += field.Size;
            return size;
        }

        private static void WriteHeader(byte[] b, SimHubHeader h)
        {
            WriteUInt32(b, 0, h.GameSignature);
            WriteUInt32(b, 4, h.TelemetrySignature);
            WriteUInt16(b, 8, h.LayoutMajorVersion);
            WriteUInt16(b, 10, h.LayoutMinorVersion);
            WriteUInt64(b, 12, h.EmitterInstanceId);
            b[20] = h.PacketId;
            WriteUInt64(b, 21, h.PacketsCounter);
            b[29] = h.IsSessionRunning;
            b[30] = h.IsSessionPaused;
            WriteUInt64(b, 31, h.SessionId);
            b[39] = h.IsReplay;
            b[40] = h.IsUserInControl;
            b[41] = h.IsAIInControl;
            b[42] = h.IsSpectator;
            WriteUInt64(b, 43, (ulong)BitConverter.DoubleToInt64Bits(h.SessionTimeSeconds));
            WriteUInt32(b, 51, h.PhysicsDiscontinuityCounter);
        }

        private static SimHubHeader ReadHeader(byte[] b) => new SimHubHeader
        {
            GameSignature = ReadUInt32(b, 0),
            TelemetrySignature = ReadUInt32(b, 4),
            LayoutMajorVersion = (ushort)(b[8] | b[9] << 8),
            LayoutMinorVersion = (ushort)(b[10] | b[11] << 8),
            EmitterInstanceId = ReadUInt64(b, 12),
            PacketId = b[20],
            PacketsCounter = ReadUInt64(b, 21),
            IsSessionRunning = b[29],
            IsSessionPaused = b[30],
            SessionId = ReadUInt64(b, 31),
            IsReplay = b[39],
            IsUserInControl = b[40],
            IsAIInControl = b[41],
            IsSpectator = b[42],
            SessionTimeSeconds = BitConverter.Int64BitsToDouble((long)ReadUInt64(b, 43)),
            PhysicsDiscontinuityCounter = ReadUInt32(b, 51),
        };

        private static void WriteNumber(byte[] b, int offset, SimHubWire wire, double value)
        {
            switch (wire)
            {
                case SimHubWire.UInt16: WriteUInt16(b, offset, (ushort)value); break;
                case SimHubWire.Int32: WriteUInt32(b, offset, unchecked((uint)(int)value)); break;
                case SimHubWire.UInt32: WriteUInt32(b, offset, (uint)value); break;
                case SimHubWire.Float32: WriteUInt32(b, offset, new SingleBits { Single = (float)value }.Bits); break;
                case SimHubWire.Double: WriteUInt64(b, offset, (ulong)BitConverter.DoubleToInt64Bits(value)); break;
                default: throw new ArgumentOutOfRangeException(nameof(wire));
            }
        }

        private static double ReadNumber(byte[] b, int offset, SimHubWire wire)
        {
            switch (wire)
            {
                case SimHubWire.UInt16: return (ushort)(b[offset] | b[offset + 1] << 8);
                case SimHubWire.Int32: return unchecked((int)ReadUInt32(b, offset));
                case SimHubWire.UInt32: return ReadUInt32(b, offset);
                case SimHubWire.Float32: return new SingleBits { Bits = ReadUInt32(b, offset) }.Single;
                case SimHubWire.Double: return BitConverter.Int64BitsToDouble((long)ReadUInt64(b, offset));
                default: throw new ArgumentOutOfRangeException(nameof(wire));
            }
        }

        private static void WriteUInt16(byte[] b, int off, ushort v)
        {
            b[off] = (byte)v;
            b[off + 1] = (byte)(v >> 8);
        }

        private static void WriteUInt32(byte[] b, int off, uint v)
        {
            for (int i = 0; i < 4; i++) b[off + i] = (byte)(v >> (8 * i));
        }

        private static void WriteUInt64(byte[] b, int off, ulong v)
        {
            for (int i = 0; i < 8; i++) b[off + i] = (byte)(v >> (8 * i));
        }

        private static uint ReadUInt32(byte[] b, int off) =>
            (uint)(b[off] | b[off + 1] << 8 | b[off + 2] << 16 | b[off + 3] << 24);

        private static ulong ReadUInt64(byte[] b, int off) => ReadUInt32(b, off) | (ulong)ReadUInt32(b, off + 4) << 32;

        [StructLayout(LayoutKind.Explicit)]
        private struct SingleBits
        {
            [FieldOffset(0)] public float Single;
            [FieldOffset(0)] public uint Bits;
        }
    }
}
