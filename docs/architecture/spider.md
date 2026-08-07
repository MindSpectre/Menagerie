# Spider Library

The spider library (`common/spider/`) is Menagerie's thread-safe service locator: register a factory or an
existing instance under a type (optionally tagged with a numeric ID for several instances of the same type),
and `get<T>()` lazily constructs it once and hands back a `std::shared_ptr`. A configurable lifetime policy
per registration controls whether an instance is reused forever, can be explicitly reset, or is reclaimed
automatically by a background janitor thread once it is idle or unreferenced. Menagerie's HTTP layer uses it
to hand `Server` and `HttpController` subclasses their default lifetime. Everything is reached through
`#include <menagerie/spider>` (`export/menagerie/spider`).

## Key types

- **`Spider`** -- the registry itself.
  `register_singleton<T>(factory | shared_ptr | value, lt = ...)` registers under ID 0;
  `register_instance<T>(..., id, lt = ...)` registers under an explicit `spider_id_t`. `get<Interface>(id =
  0)` returns the (lazily constructed) instance or throws `std::runtime_error` if nothing is registered.
  `reset<T>(id = 0)` forces recreation on the next `get` (`Resettable` lifetime only, throws otherwise).
  `has<T>(id)`, `size()`, `clear()`, and `set_sweep_interval(seconds)` (default 2 seconds) round out the API.
- **`instance()`** -- Meyers-singleton accessor for a process-wide
  `Spider&`, constructed on first call.
- **`Lifetime`** -- `std::variant<Resettable, Scoped, Timed,
  Immortal>`:
  - `Resettable{}` -- the implicit default when a type declares no policy; `reset<T>()` drops it so the next
    `get<T>()` rebuilds it.
  - `Immortal{}` -- never reset; `reset<T>()` throws `std::runtime_error`.
  - `Scoped{}` -- swept once the Spider's own copy is the only surviving `shared_ptr` (the janitor checks
    `use_count() == 1`); registering a *value* (not a factory) under `Scoped` risks an immediate sweep if
    nothing else holds a reference by the first janitor cycle.
  - `Timed{idle = 60s}` -- released after `idle` has passed since the last `get<T>()` on it (factory kept, so
    the next `get<T>()` rebuilds it); every `get<T>()` renews the lease.
- **`SPIDER_WEB(Policy)`** -- macro expanding to a `static constexpr
  Policy spider_policy` member declaration, letting a class state its own default `Lifetime` at the
  registration site instead of every caller passing one.
- **`get_spider_policy<T>()` / `has_spider_policy<T>`** -- the
  default-argument machinery behind `register_singleton`/`register_instance`'s `lt` parameter: `T::spider_policy`
  if `SPIDER_WEB` declared one, `Resettable{}` otherwise.
- **`spider_id_t`** -- `std::uint32_t` identifier
  distinguishing multiple registered instances of the same `T` (default ID `0`).

## Usage

```cpp
#include <menagerie/spider>

using namespace menagerie::spider;

struct DatabaseService {
    int connections = 5;
};

Spider spider;
spider.register_singleton<DatabaseService>([] { return std::make_shared<DatabaseService>(); });

const auto db = spider.get<DatabaseService>();

// A class can declare its own default lifetime instead of passing `lt` at
// every registration site:
class HttpController {
public:
    SPIDER_WEB(Resettable);
    // ...
};
```

`spider.register_singleton<DatabaseService>(...)` / `spider.get<DatabaseService>()` mirror
`tests/unit_tests/common/spider/test_spider.cpp`;
`SPIDER_WEB(spider::Resettable)` is the actual declaration in
`components/http/routing/controller/controller.hpp`,
and `SPIDER_WEB(spider::Immortal)` the one in
`components/http/server/server/server.hpp`.

## Design notes

`get<T>()` uses per-slot rather than whole-registry locking to keep unrelated types from serializing each
other's construction: the fast path takes a `shared_lock` and returns immediately if the object already
exists; the slow path takes only that slot's own `construction_mutex` (not a registry-wide write lock) before
calling the factory, so concurrent first-time `get<T>()` calls for two different types never block each
other. The factory itself runs entirely outside any `Spider` lock -- deliberately, so a factory that calls
`spider.get<Dependency>()` to resolve its own dependencies cannot deadlock against the lock its own
registration is holding.
