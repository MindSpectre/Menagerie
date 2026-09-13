#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)
OUT=${1:-"$REPO_ROOT/benchmark_results/resource_pool/2026-09-13-simple-pool"}
BIN="$REPO_ROOT/build/release/benchmarks/concurrency-starling/resource_pool/Menagerie.Benchmarks.Starling.ResourcePool.PlLifetime"

mkdir -p "$OUT/floating" "$OUT/pinned"

cmake --preset release -S "$REPO_ROOT"
cmake --build --preset release --target Menagerie.Benchmarks.Starling.ResourcePool.PlLifetime -j2

taskset -c 0-9 "$BIN" --pin=0 \
  --benchmark_min_time=0.05s \
  --benchmark_repetitions=10 \
  --benchmark_enable_random_interleaving=true \
  --benchmark_report_aggregates_only=true \
  --benchmark_out="$OUT/floating/results.json" \
  --benchmark_out_format=json

taskset -c 0-9 "$BIN" --pin=1 \
  --benchmark_min_time=0.05s \
  --benchmark_repetitions=10 \
  --benchmark_enable_random_interleaving=true \
  --benchmark_report_aggregates_only=true \
  --benchmark_out="$OUT/pinned/results.json" \
  --benchmark_out_format=json
