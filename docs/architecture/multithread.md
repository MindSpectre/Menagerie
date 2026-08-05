# Multithread Library

The multithread library (`common/multithread/`) is Menagerie's collection of concurrency primitives:
bounded resource pools for both thread-blocking and coroutine callers, a futex-based park/notify
primitive, an LMAX-style lock-free ring buffer, a growable thread pool, and a handful of small
supporting utilities (mutex-wrapped resource access, `io_context` runners, pause/pin helpers).
Every primitive here is a standalone, header-mostly (`INTERFACE`) CMake target; consumers either
name one directly (e.g. `Menagerie.Common.Multithread.ResourcePool.Sync`) or pull in everything
through the umbrella `#include <menagerie/multithread>`, linking the combined
`Menagerie::Common::Multithread` alias target
(`export/menagerie/multithread`).

The library has no single "front door" type: each primitive solves a different concurrency problem,
and the choice between them is a choice about the calling code's execution model.

- **A thread that can block** waiting for a resource reaches for the ResourcePool section below (or
  EventCount directly, for a bespoke wait).
- **A coroutine on an `io_context`** that must never block its executor thread reaches for the
  AsyncResourcePool section below.
- **A single producer (or a bounded set of producers) publishing to one or more consumers** at the
  highest achievable throughput reaches for the Disruptor section below.
- **Fire-and-forget work with an unpredictable arrival rate** reaches for `ThreadPool`, covered
  under Other primitives below.

```text
common/multithread/
|-- resource_pool/
|   |-- sync/               ResourcePool<T, MaxSize>, Lease<T>            (blocking wait)
|   `-- async/               AsyncResourcePool<T, MaxSize>, AsyncLease<T>  (coroutine suspend)
|-- event_count/             EventCount                                   (futex park/notify)
|-- disruptor/                Disruptor<T, SequencerT, WaitStrategyT>       (lock-free ring buffer)
|-- thread_pool/              ThreadPool                                   (growable worker pool)
|-- thread_safe_resource/     ThreadSafeResource<T>                       (mutex/shared_mutex wrapper)
|-- asio_backend/             AsioBackend, ShardedAsioBackend              (io_context runners)
|-- utils/                    pause_arc_agnostic, pin_current_thread_to_core
`-- export/menagerie/multithread   umbrella header
```

## ResourcePool

`ResourcePool<T, MaxSize>` is a
bounded pool of up to `MaxSize` interchangeable `T` resources, held entirely inline (no heap
allocation for the pool's own storage) and partitioned at construction into two regions:

- **Pinned slots** (`[0, n_pinned)`) are exclusively owned by one designated caller each. The
  caller reads `pinned(i)` - an `std::atomic<T*>&` - once and caches it; every subsequent access is
  a single `load(acquire)`, no CAS, no contention. A slot's `T*` briefly reads `nullptr` while a
  repair thread swaps it out (`exchange(nullptr, acq_rel)` -> reconstruct -> `store(release)`); the
  owner just skips its work for that iteration. The pointer load *is* the synchronization.
- **Free slots** (`[n_pinned, n_pinned + n_free)`) are shared: any thread can acquire one. Freedom
  is tracked by a bitset, one bit per slot, packed into cache-line-isolated 64-bit words
  (`struct alignas(64) PaddedWord`). `try_acquire()` scans the words starting from a
  `thread_local` hint (seeded from `hash(thread_id)`, so concurrent callers spread their CAS
  traffic across different words) and claims the lowest set bit with one
  `compare_exchange_weak(cur, cur & ~bit, acquire, relaxed)`. A `Lease<T>`
  (`detail/lease.hpp`) is a
  move-only RAII handle over the claimed slot; its destructor (or move-assignment over a live
  lease) does `word->fetch_or(bit, release)` then `waiters_->notify_one()` - the release publishes
  the holder's writes to `T` before the bit shows free again, and the notify is a cheap
  `fetch_add` with no syscall unless a thread is actually parked.

**Constructor set.** `ResourcePool` exposes five constructor overloads, all funneling into one
canonical form: `ResourcePool(n_pinned, n_free, spin_budget, factory)`
(`resource_pool.hpp-149`). The
shorter overloads default `n_pinned` to `0` (a free-only pool, the common case) and/or the spin
budget to a built-in constant. **`n_pinned` always precedes `n_free`** in every overload that takes
both - getting the order backwards silently builds a pool with the two counts swapped rather than
failing to compile, since both are plain `std::size_t`. The factory is a callable invoked once per
live slot, either `(std::size_t index) -> T` or `() -> T`
(the `ResourceFactory` concept, `resource_pool.hpp-30`),
so `T` need not be default-constructible - real resources (sockets, file descriptors, connections)
get real per-slot construction arguments. If the factory throws while building slot `k`, the
constructor destroys the `[0, k)` slots already built and rethrows: no partially-built pool, no
leak.

**Why `spin_budget` is `std::chrono::nanoseconds` only.** `acquire_for(timeout)` computes its
deadline once (`t0 + timeout`), then spins with `pause_arc_agnostic()` until
`min(t0 + spin_budget, deadline)`, then parks on the `EventCount` until the deadline
(`resource_pool.hpp-235`). Both
`spin_budget` and `timeout` are typed as `std::chrono::nanoseconds`, never a bare integer: a
constructor overload set that accepted `(n_free, std::size_t, factory)` for one shape and
`(n_free, nanoseconds, factory)` for another would let an integer literal silently bind to the
wrong parameter and misconstruct the pool (swapping which count is which, or which argument is the
spin budget) with no compiler error. Using `chrono::nanoseconds` exclusively makes every overload's
third argument unambiguous at the call site and ill-formed if the caller passes a raw number.

## AsyncResourcePool

`AsyncResourcePool<T, MaxSize>`
is the coroutine-friendly sibling: the same inline storage, pinned region, and lock-free bitset
`try_acquire()` fast path as `ResourcePool` (the file is self-contained rather than sharing a base
with the sync pool - the storage/bitset/repair mechanics are duplicated on purpose so the sync pool
is never put at risk by async-only changes), but a caller that finds no free slot **suspends a
coroutine** instead of parking a thread. `try_acquire()` never suspends and is callable from
anywhere; `async_acquire_for(exec, timeout)` and `async_acquire(exec)` (unbounded) return
`boost::asio::awaitable<std::optional<AsyncLease<T>>>`, with `std::nullopt` meaning timed out,
cancelled, or the pool was shut down.

**Waiting.** Where the sync pool parks on an `EventCount`, the async pool registers a
`detail::WaiterNode` - a
plain struct living in the suspended coroutine's own frame, never heap-allocated by the pool - into
a mutex-guarded, doubly-linked, FIFO `detail::WaiterList`. The mutex is touched only on the slow
path (parking, waking, cancelling, draining); the bitset fast path stays lock-free throughout. Each
node carries a single `std::atomic<WaitState>` that arbitrates a three-way race: a release calling
`wake_one()`, a `steady_timer` firing on a bounded wait, or an asio cancellation slot firing.
Whichever of the three wins the `state` CAS (`parked -> {notified, timed_out, cancelled}`) unlinks
the node under the list mutex and posts the coroutine's resume - so the node is detached from the
list *before* its coroutine can run and potentially destroy the frame that holds it. An
`AsyncLease<T>`'s release path is `word->fetch_or(bit, release)` then `waiters_->wake_one()`
(structurally identical to `Lease<T>`, but waking a coroutine rather than a thread); resuming always
hops back onto the coroutine's own executor via `asio::post`, never runs on the releasing thread.

**Cancellation and shutdown.** A parked wait honors asio's per-operation cancellation slot (subject
to the cancellation type the coroutine's cancellation state actually delivers - see the class-level
warning in the header about `terminal` vs `total`). `shutdown()` drains every parked waiter to
`nullopt` and makes subsequent `async_acquire*` calls resolve to `nullopt` immediately rather than
parking; it does **not** touch the bitset, so `try_acquire()` still works against any bits still
free. The destructor asserts the waiter list is empty - `shutdown()` (and letting it drain) is a
caller responsibility, not something the destructor does for you.

**Timers.** `AsyncResourcePool` composes the park with a `boost::asio::steady_timer` via
`awaitable_operators::operator||` for the bounded variant
(`async_resource_pool.hpp-267`).
Because neither a `steady_timer` nor that composition is thread-safe, the class-level documentation
requires running each acquiring coroutine on its own strand whenever the driving `io_context` has
more than one thread; a single-threaded `io_context` (as used throughout the flagship benchmark's
`ShardedAsioBackend`, covered under Performance notes below) needs no strand.

## EventCount

`EventCount` is the wait primitive behind
`ResourcePool`'s free-region park phase: a lost-wakeup-safe park/notify built on a single
`alignas(64) std::atomic<std::uint64_t>` that packs an `epoch` in the high 32 bits and a
`waiter_count` in the low 32 bits. On Linux it drives a raw `FUTEX_WAIT_PRIVATE` /
`FUTEX_WAKE_PRIVATE` on the epoch half of that word directly via `syscall(SYS_futex, ...)`; every
other target (or with `EVENT_COUNT_FORCE_FALLBACK` defined) falls back to a
`std::mutex` + `std::condition_variable` pair behind the identical public API, so call sites never
need to `#ifdef`.

The contract is the classic prepare/recheck/wait dance: `prepare_wait()` registers the caller
(`fetch_add(1)`, seq_cst) and returns the current epoch as a wait key; the caller **must** re-check
its real condition (e.g. `try_acquire()`) before calling `wait_until(key, deadline)`, which parks
only while the epoch still matches `key`. A releaser calls `notify_one()`/`notify_all()`, which
bumps the epoch with a single `fetch_add` and only issues the futex wake syscall if the
pre-increment waiter count was non-zero - so a notify landing on an empty waiter set costs one
atomic op and zero syscalls. Reach for `EventCount` directly (rather than through `ResourcePool`)
when a bespoke blocking wait needs the same "cheap when uncontended, correct under a race" park/wake
shape without a full resource pool wrapped around it.

## Disruptor

`Disruptor<T, SequencerT, WaitStrategyT>` bundles
a runtime-sized power-of-two `RingBuffer<T>`
(one heap allocation at construction; indexing is `sequence & (buffer_size - 1)`, a single AND
instead of a modulo) with a sequencer that coordinates claim/publish/consume. A
`StaticRingBuffer<T, BufferSize>`
sibling exists for a compile-time-sized, stack-allocatable buffer with the identical indexing
scheme. Both the sequencer and the wait strategy are compile-time template parameters, stored by
value inside the `Disruptor`, so there is no virtual dispatch on the hot path; `AnyWaitStrategy`
(`wait_strategies/wait_strategy.hpp`)
opts back into one heap-allocated, virtually-dispatched strategy for callers who must choose at
runtime.

**Sequencers.** Two sequencer implementations satisfy the shared
`IsSequencer` concept:

- `MultiProducerSequencer`
  claims with a single atomic `fetch_add` on a shared cursor - every producer gets a unique
  sequence in one instruction, no CAS-retry loop under contention. Because fetch-add can run ahead
  of what has actually been published, availability is tracked per-slot by a *generation number*
  (`sequence >> index_shift`, not a boolean): a slot is available for `seq` iff it currently holds
  `generation_of(seq)`. This needs no consumer-side write-back ("mark consumed") - the availability
  flags flow one way, producer to consumer - while still being safe across ring wraps, because
  backpressure guarantees a slot's previous occupant is already consumed before it is overwritten
  with a new generation.
- `SingleProducerSequencer`
  is the SPSC fast path: because exactly one producer claims and publishes in order, the claimed
  counter is a plain producer-private `std::int64_t` (no atomic, no CAS), there are no gaps to
  track, and `get_highest_published()` is just the published cursor. It is a strictly narrower
  contract than the multi-producer sequencer - using it from more than one producer thread is
  undefined - in exchange for removing every atomic operation from the claim path.

**Wait strategies** (`disruptor/wait_strategies/`)
are the consumer-side complement: `BusySpinWaitStrategy` (tight `pause_arc_agnostic()` loop, lowest
latency, one core fully consumed), `YieldingWaitStrategy` (spin then `std::this_thread::yield()`
after a threshold), `BlockingWaitStrategy` (`condition_variable`, zero idle CPU, microsecond-scale
wake latency), and `TimeoutBlockingWaitStrategy` (blocking with a periodic timeout, for wake-and-
recheck shutdown polling). All four satisfy the same
`IsWaitStrategy` concept
(`wait_for(sequence, cursor)`, `signal()`, `signal_all()`), so swapping one for another is a
template-argument change, not a rewrite.

**Producer/consumer usage.** A producer claims with `sequencer().next()` (or `next_batch(n)` for a
single fetch-add covering several slots, or `try_next()` for a non-blocking best-effort claim),
writes `ring_buffer()[seq]`, then calls `sequencer().publish(seq)`. A consumer reads
`sequencer().get_highest_published(next_seq, sequencer().get_cursor())`, drains
`[next_seq, available]` in order, and calls `update_gating_sequence(available)` to advance the
backpressure watermark the producer's claim path waits on.

## Other primitives

**`ThreadPool`** (`thread_pool/thread_pool.hpp`,
`thread_pool.cpp`) is a priority-queue-backed
worker pool that grows between `min_threads` and `max_threads`
(`ThreadPoolConfig`, with
`minimal()`/`basic()`/`high_performance()`/`quick_cleanup()` presets): `enqueue(func, priority,
args...)` returns a `std::future`, spins up a new worker when the pool is not yet full and no
worker is idle, and a background cleanup thread periodically reaps workers that have exceeded
`idle_timeout`. Unlike the other primitives in this library, `ThreadPool` is a `STATIC` (not
`INTERFACE`/header-only) target, since it has an out-of-line `.cpp`.

**`ThreadSafeResource<T>`** (`thread_safe_resource/thread_safe_resource.hpp`)
is a small `std::shared_mutex` wrapper: `.read()` returns a `ReadProxy` holding a `shared_lock`,
`.write()` (and `operator->`) return a `WriteProxy` holding a `unique_lock`, and `with_lock` /
`with_read_lock` take a callable for scoped access. `ThreadPool` uses it internally to guard its
worker list and task queue. Construction mirrors `T`'s own two initialization forms rather than
collapsing them: the parenthesized form forwards to `T(args...)`, while the braced form goes
through `T`'s `initializer_list` constructor - so
`ThreadSafeResource<std::vector<int>>(5)` holds five elements and
`ThreadSafeResource<std::vector<int>>{5}` holds one.

**`AsioBackend` / `ShardedAsioBackend`** (`asio_backend/`)
are RAII `io_context` runners for asio-based code (used throughout the resource-pool benchmarks).
`AsioBackend` is one `io_context` driven by N `std::jthread`s, with an optional per-thread core-pin
hook run before `io_.run()` - the reason it hand-rolls the thread loop instead of using
`boost::asio::thread_pool`. `ShardedAsioBackend` instead runs N independent single-threaded
`io_context`s, one per shard: each shard's scheduler lock is touched only by its own runner thread
(plus cross-shard wake posts), so the shared-scheduler-lock contention of one `io_context` driven by
many threads disappears, and single-threaded contexts need no per-coroutine strand.

**`utils/`** holds two architecture-abstracted primitives every wait loop in this library depends
on: `pause_arc_agnostic()` (`_mm_pause()` on x86,
the `yield` instruction on AArch64) for spin loops that want to cede a pipeline slot without
yielding to the scheduler, and
`pin_current_thread_to_core(core)`
(`pthread_setaffinity_np`) for benchmarks and latency-sensitive producers/consumers that need
deterministic core placement.

## Performance notes

The `benchmarks/multithread/` tree has two independent subjects, each measuring a different thing:

**Disruptor** (`benchmarks/multithread/disruptor/disruptor_speed_test.cpp`)
is a standalone throughput harness (not a Google Benchmark subject) that times raw claim/publish/
consume loops: multi-producer-to-one-consumer with one-at-a-time versus batched claims, pinned
single-producer/single-consumer runs comparing `MultiProducerSequencer` against
`SingleProducerSequencer` on the identical loop, a producer-only publish-rate measurement isolating
`next()`/`publish()` from any consumer, and a consumer-batching sweep that varies how far the
consumer lets the producer get ahead before draining, to show throughput as a function of batch
size. The shape to look for, not a number to memorize: `SingleProducerSequencer` should out-run
`MultiProducerSequencer` on the same SPSC workload (it removes the claim-side CAS and the
per-slot availability buffer), and both sequencers' throughput should climb as the consumer-batching
sweep lets more entries drain per cache-line-crossing.

**ResourcePool / AsyncResourcePool** (`benchmarks/multithread/resource_pool/`)
is a Google Benchmark suite plus one standalone flagship binary:

- Per-subject binaries (`Try`, `AcqFor1us`/`2us`/`10us`, `Pinned` for the sync pool;
  `ArpAcqFor1us`/`2us`/`10us` for the async pool) sweep a fixed set of scenarios - steady load,
  bursty load, timeout pressure, an asio-`post()`-driven producer, a pinned-cell zero-contention
  floor, and a heavy SPMC-drain burst - across a range of worker counts
  (`common/bench_scenarios.hpp`),
  each timing only the acquire call itself (`try_acquire` / `acquire_for` / `async_acquire_for`)
  around a synthetic `MockResource::work_for(duration)` busy-wait timed via TSC.
- The **flagship** binary (`flagship/flagship_bench.cpp`)
  measures something the per-subject benches deliberately do not: **end-to-end** latency from when
  a disruptor-fed record is produced to when its processing actually starts, under a bursty or
  steady arrival pattern, comparing a sync thread-per-consumer configuration against several async
  coroutine-dispatch configurations (`Dispatch::Pull`, `PullDedicated`, `Channel` - see
  `flagship_config.hpp`).
  The reason this exists as a separate subject: an acquire-call-only measurement can make a
  saturated pool look artificially fast (an `acquire_for` that returns quickly because the caller
  already waited in an upstream queue is scored as if it had never waited at all), so the flagship
  instead times the whole record lifetime and reports p50/p95/p99/max plus achieved throughput and
  a "did the backlog actually clear before the next burst" check.

Both suites warn (without failing) about jitter sources before running - CPU governor not set to
`performance`, SMT active, deep C-states enabled, transparent hugepages disabled, high load average
(`common/pool_bench_main.hpp`)
- since a bounded-pool latency measurement at the hundreds-of-nanoseconds scale is easy to swamp
with unrelated scheduling noise. This page intentionally does not repeat exact latency or
throughput figures from either suite: they are a function of the machine, the kernel's CPU
frequency scaling policy, and which of the mitigations above are active, and go stale the moment
any of those change. Run the relevant binary (`--pin=0` for floating workers, `--pin=1` to pin each
worker to its own core) against the target machine to get current numbers.
