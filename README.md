# Menagerie

Menagerie is a C++23 library and application suite for Linux (clang + libc++; macOS and
Windows presets exist but are experimental). It provides an HTTP/1.1 server stack, a
layered PostgreSQL client, and a set of header-mostly foundation libraries covering
logging, concurrency, serialization, terminal formatting, and timing. Every component is
built through CMake presets, with dependencies resolved via vcpkg.

## Components

| Component | Description | Docs |
| --- | --- | --- |
| `http` | Server-side HTTP/1.1 stack over TCP and TLS; HTTP/2 and HTTP/3 exist as compiling scaffolds. | [docs/architecture/http.md](docs/architecture/http.md) |
| `database` | Layered PostgreSQL client with a provider-agnostic, compile-time-checkable query core. | [docs/architecture/database.md](docs/architecture/database.md) |
| `multithread` | Concurrency primitives: resource pools, a futex-based park/notify primitive, a lock-free ring buffer, a growable thread pool. | [docs/architecture/multithread.md](docs/architecture/multithread.md) |
| `beavers` | Foundation layer: a typed result type, fixed-capacity strings, class-trait mixins, meta-programming utilities. | [docs/architecture/beavers.md](docs/architecture/beavers.md) |
| `crow` | Asynchronous logging stack: a Disruptor-backed ring buffer dispatching to console and file sinks. | [docs/architecture/crow.md](docs/architecture/crow.md) |
| `chameleon` | Terminal text-formatting toolkit: ANSI colors, box-drawing glyphs, and Box/Section/Table renderers. | [docs/architecture/chameleon.md](docs/architecture/chameleon.md) |
| `chrono` | Timing toolkit: wall-clock formatting, HTTP-date rendering, a hardware tick counter, stopwatches, deadline-bound execution. | [docs/architecture/chrono.md](docs/architecture/chrono.md) |
| `serialization` | Field-descriptor serialization framework behind every Builder-pattern config type in the codebase. | [docs/architecture/serialization.md](docs/architecture/serialization.md) |
| `spider` | Thread-safe service locator with configurable per-registration instance lifetimes. | [docs/architecture/spider.md](docs/architecture/spider.md) |
| `crypto` | OpenSSL wrapper for password/data hashing: HMAC-SHA256, PBKDF2-HMAC-SHA256, secure salt generation. | [docs/architecture/crypto.md](docs/architecture/crypto.md) |
| `math` | Random-value toolkit built on `std::mt19937`: integers, durations/dates, collection sampling. | [docs/architecture/math.md](docs/architecture/math.md) |
| `algorithms` | Bounded-memory streaming sort over batched data (`SlidingWindowSorter`). | [docs/architecture/algorithms.md](docs/architecture/algorithms.md) |

## Quick example

A minimal HTTP server, trimmed from `examples/http/minimal_http_server.cpp`:

```cpp
#include <menagerie/http>

namespace http = menagerie::http;

class GreeterController final : public http::HttpController {
public:
    void configure_routes() override {
        Get("/hello/{name}", &GreeterController::hello);
        Get("/healthz", &GreeterController::healthz);
    }

private:
    static http::AsyncResponse hello(http::RequestContext ctx) {
        co_return ctx.ok("hello, " + ctx.path_param_or<std::string>("name", std::string{"world"}) + "\n");
    }

    static http::AsyncResponse healthz(http::RequestContext ctx) {
        co_return ctx.json(R"({"status":"ok"})");
    }
};

// in main():
http::ServerConfig cfg = http::ServerConfig::Builder{}.finalize();
http::run_standalone(std::move(cfg), threads, [](http::Server& server) {
    server.add_tcp_listener("127.0.0.1", 8080, http::Http11Driver{http::Http11Config{}});

    auto greeter = std::make_shared<GreeterController>();
    server.in_group("/api").add_controller(std::move(greeter));
});
```

See [docs/guides/http-server.md](docs/guides/http-server.md) for routing, configuration,
error handling, and shutdown.

## Building

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

See [docs/guides/getting-started.md](docs/guides/getting-started.md) for prerequisites,
the vcpkg setup, and the full list of presets and build options.

## Documentation

- `docs/architecture/` - one page per library/component: what it does and how it is built.
- `docs/guides/` - task-oriented walkthroughs, starting with getting-started.md.
- `docs/roadmap.md` - work that is still planned.

The Doxygen API reference indexes this README alongside `docs/architecture/` and
`docs/guides/`:

```bash
cmake --preset debug -DBUILD_DOCS=ON
cmake --build build/debug --target docs
```

The generated site lands at `build/docs/html/index.html`.

## Roadmap

See [docs/roadmap.md](docs/roadmap.md) for planned HTTP, database, and crypto work.

## License

MIT - see [LICENSE](LICENSE).
