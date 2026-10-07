"""Synthetic protocol checks on a private loopback socket, never a game or SimHub."""
import base64
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

CAPTURE = Path(__file__).with_name("forza_capture.py")
HIDDEN = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


class NativeWireTests(unittest.TestCase):
    def receive(self, profile, payloads):
        with tempfile.TemporaryDirectory(prefix="forza-native-fixture-") as folder:
            root = Path(folder)
            output = root / "wire.jsonl"
            identity = root / "synthetic-identity.bin"
            identity.write_bytes(b"protocol test, not a game executable")
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
            child = subprocess.Popen([
                sys.executable, str(CAPTURE), "--port", str(port), "--seconds", ".6",
                "--output", str(output), "--profile", profile,
                "--source-exe", str(identity), "--source-build", "synthetic-test-only"
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, creationflags=HIDDEN)
            try:
                deadline = time.monotonic() + 5
                while not output.exists() or output.stat().st_size == 0:
                    if child.poll() is not None or time.monotonic() >= deadline:
                        self.fail("Receiver did not become ready: " + str(child.communicate(timeout=2)))
                    time.sleep(.01)
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    sender.bind(("127.0.0.1", 0))
                    sender_port = sender.getsockname()[1]
                    for data in payloads:
                        sender.sendto(data, ("127.0.0.1", port))
                child.communicate(timeout=5)
                rows = [json.loads(line) for line in output.read_text().splitlines()]
                self.assertEqual(hashlib.sha256(identity.read_bytes()).hexdigest().upper(),
                                 rows[0]["producer"]["executableSha256"])
                self.assertEqual("synthetic-test-only", rows[0]["producer"]["declaredBuild"])
                self.assertEqual(profile, rows[0]["producer"]["profile"])
                for row, data in zip(rows[1:-1], payloads):
                    self.assertEqual(data, base64.b64decode(row["rawBase64"]))
                    self.assertEqual({"address": "127.0.0.1", "port": sender_port}, row["sender"])
                self.assertEqual(len(payloads), rows[-1]["packets"])
                return child.returncode, rows
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait(timeout=5)

    def test_all_native_profiles_capture_throttle_brake_and_signed_steering(self):
        data = bytearray(324)
        struct.pack_into("<iI", data, 0, 1, 0xFFFFFFF0)
        struct.pack_into("<f", data, 256, 20.5)
        data[315], data[316], data[319] = 221, 35, 3
        struct.pack_into("<b", data, 320, -76)
        data[323] = 0x52  # Cannot establish that a DBCE mod sent this.
        for profile in ("fh4", "fh5", "fh6"):
            with self.subTest(profile=profile):
                code, rows = self.receive(profile, [data])
                self.assertEqual(0, code)
                self.assertEqual("native-declared", rows[1]["source"])
                fields = rows[1]["fields"]
                self.assertEqual((221, 35, -76, 3),
                                 (fields["Accel"], fields["Brake"], fields["Steer"], fields["Gear"]))
                self.assertEqual(0xFFFFFFF0, fields["TimestampMs"])
                self.assertEqual(1, rows[-1]["driving"])

    def test_invalid_native_payload_retained_and_capture_fails(self):
        code, rows = self.receive("fh6", [b"invalid-datagram"])
        self.assertEqual(1, code)
        self.assertIn("error", rows[1])
        self.assertEqual(1, rows[-1]["invalid"])
        self.assertEqual(0, rows[-1]["driving"])

    def test_fh6_reserved_port_refused_without_file_creation(self):
        with tempfile.TemporaryDirectory(prefix="forza-reserved-fixture-") as folder:
            output = Path(folder) / "wire.jsonl"
            for port in (5200, 5300):
                child = subprocess.run([sys.executable, str(CAPTURE), "--profile", "fh6",
                                        "--port", str(port), "--output", str(output)],
                                       capture_output=True, text=True, timeout=5, creationflags=HIDDEN)
                self.assertEqual(2, child.returncode)
                self.assertIn("reserves ports", child.stderr)
                self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
