import base64
import math
import struct
import unittest
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from forza_capture import decode, packet_record


class CaptureTests(unittest.TestCase):
    def test_supervised_capture_binds_actual_port_and_closes_with_footer(self):
        with tempfile.TemporaryDirectory() as folder:
            output, stop = Path(folder) / "wire.jsonl", Path(folder) / "stop"
            process = subprocess.Popen([sys.executable, str(Path(__file__).with_name("forza_capture.py")),
                                        "--port", "0", "--seconds", "10", "--output", str(output),
                                        "--stop-file", str(stop)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 5
                header = None
                while time.monotonic() < deadline:
                    if output.exists():
                        text = output.read_text(encoding="utf-8")
                        if "\n" in text:
                            header = json.loads(text.splitlines()[0])
                            break
                    if process.poll() is not None:
                        self.fail("Receiver exited before binding")
                    time.sleep(.01)
                self.assertIsNotNone(header)
                self.assertGreater(header["port"], 0)
                self.assertEqual(0, header["requestedPort"])
                packet = bytearray(324)
                struct.pack_into("<iIf", packet, 0, 1, 1234, 9000)
                packet[323] = 0x52
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    sender.sendto(packet, ("127.0.0.1", header["port"]))
                # The file's data rows are buffered. Receive timeout bounds this
                # interval; ending after it tests the normal supervisor stop.
                time.sleep(.1)
                stop.write_text("owned stop", encoding="utf-8")
                stdout, stderr = process.communicate(timeout=5)
                self.assertEqual(0, process.returncode, stderr)
                rows = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines()]
                self.assertEqual("supervisor-stop", rows[-1]["stopReason"])
                self.assertEqual(1, rows[-1]["packets"])
                self.assertEqual(bytes(packet), base64.b64decode(rows[1]["rawBase64"]))
                self.assertEqual(1234, rows[1]["fields"]["TimestampMs"])
                self.assertEqual(0, json.loads(stdout)["invalid"])
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as again:
                    again.bind(("127.0.0.1", header["port"]))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)

    def test_stale_or_self_stop_file_refuses_without_capture(self):
        with tempfile.TemporaryDirectory() as folder:
            output, stop = Path(folder) / "wire.jsonl", Path(folder) / "stop"
            stop.write_text("old", encoding="utf-8")
            for flag in (stop, output):
                result = subprocess.run([sys.executable, str(Path(__file__).with_name("forza_capture.py")),
                                         "--port", "0", "--seconds", "1", "--output", str(output),
                                         "--stop-file", str(flag)], capture_output=True, text=True, timeout=5)
                self.assertNotEqual(0, result.returncode)
                self.assertIn("fresh path distinct", result.stderr)
                self.assertFalse(output.exists())

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
