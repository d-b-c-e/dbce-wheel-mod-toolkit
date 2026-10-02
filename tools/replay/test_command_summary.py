import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import command_summary as summary
import replay_case as replay

FIXTURE = Path(__file__).parent / "fixtures/woden-ffb-reference-v1.json"
FIXTURE_SHA = "41bf64ca0fbcded234c27f65a075ef1a11fc3a37707dd606fd0d20cca9076d57"


class CommandSummaryTests(unittest.TestCase):
    def setUp(self):
        self.raw, self.digest = summary.read_summary(FIXTURE)

    def adapt(self, raw=None):
        return summary.woden(self.raw if raw is None else raw, self.digest)

    def test_exact_authorized_fixture_and_deterministic_report(self):
        self.assertEqual(self.digest, FIXTURE_SHA)
        report = summary.report(self.adapt())
        self.assertEqual(replay.encode(report), replay.encode(summary.report(self.adapt())))
        self.assertEqual(len(report["inputs"][0]["cases"]), 20)
        self.assertFalse(report["physicalAcceptance"])
        self.assertFalse(report["crossGameFeelEquivalence"])
        self.assertEqual(report["inputs"][0]["provenance"]["SharedFfbSha256"], self.raw["SharedFfbSha256"])
        self.assertEqual(report["inputs"][0]["cases"][0]["producerRow"], self.raw["Rows"][0])
        self.assertEqual(report["inputs"][0]["nativeVersion"]["status"], "unsupported")

    def test_all_cross_game_pairs_unsupported_without_fabricated_values(self):
        report = summary.report(self.adapt())
        self.assertEqual(len(report["crossGameComparisons"]), 3)
        for pair in report["crossGameComparisons"]:
            self.assertEqual(set(pair["metrics"]), set(summary.METRICS))
            for metric in pair["metrics"].values():
                self.assertEqual(metric["status"], "unsupported")
                self.assertNotIn("delta", metric)
                self.assertNotIn("value", metric)

    def test_verified_sample_quantities_and_shutdown_separate(self):
        for source, case in zip(self.raw["Rows"], self.adapt()["cases"]):
            metrics = case["metrics"]
            self.assertEqual(metrics["capOccupancy"]["value"], source["AtOutputCapFrames"] / 300)
            self.assertEqual(metrics["duty"]["value"], source["NonzeroCommandFrames"] / 300)
            self.assertEqual(metrics["slew"]["value"], source["MaximumObservedSlew"])
            self.assertEqual(metrics["shutdownZeros"]["value"], 1)
            self.assertEqual(metrics["duration"]["value"], 6)

    def test_compatible_scalar_deltas(self):
        a = self.adapt()["cases"][0]["metrics"]
        b = copy.deepcopy(a)
        b["peak"]["value"] = .2
        result = summary.compare_metrics(a, b)
        self.assertEqual(result["peak"]["delta"], .2)
        self.assertTrue(all(m["status"] == "supported" for m in result.values()))

    def test_units_effect_semantics_and_missing_metrics_not_equated(self):
        a = self.adapt()["cases"][0]["metrics"]
        for key in summary.METRICS:
            for field, value in (("unit", "Nm"), ("definition", "native-periodic.effect-time@1"),
                                 ("definition", None), ("status", "unsupported")):
                b = copy.deepcopy(a)
                b[key][field] = value
                self.assertEqual(summary.compare_metrics(a, b)[key]["status"], "unsupported")
        self.assertTrue(all(m["status"] == "unsupported" for m in summary.compare_metrics(a, {}).values()))

    def test_invalid_models_units_boundaries_and_options_rejected(self):
        for field, value in (("ModelVersion", True), ("ModelVersion", 4), ("Units", "Nm"),
                             ("HardwareOpened", 0), ("PhysicalTorqueMeasured", True),
                             ("SampleSeconds", float("nan")), ("SharedFfbSha256", "bad"),
                             ("FixedOptionsExceptStrength", {})):
            raw = copy.deepcopy(self.raw)
            raw[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(replay.InvalidCase):
                self.adapt(raw)

    def test_bad_rows_rejected(self):
        for field, value in (("Peak", float("inf")), ("Peak", True), ("Rms", .1),
                             ("DurationSeconds", 5), ("PeakPercent", 100),
                             ("AtOutputCapFrames", 301), ("NonzeroCommandDuty", .5),
                             ("AtOutputCapSeconds", 1), ("CommandsSha256", "bad"),
                             ("ShutdownZeroCalls", True)):
            raw = copy.deepcopy(self.raw)
            raw["Rows"][0][field] = value
            with self.subTest(field=field), self.assertRaises(replay.InvalidCase):
                self.adapt(raw)
        raw = copy.deepcopy(self.raw)
        raw["Rows"][1] = copy.deepcopy(raw["Rows"][0])
        with self.assertRaises(replay.InvalidCase):
            self.adapt(raw)
        raw = copy.deepcopy(self.raw)
        raw["FixedOptionsExceptStrength"]["Invert"] = 0
        with self.assertRaises(replay.InvalidCase):
            self.adapt(raw)

    def test_reused_bounded_decoder_rejects_duplicate_and_nonfinite_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            for data in (b'{"x":1,"x":2}', b'{"x":NaN}', b" " * (summary.MAX_SUMMARY + 1)):
                path.write_bytes(data)
                with self.assertRaises(replay.InvalidCase):
                    summary.read_summary(path)

    def test_cli_success_and_invalid_artifact_exit_contract(self):
        command = [sys.executable, str(Path(summary.__file__)), str(FIXTURE)]
        result = subprocess.run(command, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(replay.decode(result.stdout), summary.report(self.adapt()))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_bytes(b"{}")
            result = subprocess.run(command[:-1] + [str(path)], capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    unittest.main()
