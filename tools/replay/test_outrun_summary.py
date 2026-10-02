import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import command_summary as shared
import outrun_summary as outrun
import replay_case as replay


class OutrunSummaryTests(unittest.TestCase):
    def setUp(self):
        self.path = outrun.FIXTURES / outrun.ARTIFACTS["summary"][0]
        self.raw, self.digest = shared.read_summary(self.path)

    def adapt(self, value=None):
        return outrun.adapt(self.raw if value is None else value, self.digest)

    def test_verified_fixture_and_provenance_retained(self):
        result = self.adapt()
        self.assertEqual(result["producerSummary"], self.raw)
        self.assertEqual(result["sourceRevision"]["value"], "9a6b5989de456e71e89d8ac33e1c9be5f23ee645")
        self.assertEqual(result["declaredCalculationRevision"]["value"], "73747cc3585f48c64465340c8c21ee5c06093cf8")
        self.assertFalse(result["declaredCalculationRevision"]["authenticated"])
        self.assertEqual(result["shutdownZeros"]["status"], "unsupported")
        self.assertEqual(result["nativeVersion"]["status"], "unsupported")
        self.assertEqual(result["provenanceCatalog"], outrun.reviewed_artifact("catalog"))

    def test_raw_units_and_distinct_definitions_no_conversion(self):
        metrics = self.adapt()["cases"][0]["metrics"]
        self.assertEqual(metrics["constantPeakRequest"]["value"], 10000)
        self.assertEqual(metrics["constantPeakRequest"]["unit"], "pre-actuator-native-range-request")
        self.assertEqual(metrics["sampleTickSpanMilliseconds"]["value"], 799)
        self.assertEqual(metrics["fixed60CalculationStepSeconds"]["value"], .8)
        self.assertEqual(metrics["requestPairAverageRate"]["value"], 595117.6470588235)
        self.assertEqual(metrics["periodicMaximumAmplitudeParameter"]["unit"], "pre-clamp-amplitude-parameter")
        self.assertTrue(set(metrics).isdisjoint(shared.METRICS))
        self.assertTrue(all(item["definition"].startswith("outrun.") for item in metrics.values()))

    def test_mismatched_schemas_metrics_units_and_claims_rejected(self):
        for field, value in (("schema", "dbce.woden.ffb-regression-summary@1"), ("version", True),
                             ("version", 2), ("domain", "normalized-output"), ("physical_output", True),
                             ("production_recalculation_verified", False), ("declared_revision_authenticated", True),
                             ("recording_sha256", "0" * 64), ("frame_count", 49)):
            changed = copy.deepcopy(self.raw)
            changed[field] = value
            with self.subTest(field=field), self.assertRaises(replay.InvalidCase):
                self.adapt(changed)
        for field, value in (("units", "normalized-command"), ("peak_absolute_request", 1),
                             ("request_cap_fraction", 1), ("nonzero_requests", True),
                             ("maximum_request_pair_average_delta_units_per_sample_tick_second", float("nan"))):
            changed = copy.deepcopy(self.raw)
            changed["constant"][field] = value
            with self.subTest(field=field), self.assertRaises(replay.InvalidCase):
                self.adapt(changed)

    def test_unsupported_claims_and_periodic_merging_rejected(self):
        for key in self.raw["unsupported"]:
            changed = copy.deepcopy(self.raw)
            changed["unsupported"][key] = 0
            with self.subTest(key=key), self.assertRaises(replay.InvalidCase):
                self.adapt(changed)
        changed = copy.deepcopy(self.raw)
        changed["periodic"]["units"] = changed["constant"]["units"]
        with self.assertRaises(replay.InvalidCase):
            self.adapt(changed)
        with self.assertRaises(replay.InvalidCase):
            outrun.adapt(self.raw, "0" * 64)

    def test_provenance_artifact_tampering_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, _ in outrun.ARTIFACTS.values():
                (root / name).write_bytes((outrun.FIXTURES / name).read_bytes())
            for kind in ("summary", "definitions", "catalog"):
                target = root / outrun.ARTIFACTS[kind][0]
                original = target.read_bytes()
                target.write_bytes(original + b" ")
                with patch.object(outrun, "FIXTURES", root), self.subTest(kind=kind), self.assertRaises(replay.InvalidCase):
                    self.adapt()
                target.write_bytes(original)

    def test_combined_report_all_cross_game_pairs_stay_unsupported(self):
        woden, digest = shared.read_summary(outrun.FIXTURES / "woden-ffb-reference-v1.json")
        result = shared.report(shared.woden(woden, digest), self.adapt())
        self.assertFalse(result["physicalAcceptance"])
        self.assertFalse(result["crossGameFeelEquivalence"])
        self.assertEqual([item["status"] for item in result["inputs"]], ["supported", "supported", "unsupported"])
        for pair in result["crossGameComparisons"]:
            self.assertTrue(all(item["status"] == "unsupported" and "delta" not in item for item in pair["metrics"].values()))
        # Renaming an OutRun metric cannot make its units/definition compatible.
        a = result["inputs"][0]["cases"][0]["metrics"]
        b = {"peak": result["inputs"][1]["cases"][0]["metrics"]["constantPeakRequest"]}
        self.assertEqual(shared.compare_metrics(a, b)["peak"]["status"], "unsupported")

    def test_cli_exact_fixture_and_unreviewed_file_exit_contract(self):
        script = str(Path(shared.__file__))
        command = [sys.executable, script, str(outrun.FIXTURES / "woden-ffb-reference-v1.json"), "--outrun-summary", str(self.path)]
        result = subprocess.run(command, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(replay.decode(result.stdout)["inputs"][1], self.adapt())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "wrong.json"
            path.write_bytes(replay.encode(self.raw))
            result = subprocess.run(command[:-1] + [str(path)], capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    unittest.main()
