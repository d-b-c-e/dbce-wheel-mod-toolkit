"""Adapter for one reviewed synthetic OutRun raw-request artifact only."""
from pathlib import Path

from command_summary import metric, read_summary
import replay_case as replay

FIXTURES = Path(__file__).parent / "fixtures"
ARTIFACTS = {
    "summary": ("outrun-command-summary-v1.json", "74c09edd120c113adf25d6c3287c5c734a7aeca7ed8968747488e4d1544dd339"),
    "definitions": ("outrun-command-metrics-v1.json", "0d816acab7ba9f2f35933b20fd8d8d57637d4025bd71319415a0428b8a7c21b6"),
    "catalog": ("outrun-command-evidence-v1.json", "2340a5930db8d242ca9eeefdc50c8296ffb70bc506d0c9f045866ea2a5cf48f9"),
}


def reviewed_artifact(kind):
    filename, expected_hash = ARTIFACTS[kind]
    value, actual_hash = read_summary(FIXTURES / filename)
    replay.require(actual_hash == expected_hash, "reviewed OutRun fixture/provenance artifact changed")
    return value


def adapt(summary, artifact_sha256):
    # This is deliberately a single approved-case adapter. No generic conversion
    # or extension to private/unknown recordings follows from its schema label.
    replay.require(artifact_sha256 == ARTIFACTS["summary"][1], "unreviewed OutRun artifact bytes")
    expected = reviewed_artifact("summary")
    replay.require(replay.encode(summary) == replay.encode(expected),
                   "unreviewed OutRun schema, metrics or evidence boundary")
    definitions, catalog = reviewed_artifact("definitions"), reviewed_artifact("catalog")
    constant, periodic = summary["constant"], summary["periodic"]
    units = constant["units"]
    metrics = {
        "constantPeakRequest": metric(constant["peak_absolute_request"], units,
                                      "outrun.pre-strength.constant.absolute-peak@1"),
        "constantMeanAbsoluteRequest": metric(constant["mean_absolute_request"], units,
                                              "outrun.pre-strength.constant.request-count.mean-absolute@1"),
        "requestCapFraction": metric(constant["request_cap_fraction"], "fraction",
                                     "outrun.pre-strength.constant.request-count.abs-eq-10000@1"),
        "nonzeroRequestFraction": metric(constant["nonzero_request_fraction"], "fraction",
                                         "outrun.pre-strength.constant.request-count.abs-ne-0@1"),
        "sampleTickSpanMilliseconds": metric(summary["sample_tick_span_ms"], "milliseconds",
                                             "outrun.monotonic-ms.last-minus-first.no-terminal-hold@1"),
        "fixed60CalculationStepSeconds": metric(summary["fixed60_calculation_step_sum_seconds"], "seconds",
                                                "outrun.nominal-calculation-call-count.div-60@1"),
        "requestPairAverageRate": metric(constant["maximum_request_pair_average_delta_units_per_sample_tick_second"],
                                         units + "/sample-tick-second",
                                         "outrun.constant.segmented.positive-ms-tick-pair.max-average-rate@1"),
        "periodicMaximumAmplitudeParameter": metric(periodic["maximum_amplitude_parameter"], "pre-clamp-amplitude-parameter",
                                                   "outrun.periodic.pre-clamp.parameter.max@1"),
        "periodicMaximumRequestedFrequencyHz": metric(periodic["maximum_requested_frequency_hz"], "requested-Hz",
                                                     "outrun.periodic.requested-frequency.max@1"),
    }
    return {"game": "OutRun2006", "status": "supported", "adapter": "outrun-reviewed-synthetic-requests@1",
            "artifactSha256": artifact_sha256, "cases": [{"case": "legacy-selftest-48-frames", "metrics": metrics}],
            "producerSummary": summary, "metricDefinitions": definitions, "provenanceCatalog": catalog,
            "sourceRevision": {"status": "catalog-reported", "value": catalog["adapterCheckoutCommit"],
                               "scope": "reviewed adapter checkout; not embedded calculation identity authentication"},
            "declaredCalculationRevision": {"value": summary["declared_calculation_revision"], "authenticated": False},
            "nativeVersion": {"status": "unsupported", "reason": "memory-only sinks; no native output loaded/version observed"},
            "shutdownZeros": {"status": "unsupported", "reason": "recorder completion is not output shutdown; no zero/release proof"},
            "hashScope": "exact reviewed summary/definitions/catalog bytes; catalog's remaining artifact/source claims retained"}
