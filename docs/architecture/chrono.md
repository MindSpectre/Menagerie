# Chrono Library

The chrono library (`common/chrono/`) is Menagerie's timing toolkit: wall-clock formatting/parsing for two
locales (local time and UTC), a locale-independent HTTP-date renderer, a raw hardware tick counter for
latency sampling below `steady_clock`'s call overhead, two stopwatch flavors for ad hoc interval measurement,
and a deadline-bound function executor built on top of `ThreadPool`. Everything is reached through
`#include <menagerie/chrono>` (`export/menagerie/chrono`).

## Key types

- **`Clock`** -- base clock: `now()` returns a `system_clock`
  time point, `parse<Dur>(text, fmt)` parses a `strftime`-style string into one.
- **`LocalClock` / `UTCClock`** -- aliases of
  `SpecClock<ClockType::Local>` / `SpecClock<ClockType::UTC>`; add `current_time(fmt)`,
  `format_time(tp, fmt)`, `format_time_iso_ms(tp)` (millisecond-precision ISO 8601, with a
  `DoesStringHaveAppendMethod`-constrained overload that appends into a caller-supplied string instead of
  allocating a new one), and `to_tm(tp)` (`localtime_r` vs `gmtime_r` under the hood).
- **`clock_formats::eu_dmy_hms` / `us_mdy_hms` / `iso8601`** -- the
  canonical `strftime` pattern strings passed to `current_time(...)` / `format_time(...)`.
- **`format_imf_fixdate(t, out)` / `IMF_FIXDATE_LEN`** -- renders an
  RFC 9110 SS5.6.7 IMF-fixdate (e.g. an HTTP `Date` header) into a fixed 29-byte buffer; day/month names come
  from a fixed English table rather than `strftime("%a"/"%b")`, so the output never depends on the process's
  `LC_TIME` locale.
- **`TscClock`** -- raw hardware tick counter (`rdtscp` on x86,
  `cntvct_el0` on AArch64) for fine-grained sampling; `now()` reads a tick, `elapsed(since)` and
  `to_duration(ticks)` / `to_cycles(duration)` convert against a `cycles_per_ns()` calibration measured once
  against `steady_clock` and cached for the process lifetime.
- **`Stopwatch<Duration, Clock>`** -- a vector of time-point
  flags: `start()`, `add_flag()`, `stop()` (returns and clears the flag vector), `delta_t(i)` /
  `from_start(i)` / `from_prev(i)`, `average_delta()`, and the static `measure(func)` helper that times a
  single callable invocation.
- **`PrintingStopwatch<T>`** -- a named-flag stopwatch
  that prints its report to `std::cout` on `print()`/`finish()` and automatically in its destructor;
  `flag(name)` (or `operator++`/`operator--` for unnamed add/remove), `set_countdown_from_prev(bool)` and
  `set_countdown_from_start(bool)` control which deltas the report includes.
- **`Timer`** -- runs a callable with a deadline on an owned
  `ThreadPool`: `execute_polite_vanish(timeout, fn, args...)` requires a
  `std::shared_ptr<CancellationToken>` among `args...` and, on timeout, asks the callable to cooperatively
  stop; `execute_violent_kill(timeout, token, fn, args...)` runs `fn` on a raw `std::thread` and force-kills
  it on timeout (`pthread_cancel` / `TerminateThread`). Both return a `std::future` for the result.
- **`CancellationToken`** -- the cooperative-stop
  flag `execute_polite_vanish` watches: `cancel()`, `renew()`, `stop_requested()`.
- **`exponential_backoff(attempt, base, cap)`** -- `base *
  2^attempt` clamped to `cap`, with the shift itself clamped so `1u << shift` cannot overflow.
- **`sleep_for<Duration>(d)` / `async_sleep_for<Duration>(d)`**
  -- blocking (`std::this_thread::sleep_for`) and coroutine (`boost::asio::awaitable<void>`, backed by
  `steady_timer`) sleeps, constrained to `beavers::IsDuration`.

## Usage

```cpp
#include <menagerie/chrono>

using namespace menagerie::chrono;
using namespace std::literals;

Stopwatch<> stopwatch(20);
stopwatch.start();
sleep_for(10ms);
stopwatch.add_flag();
const auto flags = stopwatch.stop();  // 2 flags: start + add_flag

const auto duration = Stopwatch<>::measure([] { sleep_for(50ms); });

std::array<char, IMF_FIXDATE_LEN + 1> buf{};
const auto len = format_imf_fixdate(std::time(nullptr), std::span<char, IMF_FIXDATE_LEN + 1>{buf});

const auto now_iso = UTCClock::current_time(clock_formats::iso8601);
```

`Stopwatch<>`, `sleep_for`, and `Stopwatch<>::measure` above mirror
`tests/unit_tests/common/chrono/stopwatch/test_stopwatch.cpp`;
`format_imf_fixdate` / `IMF_FIXDATE_LEN` mirror
`tests/unit_tests/component/http/types/test_http_date.cpp`.

## Design notes

`Timer`'s two execution modes trade correctness for reach differently. `execute_polite_vanish` only ever asks
the worker to stop -- the watchdog thread flips a `CancellationToken` and returns, leaving the worker
responsible for noticing and unwinding, so it is safe with locks held but does nothing if the callable never
checks the token. `execute_violent_kill` guarantees the deadline is enforced from the outside
(`pthread_cancel`/`TerminateThread`) but is undefined behavior if the target thread holds a lock at
cancellation time; the header comments call the Windows path "dangerous!" and the POSIX path "UB if locks
held" outright. Reach for `execute_polite_vanish` unless the callable is known-uncooperative (e.g. calling
into a library with no cancellation point), since a violently killed thread can leave shared state
(mutexes, allocator internals) permanently corrupted for the rest of the process.
