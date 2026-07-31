# Crow Library

The crow library (`common/crow/`) is Menagerie's asynchronous logging stack: a Disruptor-backed `Logger` that
buffers events from any number of producer threads into a lock-free ring buffer and dispatches them to a set of
sinks from one consumer thread, `LoggerProvider` (a mixin giving any class its own logger handle and prefix),
`ConsoleSink`/`FileSink` output destinations, and the `LOG_*`/`COMPONENT_LOG_*` macro API most call sites use
instead of touching `Logger` directly. Everything is reached through `#include <menagerie/crow>`
(`export/menagerie/crow`).

## Key types

- **`Logger`** -- owns a
  `Disruptor<LogEvent, MultiProducerSequencer, AnyWaitStrategy>`, a consumer `std::jthread`, and (unless
  `health_check_interval` is zero) a janitor `std::jthread`; `log(...)` (format-string or plain-message
  overloads) and `stream(...)` publish an event without blocking the caller, `add_sink(...)`/`remove_sink(...)`
  register and unregister a `Sink` on its own `asio::strand`, `sink_report()` returns every registered sink's
  health, `sweep()` forces one janitor pass now, `set_error_callback(...)` replaces the handler invoked on sink
  lifecycle transitions, and `shutdown()` drains every pending event before returning.
- **`LoggerConfig::Builder`** -- configures
  `ring_buffer_size` (must be a power of two), `pool_size` (internal thread-pool size when `Logger` is
  constructed without an external executor; defaults to 2, since every sink is serialized on its own strand and
  parallelism beyond the sink count buys nothing), `wait_strategy` (`BusySpin` / `Yielding` / `Blocking`), and
  `health_check_interval` (how often the janitor sweeps sinks; default 1s, zero disables the janitor entirely).
- **`LoggerProvider`** -- base class giving a subclass
  `get_logger()` / `set_logger(...)` / `prefix()` / `set_prefix(...)`; the prefix is the class-name label the
  `LOG_*` macros render into every entry.
- **`ComponentLoggerManager`** -- lazily-initialized static
  singleton `Logger*` backing the `COMPONENT_LOG_*` macros for code that is not a `LoggerProvider`.
- **`Sink`** -- the non-templated base every sink
  implements (`process(event)`, `process_batch(...)`, `flush()`, `should_log(level, prefix)`, all `noexcept`),
  letting `Logger` hold heterogeneous sinks in one `vector<shared_ptr<Sink>>`. It also exposes `maintain()`
  (recovery/rotation, called by the janitor once a sink's backoff deadline has passed) and `get_status()` (the
  sink's current `SinkStatus`); a sink implementation may also offer its own way to force maintenance
  immediately, bypassing the backoff gate -- e.g. `FileSink::force_maintain()`. No `Sink` virtual ever throws --
  failures are status transitions, never exceptions.
- **`ConsoleSink<EntryType>`** /
  **`FileSink<EntryType>`** -- `ConsoleSink` writes ANSI-colored
  stdout output and never goes `Dead` (it owns no losable resource, only a stream that can be temporarily
  unusable); `FileSink` writes rotating-file output in one of four modes, set by `rotate_file()` and
  `add_time_to_filename()`: neither set writes `NAME.log` forever; only the timestamp flag fixes `NAME_TIME.log`
  at open; only the rotate flag indexes `NAME.log` -> `NAME_1.log` -> `NAME_2.log` ..., resuming the highest
  index on startup while it still has room; both set renames each rotation to a fresh `NAME_TIME.log`, folding a
  same-second rotation into the file already open instead of fanning out. Both sinks are templated on which
  `EntryType` formats the event.
- **`ConsoleSinkConfig::Builder`** /
  **`FileSinkConfig::Builder`** -- per-sink
  `threshold`, `prefix_filter`, and other per-sink options (`enable_colors`, `rotation`/`max_file_size`, ...).
- **`PrefixFilter`** -- per-sink allow/deny list keyed on the
  logging class's prefix, with `AllowAll` (default), `Allowlist`, and `Denylist` modes.
- **`DetailedEntry`** /
  **`LightEntry`** -- the two built-in `EntryType`s: `DetailedEntry`
  formats timestamp, source location, thread/process id, and prefix; `LightEntry` formats only the level and
  message.
- **`LogLevel`** -- `Trace` / `Debug` / `Info` / `Warning` /
  `Error` / `Fatal`, with the short aliases `TRC`/`DBG`/`INF`/`WRN`/`ERR`/`FAT`.
- **`LOG_TRC`/`LOG_DBG`/`LOG_INF`/`LOG_WRN`/`LOG_ERR`/`LOG_FAT`, `LOG_*_ONCE`, `COMPONENT_LOG_*`
  macros** -- the call-site logging API; stream form
  (`LOG_INF() << "..."`) or format-string form (`LOG_INF("User {} logged in", user)`).

## Usage

```cpp
#include <menagerie/crow>

using namespace menagerie::crow;

class Server : LoggerProvider {
public:
    Server() {
        auto logger = std::make_shared<Logger>();
        logger->add_sink(std::make_shared<ConsoleSink<DetailedEntry>>(
            ConsoleSinkConfig::Builder{}.threshold(LogLevel::Debug).enable_colors(true).finalize()));
        set_logger(std::move(logger));
        set_prefix("Server");
    }

    void handle_request() {
        LOG_INF() << "handling request";
        LOG_DBG("elapsed={}ms", 12);
    }

    ~Server() {
        get_logger()->shutdown();
    }
};
```

## Design notes

The `LOG_*`/`COMPONENT_LOG_*` macros are compiled out entirely, not just filtered at runtime: without
`ENABLE_LOGGING` (or `COMPONENT_LOGGING` for the `COMPONENT_LOG_*` family) defined, each macro expands to
`menagerie::crow::DummyStream{}` -- a class whose `operator<<` is a no-op returning `*this` -- so a call site pays
literally zero cost (no branch, no function call) in builds where logging is disabled, while the same source
still compiles either way. `Logger::log(...)` and `stream(...)` never block the caller: publishing to the
Disruptor is the only work done on the producer's thread, and formatting, filtering, and I/O all happen on the
single consumer thread that drains the ring buffer.

**Sink health.** Every sink carries a `SinkStatus`: `Healthy` (accepting and writing), `Degraded` (still writing,
but a maintenance action such as rotation is failing), or `Dead` (not writing; events go undelivered until
recovery succeeds). Failures are status transitions, never exceptions. The Logger never evicts a failing sink on
its own -- `remove_sink(...)` is the only way a sink stops being dispatched to -- and events skipped because a
sink is `Dead` are counted in that sink's `undelivered()`. That count is an upper bound, not an exact figure: it
is incremented per batch the dispatcher would otherwise have posted to the sink, not per event filtered by the
sink's own prefix filter -- though a batch the sink's threshold could never have accepted in the first place is
skipped before dispatch and never counted at all. The janitor calls `maintain()` on each non-`Healthy`
sink whose backoff deadline has passed: the first retry after a failure is immediate, then the deadline doubles
per consecutive failure (1s, 2s, 4s, ...) capped at 60s. Lifecycle transitions are reported through
`set_error_callback(...)`, whose default handler re-logs the transition through this Logger when some registered
sink would actually accept it, falling back to `stderr` when none would.

**Thread model.** A `Logger` runs one consumer thread and, unless `health_check_interval` is zero, one janitor
thread. Constructed without an external executor, it additionally owns `pool_size` executor threads (default 2)
running the sinks' strands; constructed with an external executor, it starts none of its own and shares whatever
threads that executor already runs.
