"""Optional integration test against bytes produced by the consumer's actual DLL.

Set FORZA_TEST_FIXTURE to a Horizon324 packet file. The test re-sends those bytes
only to its own ephemeral localhost receiver; it never opens a device or game.
"""
import base64
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from forza_capture import decode


class WireTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("FORZA_TEST_FIXTURE"), "Set FORZA_TEST_FIXTURE to DLL-encoded bytes")
    def test_bounded_receiver_retains_actual_encoded_bytes_and_decodes_every_field(self):
        payload = Path(os.environ["FORZA_TEST_FIXTURE"]).read_bytes()
        expected = decode(payload)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        with tempfile.TemporaryDirectory(prefix="forza-wire-") as folder:
            output = Path(folder) / "wire.jsonl"
            creation = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
            child = subprocess.Popen([sys.executable, str(Path(__file__).with_name("forza_capture.py")),
                                      "--port", str(port), "--seconds", "2", "--output", str(output)],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, creationflags=creation)
            try:
                deadline = time.monotonic() + 5
                while not output.exists() or output.stat().st_size == 0:
                    if child.poll() is not None or time.monotonic() >= deadline:
                        self.fail("Receiver failed to start: " + str(child.communicate(timeout=2)))
                    time.sleep(.01)
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    for _ in range(4):
                        sender.sendto(payload, ("127.0.0.1", port))
                stdout, stderr = child.communicate(timeout=5)
                self.assertEqual(0, child.returncode, stdout + stderr)
                records = [json.loads(line) for line in output.read_text().splitlines()]
                packets = [row for row in records if "rawBase64" in row]
                self.assertEqual(4, len(packets))
                for row in packets:
                    self.assertEqual(payload, base64.b64decode(row["rawBase64"]))
                    self.assertEqual(expected, row["fields"])
                self.assertEqual(4, records[-1]["packets"])
                self.assertEqual(0, records[-1]["invalid"])
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
