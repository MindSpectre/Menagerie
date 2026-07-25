# Crow Library

The crow library (`common/crow/`) is Menagerie's asynchronous logging stack: a Disruptor-backed `Logger` that
buffers events from any number of producer threads into a lock-free ring buffer and dispatches them to a set of
sinks from one consumer thread, `LoggerProvider` (a mixin giving any class its own logger handle and prefix),
`ConsoleSink`/`FileSink` output destinations, and the `LOG_*`/`COMPONENT_LOG_*` macro API most call sites use
instead of touching `Logger` directly. Everything is reached through `#include <menagerie/crow>`
(`export/menagerie/crow`).

## Key types

- **`Logger`** -- owns a
  `Disruptor<LogEvent, MultiProducerSequencer, AnyWaitStrategy>` and a consumer `std::jthread`; `log(...)`
  (format-string or plain-message overloads) and `stream(...)` publish an event without blocking the caller,
  `add_sink(...)` registers a `Sink` on its own `asio::strand`, and `shutdown()` drains every pending event before
  returning.
- **`LoggerConfig::Builder`** -- configures
  `ring_buffer_size` (must be a power of two), `pool_size` (internal thread-pool size when `Logger` is
  constructed without an external executor), and `wait_strategy` (`BusySpin` / `Yielding` / `Blocking`).
- **`LoggerProvider`** -- base class giving a subclass
  `get_logger()` / `set_logger(...)` / `prefix()` / `set_prefix(...)`; the prefix is the class-name label the
  `LOG_*` macros render into every entry.
- **`ComponentLoggerManager`** -- lazily-initialized static
  singleton `Logger*` backing the `COMPONENT_LOG_*` macros for code that is not a `LoggerProvider`.
- **`Sink`** -- the non-templated base every sink
  implements (`process(event)`, `process_batch(...)`, `flush()`, `should_log(level, prefix)`), letting `Logger`
  hold heterogeneous sinks in one `vector<shared_ptr<Sink>>`.
- **`ConsoleSink<EntryType>`** /
  **`FileSink<EntryType>`** -- `ConsoleSink` writes ANSI-colored
  stdout output, `FileSink` writes rotating-file output; both are templated on which `EntryType` formats the
  event.
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
