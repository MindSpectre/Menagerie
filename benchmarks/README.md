# Benchmarks

Build benchmarks with the Release preset:

```sh
cmake --preset release
cmake --build build/release --target <target>
```

Project options are declared in [`cmake/features.cmake`](../cmake/features.cmake).
`DO_BENCHMARKS` is the master switch. When it is `ON`, the individual suite options
below select which benchmarks to build. They default to `ON`; set a suite's option
to `OFF` to omit its targets and package lookup. Setting `DO_BENCHMARKS=OFF` skips
all suites, regardless of their cached option values.

Every enabled suite requires its listed packages: a missing package fails CMake
configuration. HTTP and PostgreSQL suites also follow their existing component
options (`BUILD_HTTP`, and `BUILD_DATABASE`/`BUILD_POSTGRESQL`, respectively).
vcpkg owns dependency installation through [`vcpkg.json`](../vcpkg.json);
benchmark CMake files only import the packages and do not download dependencies.

| Suite option | Required CMake packages | vcpkg feature |
| --- | --- | --- |
| `BENCHMARK_CROW_LOGGER` | `absl` | `abseil-benchmarks` |
| `BENCHMARK_DISRUPTOR` | `SPSCQueue` | `starling-benchmarks` |
| `BENCHMARK_RESOURCE_POOL` | `benchmark` | `starling-benchmarks` |
| `BENCHMARK_HTTP` | `Drogon` | `drogon-benchmarks` |
| `BENCHMARK_POSTGRESQL` | `benchmark`, `libpqxx` | `pg-benchmarks` |

| Suite | Target examples | vcpkg feature for external comparisons |
| --- | --- | --- |
| [Disruptor](concurrency-starling/disruptor/README.md) | `Menagerie.Benchmarks.Starling.Disruptor`, `Menagerie.Benchmarks.Starling.SPSCQueue` | `starling-benchmarks` (SPSCQueue) |
| Resource pool | `Menagerie.Benchmarks.Starling.ResourcePool.FlagshipBurst`, `Menagerie.Benchmarks.Starling.ResourcePool.PlLifetime` | `starling-benchmarks` (Google Benchmark) |
| Crow logger | `Menagerie.Benchmarks.Crow.FileBenchmark`, `Menagerie.Benchmarks.Abseil.FileBenchmark` | `abseil-benchmarks` |
| [HTTP](http-albatross/README.md) | `Menagerie.Benchmarks.Albatross.BenchServer`, `Menagerie.Benchmarks.Albatross.DrogonBenchServer` | `drogon-benchmarks` |
| PostgreSQL | `Menagerie.Benchmarks.Savanna.Elephant.RawVsCompiled` | `pg-benchmarks` (Google Benchmark, libpqxx) |

These dependency features are enabled by default. To select only the concurrency
benchmark dependencies:

```sh
cmake --preset release -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON \
  -DVCPKG_MANIFEST_FEATURES=starling-benchmarks \
  -DBENCHMARK_CROW_LOGGER=OFF -DBENCHMARK_HTTP=OFF -DBENCHMARK_POSTGRESQL=OFF
```

Run scripts live beside their suite or in its `scripts/` directory. Preserve raw
output and environment details under the ignored `benchmark_results/` directory.
Historical experiments are documented in the
[Disruptor history](../docs/benchmarks/disruptor-history.md) and
[HTTP history](../docs/benchmarks/http-history.md). Retired standalone HTTP probes
are preserved in Git history, with their source revision recorded in the archive.
