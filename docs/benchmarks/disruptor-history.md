# Disruptor benchmark history

Archived measurements and implementation notes from September 2026. Descriptions
and build commands below reflect the implementations at the time of each
experiment. See the [current benchmark guide](../../benchmarks/concurrency-starling/disruptor/README.md)
for the current layout and build instructions.

# SPSC queue baseline

## Baseline 1 — captured before the public queue API

This saved baseline uses the earlier single-producer path and benchmark adapter,
producer pinned to CPU 2 and consumer to CPU 4 (distinct physical cores), Release
Clang 22.1.8/libc++, capacity 65,536, 1,000,000 warmup transfers, and 100,000,000
timed transfers per run.

| Run | Seconds | Million transfers/s |
| --- | ---: | ---: |
| 1 | 0.127155 | 786.44 |
| 2 | 0.119582 | 836.25 |
| 3 | 0.120145 | 832.33 |
| Arithmetic mean | 0.122294 | **818.34** |

All three runs passed payload validation. Raw results, exact averages, compiler
command, binary hash, and source snapshots are saved locally under
`benchmark_results/disruptor/baseline-1/`. Earlier measurements below are
exploratory comparisons.

Rigtorp was then run with the same pinning, capacity, warmup, transfer count, and
three repetitions. The modified reference is the temporary private-index,
wide-padding variant from the earlier 835-million/s comparison.

| Run | Upstream rigtorp, million transfers/s | Modified rigtorp, million transfers/s |
| --- | ---: | ---: |
| 1 | 253.37 | 753.38 |
| 2 | 272.42 | 750.09 |
| 3 | 268.55 | 867.49 |
| Arithmetic mean | **264.78** | **790.32** |

All payload checks passed. Corresponding CSVs and summaries are saved alongside
the Disruptor baseline as `rigtorp-upstream-*` and `rigtorp-private-wide-*`.

## Harness

`Menagerie.Benchmarks.Starling.SPSCQueue` now measures `Disruptor` directly through
its public blocking `emplace(args...)` / `pull()` calls. One producer and one consumer each
perform **100,000,000 calls**, transferring 8-byte unsigned integers. There is
no batch claiming, publishing, or consumer release. The single-producer consumer
caches the published frontier and refreshes it only after consuming those items.

Three implementations run through the same harness:

- `single`: Disruptor with `SingleProducerSequencer`.
- `multi`: Disruptor with `MultiProducerSequencer`, still with just one producer.
- `rigtorp`: upstream [`rigtorp/SPSCQueue`](https://github.com/rigtorp/SPSCQueue),
  supplied by the vcpkg `spscqueue` port (MIT).

The separate benchmark adapter has been removed. The ring still preconstructs
its slots, so `T` must be default-constructible; move-only elements are supported.
`emplace` constructs directly in a claimed slot when construction cannot throw.
Otherwise it constructs a temporary before claiming, then moves it into the slot
without throwing. Construction failure therefore cannot leave an unpublished
claim. Destruction must not throw. `pull` requires nonthrowing move construction
and destruction, and moves the value out before releasing the slot. Moved-from
slots live until reuse or destruction.

```cpp
struct Message {
    std::uint64_t id;
    std::uint64_t payload;
};
menagerie::starling::Disruptor<Message, menagerie::starling::SingleProducerSequencer> queue{1024};
// Producer:
queue.emplace(std::uint64_t{1}, std::uint64_t{42});
// Consumer:
Message message = queue.pull();
```

`Disruptor<T>` defaults to `MultiProducerSequencer` and `BusySpinWaitStrategy`.
Both sequencers require a single consumer. Advanced producers can continue using
`sequencer()` and `ring_buffer()` to claim, fill, and publish batches; the single
producer must publish earlier claims before calling `emplace`. For consumption,
manual reads followed by `consume(sequence)` or `consume_batch(first, last)` can
alternate with `pull()`: both use the same consumed position, available through
`get_consumed_sequence()`. Consume in order only after completing those manual
reads, and keep consumption serialized. The batch range is inclusive; releasing
it performs one release store to its last sequence, allowing producers to reuse
all completed slots. Multi-producer `pull()` checks actual
publication, including gaps left by producers that claimed but have not published.
With blocking strategies, it can still spin while such a claimed gap remains.

### Direct API check — 2026-09-14

After moving the methods into `Disruptor` and changing every cursor/cache to
atomic `WideSequence`, the same three-run pinned SPSC workload measured
804.06, 799.53, and 784.10 million transfers/s: **795.90 million/s average**.
All payload checks passed. Results and build metadata are saved locally under
`benchmark_results/disruptor/public-api-2026-09-14/`; Baseline 1 remains the earlier
snapshot. The core and logger Release tests, core ASan/UBSan tests (leak detection
disabled for this traced environment), and 21 targeted TSan tests passed.

### Five-counter layout — 2026-09-14

The separate `next_pull_sequence_` was removed: `pull()` derives its next position
from `consumed_sequence_ + 1`. The retained state is:

| Counter | Role |
| --- | --- |
| `claimed_sequence_` | Highest reserved position; claims may still be unpublished. |
| `published_sequence_` | Highest position with a completed payload. |
| `consumed_sequence_` | Highest completed read, shared by manual consumption and `pull()`. |
| `cached_capacity_limit_` | Producer's cached upper bound for safe reservations. |
| `cached_published_sequence_` | Consumer's acquired snapshot of contiguous ready items. |

The last two are deliberately stale caches that reduce cross-thread reads.
Removing the read-position copy reduced the SPSC object from 1,024 to 896 bytes.
Removing the publication cache as well reached 768 bytes, but cut mean throughput
from 758.50 to 284.49 million transfers/s in that six-round comparison.

Instead, `[[no_unique_address]]` lets ring metadata reuse the enclosing
sequencer's tail padding. Every `WideSequence` member retains its full spacing.
The retained five-counter layout is **768 bytes** on this Clang/x86-64 build,
excluding ring storage: 25% smaller than the previous object. Size reductions
from tail-padding reuse depend on the compiler and ABI.

A second six-round, rotated comparison with the same pinned workload measured:

| Layout | Bytes | Mean million transfers/s |
| --- | ---: | ---: |
| Six counters | 1,024 | 775.63 |
| Five counters | 896 | 751.41 |
| Five counters, shared metadata padding | 768 | 776.93 |

### Regular capacity cache — 2026-09-15

`cached_capacity_limit_` now uses `Sequence` because it is producer-private and
does not need double-width separation from another thread's writes. On this
Clang/x86-64 build, the SPSC sequencer shrank from 640 to 512 bytes and the full
queue from 768 to 640 bytes.

A ten-round rotated comparison of otherwise identical pinned binaries measured
704.60 million transfers/s for `Sequence` and 681.62 million transfers/s for
`WideSequence`. The run was noisy (ranges 448.33–799.51 and 547.70–748.48), so it
does not support a precise speed difference, but it found no performance reason
to retain the extra 128 bytes. A final three-run build measured 678.77, 735.17,
and 712.66 million transfers/s, or 708.87 million/s on average. Every run passed
the 100-million-item payload and checksum validation.

The packed layout showed no clear throughput difference from the original in
this comparison. All 36 measured runs validated payloads. Core/logger Release
tests, ASan/UBSan (leak detection disabled), and 22 targeted TSan tests passed,
including mixing manual consumption with `pull()`. Measurements and exact
prototype patches are saved under `benchmark_results/disruptor/cursor-footprint/`.

### All counters at standard spacing — 2026-09-18

Replacing the four remaining SPSC `WideSequence` members with `Sequence` showed
**no measurable throughput degradation in this workload**. The queue object
shrinks from **640 to 384 bytes (40%)**, excluding ring storage; the single-producer
sequencer shrinks from 512 to 320 bytes. This experiment used temporary header
snapshots; the production implementation still uses its existing layout.

The existing public `emplace`/`pull` harness measured the current layout, the
standard-spacing variant, and unmodified upstream Rigtorp SPSCQueue v1.1. Runs
used Ryzen 5 7600X cores 2 and 4, capacity 65,536, 8-byte payloads, and Clang
22.1.8/libc++ with Release `-O3 -DNDEBUG`, without LTO or `-march=native`.
All six execution orders were balanced across repetitions.

| Layout | Median million transfers/s, 12 × 100M items | Median million transfers/s, 18 × 1B items |
| --- | ---: | ---: |
| Current `WideSequence` layout | 714.86 | 722.79 |
| All counters using `Sequence` | 745.85 | 742.16 |
| Upstream Rigtorp SPSCQueue | 277.14 | 276.48 |

Each run warmed up with 1M or 10M transfers respectively. Standard spacing was
4.34% ahead by median in the short runs and 2.68% ahead in the longer runs.
However, the longer-run mean paired throughput change was only +1.66%, with an
approximate 95% paired bootstrap interval of **−1.43% to +4.56%**. This supports
no detected regression, rather than a reliable speedup or exact equivalence.
The conclusion is specific to this machine, capacity, payload, and 1P1C workload;
multi-producer performance was not measured.

All 90 timed runs passed FIFO/checksum validation, as did 12 preliminary checks
covering capacities 1, 2, 1,024, and 65,536. Raw CSV, logs, summaries, source
snapshots, the exact type-replacement patch, binary hashes, build commands, and
`compare.py` are saved locally under
`benchmark_results/disruptor/sequence-width-2026-09-18/`.

## Build and run

The external comparison is opt-in. The `release` preset enables the
`starling-benchmarks` vcpkg feature, which supplies the queue dependency:

```sh
cmake --preset release -DMENAGERIE_BENCHMARK_RIGTORP_SPSC=ON
cmake --build build/release --target Menagerie.Benchmarks.Starling.SPSCQueue
lscpu -e=CPU,CORE,SOCKET,NODE,ONLINE
build/release/benchmarks/concurrency-starling/disruptor/Menagerie.Benchmarks.Starling.SPSCQueue \
  --producer-cpu 2 --consumer-cpu 4
```

Select two allowed logical CPU IDs on distinct physical cores, preferably sharing
the last-level cache. IDs 2 and 4 are examples: numbering alone does not establish
physical topology. Both workers check affinity success. If omitted, both run
unpinned and the output reports `-1` for their CPU IDs.

Run `--help` for options. Defaults are capacity 65,536, three repetitions, and
1,000,000 warmup transfers per implementation per repetition. Use `--queue single`,
`--queue multi`, or `--queue rigtorp` to isolate an implementation. Capacity must
be a nonzero power of two; both queues get the same **usable** capacity (rigtorp
allocates an extra sentinel slot and boundary padding).

## Measurement

Warmup uses the same workers and queue as the timed phase. A barrier completion
records the start before either worker enters its measured loop. The end is the
later worker finish time, excluding thread creation and joining. Every timed value
is checked against its FIFO position and accumulated into an observable checksum.
Warmup also checks FIFO order. Validation and affinity failures produce
a nonzero exit code and discard the affected timing. The run order rotates between
repetitions. Each CSV row reports elapsed seconds, millions of transfers/second,
inverse throughput in ns/transfer, and checksum.

One transfer includes **one enqueue and one dequeue**. Do not double the reported
rate to compare it with another benchmark's message throughput. `ns_per_transfer`
is inverse throughput, **not measured message latency**. Validation contributes
some consumer work equally across variants. Use a Release build (`-O3 -DNDEBUG`)
and record CPU topology, compiler, and flags with results.

`AlignedSequence<Alignment>` parameterizes the atomic counter's alignment in
bytes. `Sequence` retains the hardware interference size; `WideSequence` doubles
it (64 and 128 bytes on the measured machine). Shared publication and consumption
positions and the private claim counter remain atomic `WideSequence` members.
The producer-private `cached_capacity_limit_` uses `Sequence`; its neighbors are
producer-owned or immutable, so double-width spacing adds no isolation. Private
counters use relaxed loads/stores; shared positions retain acquire/release ordering.

The field names express their role: `published_sequence_` is the single producer's
completed frontier; `claimed_sequence_` counts reservations; `consumed_sequence_`
permits storage reuse; `cached_capacity_limit_` is the producer's cached highest
permitted reservation. Multi-producer publication is tracked by `slot_generations_`.
`get_published_sequence(next)` is the common polling API: SPSC loads its published
frontier, while MP scans generation markers from `next` up to the claimed position
and stops at the first gap. `approx_size()` mirrors the counters used by
`remaining_capacity()`; MP therefore includes claimed but unpublished reservations.
Wait strategies accept `const AtomicSequence&`, the common operation base of both
sequence widths. Existing custom strategies taking only `const Sequence&` need
that parameter updated. This adds no virtual dispatch to concrete strategies.

The single producer advances its atomic private claim counter with a relaxed
load and store; the multi-producer claim still uses atomic `fetch_add`. `pull()`
caches a contiguous frontier only after the sequencer confirms its publication.
Blocking and timeout-blocking notification now synchronize with the
wait mutex, preventing notifications from being lost between a predicate check
and sleeping. The historical comparisons below predate these public API changes.

The producer caches `consumed + capacity` as the highest sequence it may claim.
Most claims therefore compare their sequence directly with the limit, avoiding
a capacity load and subtraction. The sum is computed unsigned and saturated at
`INT64_MAX` so it fits `Sequence`. The consumer
derives its next sequence from the shared consumed position once per pull, then
uses that local value to read and release the slot.

The older `Menagerie.Benchmarks.Starling.Disruptor` remains a separate low-level
benchmark with batch consumption and other scenarios.

## Original baseline

Run on 2026-09-13: AMD Ryzen 5 7600X, producer CPU 2 and consumer CPU 4 (different
physical cores, same 32 MiB L3), Clang 22.1.8 with libc++, Release `-O3 -DNDEBUG`,
no LTO or `-march=native`. Each row below summarizes three runs of 100,000,000
transfers, capacity 65,536, with 1,000,000 warmup transfers before each run.

| Implementation | Median seconds | Median million transfers/s | Range, million transfers/s |
| --- | ---: | ---: | ---: |
| Disruptor single producer | 0.291128 | 343.49 | 341.12–347.77 |
| Disruptor multi producer (1P1C) | 2.615315 | 38.24 | 38.09–38.29 |
| rigtorp SPSCQueue | 0.362616 | 275.77 | 258.34–288.24 |

Every run passed FIFO validation and produced checksum `4999999950000000`.
The original single-producer wrapper was about 1.25 times rigtorp's throughput in
this configuration. This is a machine/workload-specific baseline, not a measured
improvement to the Disruptor or evidence isolating any particular cache effect.
Raw CSV and the compiler command/topology are saved locally under
`benchmark_results/disruptor/spsc_baseline/` (the repository ignores that directory).

## Optimization comparison

The original executable was preserved before editing. On the same machine and
CPU pair, three rounds alternated the original, wide-only, and wide-plus-cache
executables. Every measured run transferred 100,000,000 items after 1,000,000
warmup transfers. Median SPSC throughput:

| Configuration | Million transfers/s |
| --- | ---: |
| Original layout and consumer | 345.58 |
| Wide spacing, original consumer | 311.74 |
| Wide spacing and cached consumer | 703.92 |

Wide spacing alone was slower in this comparison. Together with consumer cursor
caching, throughput increased by 103.7% over the original executable.

A second comparison held consumer caching constant and alternated the original
and wide core layouts over five rounds:

| Cached-consumer configuration | Median million transfers/s | Range |
| --- | ---: | ---: |
| Original core layout | 590.05 | 572.56–617.64 |
| Wide core layout | 717.84 | 690.87–723.81 |

With caching enabled, the wide layout improved the median by 21.7%. This supports
keeping both changes for SPSC on this machine; it does not establish prefetching
as the cause. The multi-producer widening experiment showed no benefit, so its
original layout was restored. All runs passed FIFO and checksum validation.

The two comparison CSVs are `2026-09-13-optimization-comparison.csv` and
`2026-09-13-cached-layout-comparison.csv` in the local results directory. The first
comparison widened both sequencers; the final implementation widens only SPSC.

The final executable was then run for three repetitions with the original MP
layout restored. Median rates were **700.91 million transfers/s** for SPSC,
37.96 million/s for MP, and 252.96 million/s for rigtorp. SPSC transferred
100,000,000 items in a median 0.142673 seconds. Raw results are in
`2026-09-13-optimized-final.csv`.

## Standard-size spacing comparison

Consumer cursor caching remained enabled in all four layouts. Eight rounds
rotated their execution order; each run transferred 100,000,000 items on CPUs
2 and 4 with the same Release flags, capacity, and warmup as above. The 64-byte
variants also used standard alignment for the adapter's consumer state.

| Layout | Sequencer bytes | Queue object bytes | Median million transfers/s |
| --- | ---: | ---: | ---: |
| Current 128-byte spacing | 640 | 896 | 701.66 |
| 64-byte spacing, separate private counters | 320 | 448 | 593.63 |
| 64-byte spacing, private counters together | 256 | 384 | 585.95 |
| Original compact 64-byte layout | 192 | 320 | 582.05 |

The grouped layout places `next_value_` and `cached_gating_` in one producer-only
cache line while keeping read-mostly metadata separate. The compact layout also
puts that metadata alongside the private counters. Object sizes exclude the ring's
heap allocation, which is identical across variants.

Standard separated spacing halved the sequencer footprint at a 15.4% throughput
cost in this comparison. Grouping the private counters saved more space but did
not recover throughput. All 32 runs passed FIFO/checksum validation. The 128-byte
layout remains the implementation; the alternatives were built using temporary
header overlays. Results and compiler commands are saved as
`2026-09-13-standard-spacing-comparison.csv` and
`2026-09-13-standard-spacing-environment.json` in the local results directory.

## Explaining the rigtorp gap

The optimized SPSC adapter uses private progress counters (`next_value_` and
`next_consume_`) and separate atomic publication counters. Upstream rigtorp uses
its published atomic indices for the owner's progress as well. In the inspected
Clang output, `front()` and `pop()` each load the atomic read index. The private
counter design allows that value to be reused across the corresponding work and
keeps local progress reads separate from the cache line observed by the peer.
Both implementations already cache peer progress; neither SPSC hot loop contains
a locked read-modify-write instruction.

To check this explanation, temporary copies of rigtorp were compiled with the
same benchmark. The private-index variants add producer-owned and consumer-owned
plain counters, use those for local indexing, and retain the original release
stores and acquire loads for cross-thread publication. Capacity and wraparound
logic remain the upstream implementation's. Eight interleaved rounds of
100,000,000 transfers produced:

| rigtorp diagnostic variant | Median million transfers/s |
| --- | ---: |
| Unmodified upstream | 262.64 |
| Wider padding only | 255.11 |
| Private local indices, standard padding | 816.82 |
| Private local indices, wider padding | 835.52 |

This supports local/private index handling as a major source of the measured
gap, rather than wider padding alone. It does not separately quantify compiler
reuse, dependency chains, or coherence effects. The modified rigtorp variants
also exceed the earlier Disruptor result, so these measurements do not establish
inherent superiority of the Disruptor algorithm. All variants passed payload
validation; the private-index variant also completed a capacity-one run under
ThreadSanitizer without a report.

The repository benchmark still uses unmodified upstream rigtorp. Diagnostic
results are saved in `2026-09-13-rigtorp-diagnostics.csv`, with exact patches,
compiler commands, and consumer assembly under `rigtorp-diagnostics/` in the
local results directory.

## Closing the remaining gap

The gap to the modified rigtorp reference reproduced within a single executable:
711.34 versus 843.04 million transfers/s over six alternating runs each. All
following comparisons use that temporary private-index, wide-layout reference;
the normal benchmark dependency is still upstream rigtorp.

The original producer computed `sequence - buffer_size` on every claim, then
compared it with cached gating. Caching the equivalent claim limit moves this
arithmetic to cache refreshes. Ten rotated rounds of 100,000,000 transfers gave:

| Disruptor variant | Median million transfers/s |
| --- | ---: |
| Before this change | 707.20 |
| Cached claim limit | 786.48 |
| Busy spinning instead of yielding | 699.17 |
| Claim limit plus busy spinning | 796.19 |

The original spin/yield behavior was retained. A subsequent twelve-round
comparison investigated consumer/index changes:

| Variant | Median million transfers/s |
| --- | ---: |
| Cached claim limit | 801.46 |
| Claim limit plus local consumer sequence snapshot | 826.41 |
| Claim limit plus duplicated immutable ring metadata | 789.12 |
| Claim limit plus a next-to-claim producer counter | 755.38 |
| Modified rigtorp reference | 855.57 |

Only the claim limit and consumer snapshot were retained. The snapshot avoids a
reload of `next_consume_` after acquiring a new published frontier; the normal
cached path already reused it. Its smaller measured gain is less certain than
the claim-limit improvement: it improved seven of twelve paired rounds.
Perf counters also showed that the reference could execute more instructions
while using fewer cycles. Instruction count alone does not explain performance;
these experiments do not identify a single microarchitectural cause for the
remaining few percent.

Final verification became noisy for both implementations. Eight alternating
100,000,000-transfer runs measured medians of 754.00 and 762.12 million/s for
Disruptor and the modified reference. Four alternating **1,000,000,000-transfer**
runs measured 786.99 and 783.04 million/s; ranges were 755.66–800.65 and
485.40–842.06 respectively. These final runs do not establish a stable remaining
gap or a speed advantage over the modified reference. The earlier 835 million/s
number is a measured result under earlier conditions, not a fixed ceiling.

All timed runs checked every payload. The final core suite passed in Release and
under ASan/UBSan; LeakSanitizer was disabled because this environment's tracing
prevented its process inspection. All thirteen sequence/SPSC tests passed under
ThreadSanitizer, including mixed claiming after partial consumption and valid
claims through `INT64_MAX` with an unsigned capacity threshold above that value.
No adapter unit tests were added.

Raw comparisons, exact diagnostic patches, compiler commands, and perf captures
are under `benchmark_results/disruptor/spsc_baseline/remaining-gap/` locally.

## Compiler-hint experiments

Hints were evaluated against the Baseline 1 implementation using the same pinned
CPUs 2/4, Release flags, 65,536 capacity, 1,000,000 warmup transfers, and
100,000,000 timed transfers. The variant order rotated between rounds.

Seven rounds tested these configurations (arithmetic mean throughput):

| Variant | Million transfers/s |
| --- | ---: |
| Unchanged control | 834.38 |
| Producer cache-refresh branch marked unlikely | 815.13 |
| Consumer cache-refresh branch marked unlikely | 804.31 |
| Both cache-refresh branches marked unlikely | 811.29 |
| Force-inline queue/sequencer/sequence/ring/wait methods | 839.28 |
| Branch hints plus forced inlining | 782.08 |
| Branch hints plus isolated cold producer wait | 818.59 |

The executable `.text` section was byte-for-byte identical between the control
and the forced-inline variant, and between the branch-only and branch-plus-inline
variants. Their timing differences are measurement variation, not an inlining
improvement. Marking hot methods with `gnu::hot` also produced identical code.

Follow-up trials tested hints inside the spin loops, a producer-only
`gnu::cold`/`gnu::noinline` wait helper with an unlikely call, and outlining that
helper without the cold/branch hints. Only the producer-only cold helper looked
promising enough to confirm separately. Twelve alternating paired runs gave:

| Configuration | Mean million transfers/s | Median million transfers/s |
| --- | ---: | ---: |
| Unchanged control | 820.95 | 831.66 |
| Isolated cold producer wait | 825.86 | 818.01 |

The candidate won only six of twelve pairs; its median paired difference was
-0.63 million transfers/s. This does not demonstrate a repeatable improvement.
No compiler hints or outlining changes were retained. All 105 timed runs passed
payload validation. Baseline 1 remains the reference implementation.

Exact patches, compiler commands, code hashes, and all measurements are saved
locally in `benchmark_results/disruptor/compiler-hints/`.

## Checks

```sh
cmake --build build/release --target Menagerie.Tests.Unit.Disruptor
ctest --test-dir build/release \
  -R '^Menagerie.Tests.Unit.Disruptor$' \
  --output-on-failure --timeout 60
```

There is no benchmark adapter or benchmark CTest registration. Public API tests
are part of the core Disruptor suite and cover move-only messages, exception
safety, ownership, manual producer batches, multi-producer FIFO, delayed
publication, blocking wakeups, and both sequence widths through erased waits.
The relevant core tests can also run under ThreadSanitizer:

```sh
cmake --preset tsan
cmake --build build/tsan --target Menagerie.Tests.Unit.Disruptor
build/tsan/tests/unit_tests/common/Menagerie.Tests.Unit.Disruptor \
  --gtest_filter='DisruptorTest.Sequence*:DisruptorTest.ErasedWaitStrategiesAcceptBothSequenceWidths:SingleProducerDisruptorTest.*:DisruptorQueueTest.*'
```

Design references supplied for subsequent optimization work:
[SPSC queue notes](https://wyattgill9.github.io/knowledge-base/spsc-queue.html) and
[Optimizing a lock-free ring buffer](https://david.alvarezrosa.com/posts/optimizing-a-lock-free-ring-buffer/).
