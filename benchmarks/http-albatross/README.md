# HTTP benchmarks

The maintained HTTP/1.1 workload compares Menagerie's server with Drogon using
the same keep-alive load generator and `/ping` endpoint.

| Target | Source | Role |
| --- | --- | --- |
| `Menagerie.Benchmarks.Albatross.BenchServer` | `bench_server.cpp` | Menagerie server |
| `Menagerie.Benchmarks.Albatross.DrogonBenchServer` | `drogon_bench_server.cpp` | Drogon reference server |
| `Menagerie.Benchmarks.Albatross.Bomber` | `http_bomber.cpp` | Saturating load generator |

The suite is enabled by `DO_BENCHMARKS`, `BENCHMARK_HTTP`, and the existing
`BUILD_HTTP` component option (all default to `ON`). Drogon is supplied by vcpkg's
`drogon-benchmarks` feature and is required: configuration fails if it is missing.
Set `BENCHMARK_HTTP=OFF` to omit the suite and its package lookup. The comparison
script requires all three binaries.

## Build and measure

Run from the repository root:

```sh
cmake --preset release -B build/bench -DENABLE_LOGGING=OFF -DCOMPONENT_LOGGING=OFF
cmake --build build/bench --target \
  Menagerie.Benchmarks.Albatross.Bomber \
  Menagerie.Benchmarks.Albatross.BenchServer \
  Menagerie.Benchmarks.Albatross.DrogonBenchServer
./benchmarks/http-albatross/run_bench.sh build/bench 100000000
```

`run_bench.sh` accepts a build directory and request count. Defaults are
`build/bench` and 100 million requests per run, with 5% warmup. Set `REPS` to
change the default three repetitions and `OUT` to change the default
`benchmark_results/http` output directory. Results are saved in `raw.jsonl`.

The script's CPU assignments assume a six-core, twelve-thread machine with
physical CPUs 0–5 and SMT siblings 6–11. Inspect `lscpu -e=CPU,CORE,SOCKET,NODE`
and adjust the script's CPU sets for other topologies. It reports both a primary
layout with client/server SMT contention and a control layout with separate
physical cores. Compare servers within the same run and core layout.

## Profile

The profiling preset preserves frame pointers and disables logging:

```sh
cmake --preset release-perf
cmake --build build/release-perf --target \
  Menagerie.Benchmarks.Albatross.Bomber \
  Menagerie.Benchmarks.Albatross.BenchServer \
  Menagerie.Benchmarks.Albatross.DrogonBenchServer
./benchmarks/http-albatross/run_perf.sh build/release-perf 20000000 1
```

`run_perf.sh` accepts a build directory, request count, and pipeline depth. It
requires `perf` access and writes profiles under `benchmark_results/http` unless
`OUT` is set. The script checks disk space and rejects RAM-backed output paths.
Profiling results should be kept separate from ordinary throughput measurements.

## Historical experiments

The [HTTP measurement history](../../docs/benchmarks/http-history.md) preserves
earlier results and findings, including the retired standalone Asio and stdexec
probes. Those experiment programs are no longer part of the benchmark directory;
the archive identifies the Git revision containing their sources.
