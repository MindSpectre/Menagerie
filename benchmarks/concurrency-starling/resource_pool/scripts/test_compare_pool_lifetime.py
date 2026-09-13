#!/usr/bin/env python3

import contextlib
import importlib.util
import io
import json
import pathlib
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).with_name("compare_pool_lifetime.py")
SPEC = importlib.util.spec_from_file_location("compare_pool_lifetime", SCRIPT)
compare = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(compare)


def aggregate(run_name, aggregate_name, items_per_second, p99_us):
    return {
        "name": f"{run_name}_{aggregate_name}",
        "run_name": run_name,
        "run_type": "aggregate",
        "aggregate_name": aggregate_name,
        "items_per_second": items_per_second,
        "p50_us": p99_us / 2.0,
        "p95_us": p99_us * 0.95,
        "p99_us": p99_us,
    }


def policy_records(subject, protected_items, external_items, protected_p99, external_p99,
                   items_stddev=1.0, p99_stddev=0.01):
    records = []
    for policy, items, p99 in (
        ("Protected", protected_items, protected_p99),
        ("External", external_items, external_p99),
    ):
        run_name = f"BM_Lifetime_{policy}_{subject}"
        records.extend(
            (
                aggregate(run_name, "mean", items, p99),
                aggregate(run_name, "median", items, p99),
                aggregate(run_name, "stddev", items_stddev, p99_stddev),
            )
        )
    return records


class LifetimeComparisonTests(unittest.TestCase):
    def test_exact_five_percent_significant_throughput_gap_keeps_external(self):
        data = {"benchmarks": policy_records("Try", 95.0, 100.0, 1.0, 1.0)}

        result = compare.compare_datasets((("floating", data),))

        self.assertEqual("KEEP_EXTERNAL", result.verdict)
        self.assertEqual(1, len(result.qualifying))
        self.assertAlmostEqual(5.0, result.qualifying[0].throughput_regression_pct)

    def test_confidently_small_gap_removes_external(self):
        data = {
            "benchmarks": policy_records(
                "Try", 98.0, 100.0, 1.0, 1.0, items_stddev=0.0, p99_stddev=0.0
            )
        }

        result = compare.compare_datasets((("floating", data),))

        self.assertEqual("REMOVE_EXTERNAL", result.verdict)
        self.assertEqual([], result.qualifying)

    def test_above_five_percent_with_difference_equal_to_two_sigma_is_inconclusive(self):
        # 10 items/s difference, with 2*hypot(3, 4) == 10 exactly.
        records = policy_records(
            "Try", 90.0, 100.0, 1.0, 1.0, items_stddev=3.0, p99_stddev=0.01
        )
        external_stddev = next(
            record
            for record in records
            if "_External_" in record["run_name"] and record["aggregate_name"] == "stddev"
        )
        external_stddev["items_per_second"] = 4.0
        data = {"benchmarks": records}

        result = compare.compare_datasets((("floating", data),))

        self.assertEqual("INCONCLUSIVE", result.verdict)
        self.assertEqual(1, len(result.inconclusive))

    def test_sub_five_point_estimate_with_wide_uncertainty_is_inconclusive(self):
        data = {
            "benchmarks": policy_records(
                "Try", 98.0, 100.0, 1.0, 1.0, items_stddev=2.0, p99_stddev=0.0
            )
        }

        result = compare.compare_datasets((("floating", data),))

        self.assertEqual("INCONCLUSIVE", result.verdict)
        self.assertGreaterEqual(result.inconclusive[0].throughput_upper_bound_pct, 5.0)

    def test_uncertainty_upper_bound_equal_to_five_is_inconclusive(self):
        records = policy_records(
            "Try", 98.0, 100.0, 1.0, 1.0, items_stddev=1.5, p99_stddev=0.0
        )
        external_stddev = next(
            record
            for record in records
            if "_External_" in record["run_name"] and record["aggregate_name"] == "stddev"
        )
        external_stddev["items_per_second"] = 0.0

        result = compare.compare_datasets((("floating", {"benchmarks": records}),))

        self.assertEqual("INCONCLUSIVE", result.verdict)
        self.assertAlmostEqual(5.0, result.inconclusive[0].throughput_upper_bound_pct)

    def test_significant_five_percent_latency_gap_keeps_external(self):
        data = {
            "benchmarks": policy_records(
                "SyncHandoff/8", 100.0, 100.0, 1.05, 1.0, items_stddev=1.0, p99_stddev=0.01
            )
        }

        result = compare.compare_datasets((("pinned", data),))

        self.assertEqual("KEEP_EXTERNAL", result.verdict)
        self.assertTrue(result.qualifying[0].latency_qualifies)

    def test_missing_policy_pair_is_rejected(self):
        data = {
            "benchmarks": [
                aggregate("BM_Lifetime_Protected_Try", kind, 100.0, 1.0)
                for kind in ("mean", "median", "stddev")
            ]
        }

        with self.assertRaisesRegex(compare.ComparisonError, "unpaired"):
            compare.compare_datasets((("floating", data),))

    def test_missing_required_metric_is_rejected(self):
        records = policy_records("Try", 95.0, 100.0, 1.0, 1.0)
        del records[-1]["p99_us"]

        with self.assertRaisesRegex(compare.ComparisonError, "p99_us"):
            compare.compare_datasets((("floating", {"benchmarks": records}),))

    def test_zero_external_denominator_is_rejected(self):
        data = {"benchmarks": policy_records("Try", 0.0, 0.0, 1.0, 1.0)}

        with self.assertRaisesRegex(compare.ComparisonError, "must be positive"):
            compare.compare_datasets((("floating", data),))

    def test_nonpositive_means_and_medians_are_rejected(self):
        cases = (
            ("mean", "items_per_second", -1.0),
            ("median", "items_per_second", 0.0),
            ("mean", "p50_us", -1.0),
            ("median", "p99_us", -0.0),
        )
        for aggregate_name, metric, invalid in cases:
            with self.subTest(aggregate_name=aggregate_name, metric=metric, invalid=invalid):
                records = policy_records("Try", 98.0, 100.0, 1.0, 1.0)
                record = next(
                    candidate
                    for candidate in records
                    if candidate["aggregate_name"] == aggregate_name
                )
                record[metric] = invalid

                with self.assertRaisesRegex(compare.ComparisonError, "must be positive"):
                    compare.compare_datasets((("floating", {"benchmarks": records}),))

    def test_negative_standard_deviations_are_rejected_but_zero_is_valid(self):
        for metric in ("items_per_second", "p99_us"):
            with self.subTest(metric=metric):
                records = policy_records(
                    "Try", 98.0, 100.0, 1.0, 1.0, items_stddev=0.0, p99_stddev=0.0
                )
                record = next(
                    candidate
                    for candidate in records
                    if candidate["aggregate_name"] == "stddev"
                )
                record[metric] = -0.01

                with self.assertRaisesRegex(compare.ComparisonError, "must be non-negative"):
                    compare.compare_datasets((("floating", {"benchmarks": records}),))

        valid = {
            "benchmarks": policy_records(
                "Try", 98.0, 100.0, 1.0, 1.0, items_stddev=0.0, p99_stddev=0.0
            )
        }
        self.assertEqual(
            "REMOVE_EXTERNAL", compare.compare_datasets((("floating", valid),)).verdict
        )

    def test_cli_returns_nonzero_for_invalid_input(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "invalid.json"
            path.write_text(json.dumps({"benchmarks": []}), encoding="utf-8")

            errors = io.StringIO()
            with contextlib.redirect_stderr(errors):
                self.assertNotEqual(0, compare.main([str(path)]))
            self.assertIn("no lifetime-policy aggregate records", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
