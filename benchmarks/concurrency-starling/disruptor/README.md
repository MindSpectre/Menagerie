# Disruptor benchmarks

Two targets cover different workloads:

- `Menagerie.Benchmarks.Starling.SPSCQueue`: public queue API throughput, comparing
  single-producer Disruptor, multi-producer Disruptor with one producer, and
  unmodified upstream Rigtorp SPSCQueue.
- `Menagerie.Benchmarks.Starling.Disruptor`: low-level scenarios, including batch
  consumption and multiple producers.

## Build and run

Enable `DO_BENCHMARKS` and `BENCHMARK_DISRUPTOR` to build both targets (both
options default to `ON`). vcpkg's default `starling-benchmarks` feature supplies
SPSCQueue, which is required: configuration fails if it is missing. Set
`BENCHMARK_DISRUPTOR=OFF` to skip the suite and its package lookup. There is no
separate Rigtorp option or dependency download in the benchmark build.

```sh
cmake --preset release
cmake --build build/release --target Menagerie.Benchmarks.Starling.SPSCQueue
lscpu -e=CPU,CORE,SOCKET,NODE,ONLINE
build/release/benchmarks/concurrency-starling/disruptor/Menagerie.Benchmarks.Starling.SPSCQueue \
  --producer-cpu 2 --consumer-cpu 4
```

Choose allowed CPU IDs on distinct physical cores, preferably sharing the
last-level cache. CPUs 2 and 4 are examples; numbering alone does not establish
physical topology. Both workers check affinity success. Omitting the CPU options
runs unpinned and reports `-1` for each CPU.

For a dependency-minimal benchmark build, enable only the relevant vcpkg feature:

```sh
cmake --preset release -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON \
  -DVCPKG_MANIFEST_FEATURES=starling-benchmarks \
  -DBENCHMARK_CROW_LOGGER=OFF -DBENCHMARK_HTTP=OFF -DBENCHMARK_POSTGRESQL=OFF
```

Project build options live in `cmake/features.cmake`; vcpkg features control which
external dependencies are installed. See the [benchmark overview](../../README.md).

## SPSC workload

Run the executable with `--help` for all options. Defaults:

| Setting | Value |
| --- | --- |
| Implementations | `single`, `multi`, `rigtorp` |
| Timed transfers per implementation | 100,000,000 |
| Warmup transfers | 1,000,000 |
| Repetitions | 3 |
| Usable capacity | 65,536 |
| Payload | 8-byte unsigned integer |

Use `--queue single`, `--queue multi`, or `--queue rigtorp` to select one
implementation. `multi` still uses exactly one producer and one consumer in this
harness. Capacity must be a nonzero power of two; Rigtorp receives the same usable
capacity and allocates its own extra sentinel slot and boundary padding.

Each transfer consists of one blocking `emplace` and one blocking `pull`. There
is no batch claiming, publishing, or release. Warmup uses the same queue and
workers as the timed phase. A barrier starts timing before either worker enters
its measured loop; timing ends when the later worker finishes, excluding thread
creation and joining. Execution order rotates between repetitions.

Every payload is checked for FIFO order and accumulated into an observable
checksum. Validation or affinity failure discards the timing and exits nonzero.
CSV rows report seconds, million transfers/s, inverse throughput in ns/transfer,
and checksum. **Inverse throughput is not message latency.** Do not double the
transfer rate to account for enqueue and dequeue.

Use a Release build and record compiler flags, CPU topology, capacity, payload,
and affinity with results. The metadata includes the single-producer queue and
sequencer object sizes, excluding ring allocation.

## Sequence layout

Disruptor and both sequencers use cache-line-aligned `Sequence` members.
The SPSC path keeps five counters: claimed, published, consumed, cached capacity
limit, and cached publication frontier. Thread-private counters use relaxed
operations; shared positions retain acquire/release ordering. `WideSequence`
remains available as a sequence type, and wait strategies continue to accept
`AtomicSequence`.

The September 18 comparison on Ryzen 5 7600X, Clang 22.1.8/libc++, pinned cores
2 and 4, capacity 65,536, measured these medians over 18 runs of one billion
transfers each:

| Configuration | Million transfers/s | Queue bytes |
| --- | ---: | ---: |
| Previous wide layout | 722.79 | 640 |
| Current `Sequence` layout | 742.16 | 384 |
| Upstream Rigtorp SPSCQueue v1.1 | 276.48 | — |

The change showed no measurable degradation in this workload and reduced queue
object size by 40%. Run-to-run variation prevents claiming a reliable speedup.
All 90 timed runs across the short and sustained comparisons passed validation.
See the [measurement history](../../../docs/benchmarks/disruptor-history.md)
for methodology, earlier experiments, and local raw-result locations.
