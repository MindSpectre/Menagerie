# Menagerie

Menagerie is a C++23 library and application suite for Linux (clang + libc++; macOS and
Windows presets exist but are experimental). It provides an HTTP/1.1 server stack, a
layered PostgreSQL client, and a set of header-mostly foundation libraries covering
logging, concurrency, serialization, terminal formatting, and timing. Every component is
built through CMake presets, with dependencies resolved via vcpkg.

## Components

| Namespace | Directory | Description | Docs |
| --- | --- | --- | --- |
| `albatross` | `component/http-albatross/` | Server-side HTTP/1.1 stack over TCP and TLS; HTTP/2 and HTTP/3 exist as compiling scaffolds. | [docs/architecture/albatross.md](docs/architecture/albatross.md) |
| `savanna` | `component/database-savanna/` | Layered PostgreSQL client with a provider-agnostic, compile-time-checkable query core. | [docs/architecture/savanna.md](docs/architecture/savanna.md) |
| `starling` | `common/concurrency-starling/` | Concurrency primitives: resource pools, a futex-based park/notify primitive, a lock-free ring buffer, a growable thread pool. | [docs/architecture/starling.md](docs/architecture/starling.md) |
| `beaver` | `common/core-beaver/` | Foundation layer: a typed result type, fixed-capacity strings, class-trait mixins, meta-programming utilities. | [docs/architecture/beaver.md](docs/architecture/beaver.md) |
| `crow` | `common/logger-crow/` | Asynchronous logging stack: a Disruptor-backed ring buffer dispatching to console and file sinks. | [docs/architecture/crow.md](docs/architecture/crow.md) |
| `chameleon` | `common/render-chameleon/` | Terminal text-formatting toolkit: ANSI colors, box-drawing glyphs, and Box/Section/Table renderers. | [docs/architecture/chameleon.md](docs/architecture/chameleon.md) |
| `cuckoo` | `common/chrono-cuckoo/` | Timing toolkit: wall-clock formatting, HTTP-date rendering, a hardware tick counter, stopwatches, deadline-bound execution. | [docs/architecture/cuckoo.md](docs/architecture/cuckoo.md) |
| `pangolin` | `common/serialization-pangolin/` | Field-descriptor serialization framework behind every Builder-pattern config type in the codebase. | [docs/architecture/pangolin.md](docs/architecture/pangolin.md) |
| `spider` | `common/locator-spider/` | Thread-safe service locator with configurable per-registration instance lifetimes. | [docs/architecture/spider.md](docs/architecture/spider.md) |
| `pufferfish` | `common/crypto-pufferfish/` | OpenSSL wrapper for password/data hashing: HMAC-SHA256, PBKDF2-HMAC-SHA256, secure salt generation. | [docs/architecture/pufferfish.md](docs/architecture/pufferfish.md) |
| `rabbit` | `common/math-rabbit/` | Random-value toolkit built on `std::mt19937`: integers, durations/dates, collection sampling. | [docs/architecture/rabbit.md](docs/architecture/rabbit.md) |
| `bowerbird` | `common/algorithms-bowerbird/` | Bounded-memory streaming sort over batched data (`SlidingWindowSorter`). | [docs/architecture/bowerbird.md](docs/architecture/bowerbird.md) |

## Naming

Every module directory is `<role>-<creature>`: the role word says what it does, the
creature is the namespace. Directories sort by function, so `ls common/` reads as a
table of contents, while code keeps the short, greppable token -- `crow::Logger`, not
`logger::Logger`. A habitat stands in for the creature in one case only: a module that
fronts interchangeable providers behind a provider-agnostic core, which is why the
PostgreSQL client is `database-savanna/` -- a savanna hosts many species, and Postgres is one
provider rather than the whole component. Providers are then the creatures living in it:
`providers/postgresql-elephant/` is `menagerie::savanna::elephant`.

The theme stops at the module boundary. Anything naming an external standard or product
keeps its own name, because those are already unique and universally recognized -- hence
`Providers::PostgreSQL`, `PostgresDialect`, `Http11Driver`, and the `postgres_*.hpp`
sources are untouched by any of the above.

If you prefer conventional names in your own code, define
`MENAGERIE_CONVENTIONAL_ALIASES` and each umbrella header additionally exposes the role
word as a namespace alias:

```cpp
#define MENAGERIE_CONVENTIONAL_ALIASES
#include <menagerie/crow>

menagerie::logger::Logger log;   // same entity as menagerie::crow::Logger
```

The aliases are opt-in and purely additive: library sources always use the creature
namespaces, so enabling the macro changes no symbol names, no ABI, and cannot cause an
ODR mismatch between translation units that set it and ones that do not. You can qualify
through an alias but not reopen or forward-declare through it -- `namespace
menagerie::logger { class X; }` is not valid; use `menagerie::crow` for that. Two aliases
are worth a second thought before enabling: `menagerie::chrono` sits next to `std::chrono`
and `menagerie::http` next to `boost::beast::http`, so an unqualified `chrono::` or
`http::` in a file that sees both becomes ambiguous.


## Quick example

A minimal HTTP server, trimmed from `examples/http-albatross/minimal_http_server.cpp`:

```cpp
#include <menagerie/albatross>

namespace http = menagerie::albatross;

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

On a machine with nothing installed yet, this installs the system dependencies, clones
the repository, and produces a first build:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/MindSpectre/Menagerie/main/scripts/install-linux.sh)
```

In an existing checkout, configure and build directly - vcpkg is provisioned on the
first configure, so there is nothing to set up beforehand:

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
