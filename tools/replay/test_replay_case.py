import copy
import json
from pathlib import Path
import tempfile
import unittest

import replay_case as replay


class ReplayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "source.bin").write_bytes(b"ROM-free synthetic input")
        source = {"path": "source.bin", "sha256": replay.sha256(self.root / "source.bin"), "format": "fixture@1"}
        self.case = {"schema": "dbce.wheel.replay-case", "version": 1,
            "caseId": "fixture-drive", "game": "fixture", "adapter": "fixture@1",
            "capability": "signal-reprocess", "clock": {"domain": "emulated", "ticksPerSecond": 60},
            "source": source, "initialState": None, "artifacts": {},
            "provenance": {"sourceRevision": "test-source", "dirty": False, "executableSha256": "1"*64}}
        self.manifest = self.root / "case.json"
        self.save_case()
        self.header = {"kind": "header", "schema": "dbce.wheel.force-observation", "version": 1,
            "caseId": "fixture-drive", "caseSha256": replay.sha256(self.manifest),
            "clock": copy.deepcopy(self.case["clock"]), "model": "fixture@1", "configSha256": "2"*64,
            "profileSha256": "3"*64, "output": "observe", "physicalOutput": False}

    def save_case(self):
        self.manifest.write_bytes(replay.encode(self.case))

    def row(self, sequence=0, **changes):
        value = dict(kind="force", sequence=sequence, tick=sequence, frame=sequence,
            effect="steering", family="constant", operation="set", magnitude=0.25,
            frequencyHz=0.0, durationMs=None)
        value.update(changes)
        return value

    def write(self, name, rows=None, header=None):
        path = self.root / name
        with replay.ObservationSink(path, header or self.header) as sink:
            for row in rows if rows is not None else [self.row()]:
                sink.emit(row)
        return path

    def test_case_identity_only_and_tamper(self):
        result = replay.validate_case(self.manifest)
        self.assertEqual(result["capability"], "signal-reprocess")
        self.assertIn("identity only", result["scope"])
        (self.root / "source.bin").write_bytes(b"changed")
        with self.assertRaisesRegex(replay.InvalidCase, "hash mismatch"):
            replay.validate_case(self.manifest)

    def test_game_replay_requires_initial_state(self):
        self.case["capability"] = "game-input-replay"
        self.save_case()
        with self.assertRaisesRegex(replay.InvalidCase, "initial state"):
            replay.validate_case(self.manifest)
        self.case["initialState"] = self.case["source"]
        self.save_case()
        self.assertTrue(replay.validate_case(self.manifest)["valid"])

    def test_unsafe_paths(self):
        for path in ("../secret", "/source.bin", "C:/secret", "x\\y", "./source.bin", "x//y"):
            with self.subTest(path=path):
                self.case["source"]["path"] = path
                self.save_case()
                with self.assertRaises(replay.InvalidCase):
                    replay.validate_case(self.manifest)

    def test_symlink_escape(self):
        with tempfile.TemporaryDirectory() as other:
            external = Path(other) / "outside.bin"
            external.write_bytes(b"outside")
            try:
                (self.root / "link.bin").symlink_to(external)
            except OSError:
                self.skipTest("symlink privilege unavailable")
            self.case["source"] = {"path": "link.bin", "format": "fixture@1", "sha256": replay.sha256(external)}
            self.save_case()
            with self.assertRaisesRegex(replay.InvalidCase, "escapes"):
                replay.validate_case(self.manifest)

    def test_duplicate_keys_and_unknown_fields(self):
        with self.assertRaisesRegex(replay.InvalidCase, "duplicate"):
            replay.decode(b'{"version":1,"version":1}')
        self.case["command"] = "must never run"
        self.save_case()
        with self.assertRaises(replay.InvalidCase):
            replay.validate_case(self.manifest)

    def test_sink_snapshot_and_same_tick_order(self):
        row = self.row()
        with replay.ObservationSink(self.root / "a.jsonl", self.header) as sink:
            sink.emit(row)
            row["magnitude"] = 0.99
            sink.emit(self.row(1, tick=0))
        rows = list(replay.read_observations(self.root / "a.jsonl"))
        self.assertEqual(rows[1]["magnitude"], 0.25)
        self.assertEqual(rows[-1]["count"], 2)

    def test_refuses_hardware_header_before_creating_file(self):
        for mutation in ({"output": "hardware"}, {"physicalOutput": True}, {"physicalOutput": 0}):
            header = self.header | mutation
            with self.assertRaises(replay.InvalidCase):
                replay.ObservationSink(self.root / "denied", header)
            self.assertFalse((self.root / "denied").exists())

    def test_exclusive_creation(self):
        path = self.write("a")
        before = path.read_bytes()
        with self.assertRaises(FileExistsError):
            replay.ObservationSink(path, self.header)
        self.assertEqual(before, path.read_bytes())

    def test_invalid_request_leaves_incomplete_file(self):
        for i, mutation in enumerate(({"magnitude": float("nan")}, {"magnitude": 2}, {"tick": True},
                {"sequence": 2}, {"durationMs": 0}, {"frequencyHz": float("inf")},
                {"family": "unknown"}, {"operation": "stop", "magnitude": .1})):
            path = self.root / f"invalid-{i}"
            with self.assertRaises(replay.InvalidCase):
                with replay.ObservationSink(path, self.header) as sink:
                    sink.emit(self.row(**mutation))
            with self.assertRaises(replay.InvalidCase):
                list(replay.read_observations(path))

    def test_caller_failure_does_not_write_complete_footer(self):
        path = self.root / "failed"
        with self.assertRaises(RuntimeError):
            with replay.ObservationSink(path, self.header) as sink:
                sink.emit(self.row())
                raise RuntimeError("model failed")
        with self.assertRaisesRegex(replay.InvalidCase, "missing"):
            list(replay.read_observations(path))

    def test_reverse_time_fails(self):
        with self.assertRaisesRegex(replay.InvalidCase, "reversed"):
            self.write("reverse", [self.row(tick=5), self.row(1, tick=4)])

    def test_torn_bad_footer_and_trailing_data(self):
        data = self.write("good").read_bytes()
        for i, bad in enumerate((data[:-1], data + b"{}\n", data.replace(b'"count":1', b'"count":2'),
                                 data.replace(b'"complete":true', b'"complete":false'))):
            path = self.root / f"broken-{i}"
            path.write_bytes(bad)
            with self.assertRaises(replay.InvalidCase):
                list(replay.read_observations(path))

    def test_line_bound(self):
        path = self.root / "large"
        path.write_bytes(b" " * (replay.MAX_LINE + 1) + b"\n")
        with self.assertRaisesRegex(replay.InvalidCase, "bound"):
            list(replay.read_observations(path))

    def test_compare_changed_tune_reports_error_and_identities(self):
        a = self.write("a")
        header = self.header | {"model": "fixture@2", "configSha256": "4"*64}
        b = self.write("b", [self.row(magnitude=.5)], header)
        result = replay.compare(a, b)
        self.assertFalse(result["equal"])
        self.assertEqual(result["maxMagnitudeDelta"], .25)
        self.assertFalse(result["physicalAcceptance"])
        self.assertEqual(result["candidate"]["model"], "fixture@2")
        self.assertTrue(replay.compare(a, a)["equal"])
        self.assertTrue(replay.compare(a, b, .3)["equal"])

    def test_compare_requires_same_case_bytes(self):
        a = self.write("a")
        b = self.write("b", header=self.header | {"caseSha256": "5"*64})
        with self.assertRaisesRegex(replay.InvalidCase, "different case"):
            replay.compare(a, b)

    def test_compare_events_counts_and_full_validation(self):
        a = self.write("a")
        b = self.write("b", [self.row(operation="start", durationMs=140)])
        self.assertFalse(replay.compare(a, b)["equal"])
        self.assertEqual(replay.compare(a, b)["comparedRequests"], 0)
        c = self.write("c", [self.row(), self.row(1)])
        self.assertFalse(replay.compare(a, c)["equal"])
        c.write_bytes(c.read_bytes() + b"{}\n")
        with self.assertRaises(replay.InvalidCase):
            replay.compare(a, c)

    def test_cli_contract(self):
        self.assertEqual(replay.main(["validate", str(self.manifest)]), 0)
        self.assertEqual(replay.main(["validate", str(self.root / "missing")]), 2)


if __name__ == "__main__":
    unittest.main()
