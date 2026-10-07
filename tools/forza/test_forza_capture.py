import base64
import math
import struct
import unittest
from forza_capture import decode, packet_record


class CaptureTests(unittest.TestCase):
    def test_all_layouts_and_exact_dash_offset(self):
        for size in (232, 311, 324):
            with self.subTest(size=size):
                packet = bytearray(size)
                struct.pack_into("<i", packet, 0, 1)
                struct.pack_into("<f", packet, 40, 20.5)
                if size > 232:
                    dash = 244 if size == 324 else 232
                    struct.pack_into("<f", packet, dash + 12, 20.5)
                    struct.pack_into("<f", packet, dash + 64, 123.5)
                    struct.pack_into("<H", packet, dash + 68, 257)
                    packet[dash + 74] = 128
                    struct.pack_into("<b", packet, dash + 76, -127)
                result = decode(packet)
                self.assertEqual(20.5, result["VelocityZ"])
                if size > 232:
                    self.assertEqual(20.5, result["Speed"])
                    self.assertEqual(123.5, result["CurrentRaceTime"])
                    self.assertEqual(257, result["LapNumber"])
                    self.assertEqual(128, result["HandBrake"])
                    self.assertEqual(-127, result["Steer"])
                else:
                    self.assertNotIn("Speed", result)

    def test_independent_corners_and_integer_strip_flags(self):
        packet = bytearray(324)
        struct.pack_into("<ffff", packet, 68, .1, .2, .3, .4)
        struct.pack_into("<iiii", packet, 116, 1, 0, 0, 1)
        struct.pack_into("<ffff", packet, 148, .8, .6, .4, .2)
        result = decode(packet)
        self.assertAlmostEqual(.3, result["NormalizedSuspensionTravel.RearLeft"])
        self.assertEqual(1, result["WheelOnRumbleStrip.RearRight"])
        self.assertEqual(0, result["WheelOnRumbleStrip.FrontRight"])
        self.assertAlmostEqual(.6, result["SurfaceRumble.FrontRight"])

    def test_malformed_input_keeps_original_bytes_and_reason(self):
        for data in (b"x" * 323, b"x" * 325, b"", struct.pack("<i", 2) + bytes(320)):
            record = packet_record(data, .1)
            self.assertIn("error", record)
            self.assertEqual(data, base64.b64decode(record["rawBase64"]))

    def test_nonfinite_in_any_float_channel_is_rejected(self):
        for offset in (16, 20, 68, 148, 208, 256, 308):
            for bad in (math.nan, math.inf, -math.inf):
                packet = bytearray(324)
                struct.pack_into("<f", packet, offset, bad)
                with self.assertRaises(ValueError):
                    decode(packet)

    def test_source_and_raw_bytes_are_retained(self):
        packet = bytearray(324)
        packet[323] = 0x52
        record = packet_record(packet, 1.25)
        self.assertEqual("mod", record["source"])
        self.assertEqual(1.25, record["seconds"])
        self.assertEqual(packet, base64.b64decode(record["rawBase64"]))

    def test_native_profiles_do_not_interpret_toolkit_marker(self):
        for profile in ("fh4", "fh5", "fh6"):
            for marker in (0x52, 0x4B, 0x53, 0):
                data = bytearray(324)
                data[323] = marker
                record = packet_record(data, 0, profile)
                self.assertEqual("native-declared", record["source"])
                self.assertEqual(marker, record["trailingByte"])
                self.assertEqual(profile, record["profile"])
                self.assertEqual(data, base64.b64decode(record["rawBase64"]))

    def test_fh6_collision_block_and_puddle_flags_from_published_contract(self):
        data = bytearray(324)
        struct.pack_into("<Iff", data, 232, 0xFFFFFFFF, 4.25, 1200.5)
        struct.pack_into("<iiii", data, 132, 1, 0, 1, 0)
        struct.pack_into("<f", data, 244, -123.5)
        fields = decode(data, "fh6")
        self.assertEqual(0xFFFFFFFF, fields["CarGroup"])
        self.assertEqual(4.25, fields["SmashableVelDiff"])
        self.assertEqual(1200.5, fields["SmashableMass"])
        self.assertEqual(-123.5, fields["PositionX"])
        self.assertEqual(1, fields["WheelInPuddle.FrontLeft"])
        self.assertNotIn("WheelInPuddleDepth.FrontLeft", fields)
        # Older native profiles preserve the raw extension; no guessed meaning.
        for profile in ("fh4", "fh5", "toolkit"):
            self.assertNotIn("SmashableMass", decode(data, profile))

    def test_fh6_bad_collision_values_keep_bytes_and_error(self):
        for offset in (236, 240):
            for bad in (math.nan, math.inf, -math.inf):
                data = bytearray(324)
                struct.pack_into("<f", data, offset, bad)
                record = packet_record(data, 0, "fh6")
                self.assertIn("error", record)
                self.assertEqual(data, base64.b64decode(record["rawBase64"]))
                # The toolkit never owned this reserved block; behavior stays unchanged.
                self.assertNotIn("error", packet_record(data, 0))

    def test_native_profiles_reject_other_layouts_and_unknown_profile(self):
        for profile in ("fh4", "fh5", "fh6"):
            for size in (232, 311, 323, 325, 331):
                with self.assertRaises(ValueError):
                    decode(bytes(size), profile)
        with self.assertRaises(ValueError):
            decode(bytes(324), "guess")

    def test_fh6_refuses_float_puddle_contract_instead_of_calling_it_integer_presence(self):
        data = bytearray(324)
        struct.pack_into("<f", data, 132, 1.0)
        self.assertEqual(1.0, decode(data)["WheelInPuddleDepth.FrontLeft"])
        row = packet_record(data, 0, "fh6")
        self.assertIn("published WheelInPuddle", row["error"])
        self.assertEqual(data, base64.b64decode(row["rawBase64"]))


if __name__ == "__main__":
    unittest.main()
