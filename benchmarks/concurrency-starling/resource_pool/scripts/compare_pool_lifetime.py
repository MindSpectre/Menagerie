#!/usr/bin/env python3
"""Compare paired Protected/External lifetime-policy benchmark results."""

from __future__ import annotations

import json
import math
import pathlib
import sys
from typing import Any, NamedTuple, Sequence


REQUIRED_AGGREGATES = ("mean", "median", "stddev")
REQUIRED_METRICS = ("items_per_second", "p50_us", "p95_us", "p99_us")
POLICY_MARKERS = ("_Protected_", "_External_")


class ComparisonError(ValueError):
    """The benchmark inputs are incomplete or cannot be compared."""


class PairResult(NamedTuple):
    environment: str
    subject: str
    protected_mean_items: float
    external_mean_items: float
    protected_items: float
    external_items: float
    protected_items_stddev: float
    external_items_stddev: float
    throughput_regression_pct: float
    throughput_noise: float
    throughput_upper_bound_pct: float
    throughput_qualifies: bool
    protected_mean_p99: float
    external_mean_p99: float
    protected_p99: float
    external_p99: float
    protected_p99_stddev: float
    external_p99_stddev: float
    latency_regression_pct: float
    latency_noise: float
    latency_upper_bound_pct: float
    latency_qualifies: bool


class ComparisonResult(NamedTuple):
    verdict: str
    pairs: list[PairResult]
    qualifying: list[PairResult]
    inconclusive: list[PairResult]


def _number(
    record: dict[str, Any], metric: str, description: str, *, allow_zero: bool = False
) -> float:
    if metric not in record:
        raise ComparisonError(f"{description} is missing required metric {metric}")
    value = record[metric]
    if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(float(value)):
        raise ComparisonError(f"{description} has invalid {metric}: {value!r}")
    number = float(value)
    if allow_zero:
        if number < 0.0:
            raise ComparisonError(f"{description} has invalid {metric}: must be non-negative")
    elif number <= 0.0:
        raise ComparisonError(f"{description} has invalid {metric}: must be positive")
    return number


def _index_dataset(environment: str, data: dict[str, Any]) -> dict[tuple[str, str], dict[str, Any]]:
    records = data.get("benchmarks")
    if not isinstance(records, list):
        raise ComparisonError(f"{environment}: top-level 'benchmarks' must be a list")

    indexed: dict[tuple[str, str], dict[str, Any]] = {}
    for record in records:
        if not isinstance(record, dict):
            raise ComparisonError(f"{environment}: benchmark record must be an object")
        run_name = record.get("run_name", record.get("name"))
        if not isinstance(run_name, str) or not run_name.startswith("BM_Lifetime_"):
            continue
        marker = next((candidate for candidate in POLICY_MARKERS if candidate in run_name), None)
        if marker is None:
            raise ComparisonError(f"{environment}: lifetime benchmark lacks policy marker: {run_name!r}")
        aggregate_name = record.get("aggregate_name")
        if aggregate_name not in REQUIRED_AGGREGATES:
            continue
        policy = marker.strip("_")
        subject = run_name.replace(marker, "_", 1)
        aggregate_key = (f"{policy}:{subject}", aggregate_name)
        if aggregate_key in indexed:
            raise ComparisonError(f"{environment}: duplicate {aggregate_name} aggregate for {policy} {subject}")
        for metric in REQUIRED_METRICS:
            _number(
                record,
                metric,
                f"{environment}: {policy} {subject} {aggregate_name}",
                allow_zero=aggregate_name == "stddev",
            )
        indexed[aggregate_key] = record

    if not indexed:
        raise ComparisonError(f"{environment}: no lifetime-policy aggregate records found")
    return indexed


def _aggregate(index: dict[tuple[str, str], dict[str, Any]], environment: str,
               policy: str, subject: str, aggregate_name: str) -> dict[str, Any]:
    key = (f"{policy}:{subject}", aggregate_name)
    try:
        return index[key]
    except KeyError as error:
        raise ComparisonError(
            f"{environment}: unpaired or missing {aggregate_name} aggregate for {policy} {subject}"
        ) from error


def _compare_pair(environment: str, subject: str,
                  index: dict[tuple[str, str], dict[str, Any]]) -> PairResult:
    aggregates = {
        (policy, kind): _aggregate(index, environment, policy, subject, kind)
        for policy in ("Protected", "External")
        for kind in REQUIRED_AGGREGATES
    }

    protected_items = _number(aggregates[("Protected", "median")], "items_per_second", subject)
    external_items = _number(aggregates[("External", "median")], "items_per_second", subject)
    protected_p99 = _number(aggregates[("Protected", "median")], "p99_us", subject)
    external_p99 = _number(aggregates[("External", "median")], "p99_us", subject)
    protected_items_stddev = _number(
        aggregates[("Protected", "stddev")], "items_per_second", subject, allow_zero=True
    )
    external_items_stddev = _number(
        aggregates[("External", "stddev")], "items_per_second", subject, allow_zero=True
    )
    protected_p99_stddev = _number(
        aggregates[("Protected", "stddev")], "p99_us", subject, allow_zero=True
    )
    external_p99_stddev = _number(
        aggregates[("External", "stddev")], "p99_us", subject, allow_zero=True
    )

    throughput_difference = external_items - protected_items
    throughput_regression = throughput_difference / external_items * 100.0
    throughput_noise = 2.0 * math.hypot(protected_items_stddev, external_items_stddev)
    throughput_upper_bound = throughput_regression + throughput_noise / external_items * 100.0
    latency_difference = protected_p99 - external_p99
    latency_regression = latency_difference / external_p99 * 100.0
    latency_noise = 2.0 * math.hypot(protected_p99_stddev, external_p99_stddev)
    latency_upper_bound = latency_regression + latency_noise / external_p99 * 100.0

    return PairResult(
        environment=environment,
        subject=subject,
        protected_mean_items=_number(
            aggregates[("Protected", "mean")], "items_per_second", subject
        ),
        external_mean_items=_number(aggregates[("External", "mean")], "items_per_second", subject),
        protected_items=protected_items,
        external_items=external_items,
        protected_items_stddev=protected_items_stddev,
        external_items_stddev=external_items_stddev,
        throughput_regression_pct=throughput_regression,
        throughput_noise=throughput_noise,
        throughput_upper_bound_pct=throughput_upper_bound,
        throughput_qualifies=(
            throughput_regression >= 5.0 and abs(throughput_difference) > throughput_noise
        ),
        protected_mean_p99=_number(aggregates[("Protected", "mean")], "p99_us", subject),
        external_mean_p99=_number(aggregates[("External", "mean")], "p99_us", subject),
        protected_p99=protected_p99,
        external_p99=external_p99,
        protected_p99_stddev=protected_p99_stddev,
        external_p99_stddev=external_p99_stddev,
        latency_regression_pct=latency_regression,
        latency_noise=latency_noise,
        latency_upper_bound_pct=latency_upper_bound,
        latency_qualifies=(latency_regression >= 5.0 and abs(latency_difference) > latency_noise),
    )


def compare_datasets(datasets: Sequence[tuple[str, dict[str, Any]]]) -> ComparisonResult:
    if not datasets:
        raise ComparisonError("no benchmark datasets supplied")

    pairs: list[PairResult] = []
    for environment, data in datasets:
        index = _index_dataset(environment, data)
        protected_subjects = {
            key[0].removeprefix("Protected:")
            for key in index
            if key[0].startswith("Protected:")
        }
        external_subjects = {
            key[0].removeprefix("External:")
            for key in index
            if key[0].startswith("External:")
        }
        if protected_subjects != external_subjects:
            missing_external = sorted(protected_subjects - external_subjects)
            missing_protected = sorted(external_subjects - protected_subjects)
            raise ComparisonError(
                f"{environment}: unpaired subjects; missing External={missing_external}, "
                f"missing Protected={missing_protected}"
            )
        for subject in sorted(protected_subjects):
            pairs.append(_compare_pair(environment, subject, index))

    if not pairs:
        raise ComparisonError("no Protected/External benchmark pairs found")
    qualifying = [pair for pair in pairs if pair.throughput_qualifies or pair.latency_qualifies]
    inconclusive = [
        pair
        for pair in pairs
        if not (pair.throughput_qualifies or pair.latency_qualifies)
        and (pair.throughput_upper_bound_pct >= 5.0 or pair.latency_upper_bound_pct >= 5.0)
    ]
    if qualifying:
        verdict = "KEEP_EXTERNAL"
    elif inconclusive:
        verdict = "INCONCLUSIVE"
    else:
        verdict = "REMOVE_EXTERNAL"
    return ComparisonResult(verdict, pairs, qualifying, inconclusive)


def _load(path: pathlib.Path) -> dict[str, Any]:
    try:
        with path.open(encoding="utf-8") as source:
            data = json.load(source)
    except (OSError, json.JSONDecodeError) as error:
        raise ComparisonError(f"cannot read {path}: {error}") from error
    if not isinstance(data, dict):
        raise ComparisonError(f"{path}: top-level JSON value must be an object")
    return data


def _print_result(result: ComparisonResult) -> None:
    for pair in result.pairs:
        throughput_mark = " QUALIFIES" if pair.throughput_qualifies else ""
        latency_mark = " QUALIFIES" if pair.latency_qualifies else ""
        print(f"[{pair.environment}] {pair.subject}")
        print(
            "  items/s median: "
            f"Protected={pair.protected_items:.6g} (sd={pair.protected_items_stddev:.6g}) "
            f"External={pair.external_items:.6g} (sd={pair.external_items_stddev:.6g}) "
            f"regression={pair.throughput_regression_pct:+.3f}% "
            f"2sigma_noise={pair.throughput_noise:.6g} "
            f"upper={pair.throughput_upper_bound_pct:+.3f}%{throughput_mark}"
        )
        print(
            "  p99_us median:  "
            f"Protected={pair.protected_p99:.6g} (sd={pair.protected_p99_stddev:.6g}) "
            f"External={pair.external_p99:.6g} (sd={pair.external_p99_stddev:.6g}) "
            f"regression={pair.latency_regression_pct:+.3f}% "
            f"2sigma_noise={pair.latency_noise:.6g} "
            f"upper={pair.latency_upper_bound_pct:+.3f}%{latency_mark}"
        )
    print(result.verdict)
    if result.qualifying:
        print("qualifying workloads:")
        for pair in result.qualifying:
            metrics = []
            if pair.throughput_qualifies:
                metrics.append(f"throughput {pair.throughput_regression_pct:.3f}%")
            if pair.latency_qualifies:
                metrics.append(f"p99 {pair.latency_regression_pct:.3f}%")
            print(f"  {pair.environment}: {pair.subject}: {', '.join(metrics)}")
    elif result.inconclusive:
        print("uncertainty reaches the 5% gate:")
        for pair in result.inconclusive:
            metrics = []
            if pair.throughput_upper_bound_pct >= 5.0:
                metrics.append(f"throughput upper {pair.throughput_upper_bound_pct:.3f}%")
            if pair.latency_upper_bound_pct >= 5.0:
                metrics.append(f"p99 upper {pair.latency_upper_bound_pct:.3f}%")
            print(f"  {pair.environment}: {pair.subject}: {', '.join(metrics)}")


def main(arguments: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if arguments is None else arguments)
    if not args:
        print(f"usage: {pathlib.Path(sys.argv[0]).name} RESULTS.json [RESULTS.json ...]", file=sys.stderr)
        return 2
    try:
        datasets = []
        for raw_path in args:
            path = pathlib.Path(raw_path)
            label = path.parent.name or path.stem
            datasets.append((label, _load(path)))
        result = compare_datasets(datasets)
    except ComparisonError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    _print_result(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
