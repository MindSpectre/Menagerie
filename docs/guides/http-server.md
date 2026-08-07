# Building an HTTP Server

This guide walks through building an HTTP server with `menagerie::http`, from a single-file minimal
server to routing, configuration, error handling, and shutdown. By the end you will have a server that
listens on a TCP port, dispatches to controller methods through a route table, converts typed errors to
HTTP responses automatically, and shuts down cleanly on SIGINT/SIGTERM. Every snippet below is either
excerpted verbatim from `examples/http/minimal_http_server.cpp` and the HTTP test suite under `tests/`,
or was compiled with `clang++ -fsyntax-only` against the real headers before being written down here -
nothing here is invented API.

For the internals (request lifecycle, memory model, threading model), see
`docs/architecture/http.md`.

## Minimal server

The full working example lives at `examples/http/minimal_http_server.cpp`. Build and run it:

```bash
$ cmake --preset debug -DBUILD_EXAMPLES=ON
$ cmake --build --preset debug --target Menagerie.Examples.Http.MinimalServer
$ ./build/debug/examples/http/Menagerie.Examples.Http.MinimalServer examples/http/server.json
```

Then curl it:

```bash
$ curl -s -i http://127.0.0.1:8080/api/hello/world
HTTP/1.1 200 OK
Content-Type: text/plain
X-Example: minimal-http-server
Date: Fri, 24 Jul 2026 23:49:54 GMT
Server: Menagerie
Content-Length: 13

hello, world
```

Four pieces make this work: the umbrella include, a controller, a config, and a run call.

**1. Include the umbrella header.** One `#include` brings in every public HTTP type:

```cpp
#include <menagerie/http>

namespace http = menagerie::http;
```

**2. Define a controller.** Subclass `HttpController` and register routes in `configure_routes()`; each
handler returns `AsyncResponse` (a plain coroutine) or `AsyncOutcome<Response, Errors...>` (a coroutine
that can also fail with a typed error):

```cpp
class GreeterController final : public http::HttpController {
public:
    void configure_routes() override {
        Get("/hello/{name}", &GreeterController::hello);
        Get("/healthz", &GreeterController::healthz);
        Post("/echo", &GreeterController::echo);
    }

private:
    static http::AsyncResponse hello(http::RequestContext ctx) {
        co_return ctx.ok("hello, " + ctx.path_param_or<std::string>("name", std::string{"world"}) + "\n");
    }

    static http::AsyncResponse healthz(http::RequestContext ctx) {
        co_return ctx.json(R"({"status":"ok"})");
    }

    static http::AsyncOutcome<http::Response, http::BodyLimitExceeded> echo(http::RequestContext ctx) {
        auto body = co_await ctx.body().read_to_string(64 * 1024);
        if (body.is_error()) {
            co_return menagerie::beavers::err(body.error<http::BodyLimitExceeded>());
        }
        co_return ctx.ok(std::move(body).value());
    }
};
```

**3. Load or build a config.** `ServerConfig::Builder{}.finalize()` gives you defaults; a JSON file goes
through `load_server_config`, which returns an `Outcome` rather than throwing on a malformed file:

```cpp
http::ServerConfig cfg = http::ServerConfig::Builder{}.finalize();
if (argc > 1) {
    auto loaded = http::load_server_config(argv[1]);
    if (loaded.is_error()) {
        return report_config_error(loaded);
    }
    cfg = std::move(loaded).value();
}
```

**4. Run it.** `run_standalone` owns the `io_context`(s), the worker threads, and the SIGINT/SIGTERM ->
`stop()` wiring for the common "HTTP owns the process" case. Its callback is where you attach listeners,
mount controllers, and attach middleware:

```cpp
http::run_standalone(std::move(cfg), threads, [](http::Server& server) {
    http::attach_default_listeners(server);
    if (server.listeners().empty()) {
        server.add_tcp_listener("127.0.0.1", 8080, http::Http11Driver{http::Http11Config{}});
    }

    auto greeter = std::make_shared<GreeterController>();
    greeter->add_middleware(server_tag_middleware());
    server.in_group("/api").add_controller(std::move(greeter));
});
```

`attach_default_listeners` wires up whatever `listeners` the config declares; the `if
(server.listeners().empty())` fallback is why the same binary serves both with and without a config
file argument. `server_tag_middleware()` is a small post-processing middleware defined next to `main()`
in the example (same shape as `tag_middleware()` in Routing below: it calls `next`, then mutates the
returned response) - it is what stamps the `X-Example: minimal-http-server` header seen in the curl
transcript above.

## Routing

**Verbs.** `HttpController` exposes all seven verb methods - `Get`, `Post`, `Put`, `Patch`, `Delete`,
`Head`, `Options` - each accepting a member-function pointer, a free function, or a lambda:

```cpp
class ApiController final : public http::HttpController {
public:
    void configure_routes() override {
        Get("/users/{id}", &ApiController::user);
        Post("/users", &ApiController::create_user);
        Put("/items/{id}", &ApiController::put_item);
        Delete("/items/{id}", &ApiController::delete_item);
    }
    // ...
};
```

**Path and query parameters.** A `{name}` segment in the route template captures a path parameter;
`ctx.path_param<T>(name)` converts it and returns `nullopt` on a missing or unconvertible value,
`path_param_or` supplies a fallback instead. Query parameters work the same way through
`ctx.query<T>(name)` / `ctx.query_or<T>(name, fallback)`, parsed lazily from the query string on first
use:

```cpp
static http::AsyncResponse user(http::RequestContext ctx) {
    co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)) +
                     " v=" + ctx.query_or<std::string>("v", "none"));
}
```

`GET /users/42?v=hi` returns `user:42 v=hi` (`tests/integration_tests/component/http/test_http_tcp.cpp-47,104-109`).

**Groups.** `server.in_group(prefix)` mounts a controller under a path prefix; `GroupBinding::in_group`
nests further, and mounting a second controller in the same group chains off the first:

```cpp
root().in_group("/api").in_group("/v2").add_controller(std::make_shared<UsersController>());
```

`GET /api/v2/users` resolves; `GET /users` (unprefixed) does not
(`tests/unit_tests/component/http/routing/test_group.cpp-80`).

**Middleware.** A middleware is `std::function<AsyncResponse(RequestContext, const NextHandler&)>`,
attached with `controller.add_middleware(mw)` (first added runs outermost). It can short-circuit by
returning without calling `next`, enrich the context for downstream handlers with `ctx.set<T>(value)`,
or post-process the response after `co_await next(...)` returns:

```cpp
http::Middleware auth_middleware() {
    return [](http::RequestContext ctx, const http::NextHandler& next) -> http::AsyncResponse {
        if (!ctx.header("Authorization"))
            co_return http::ResponseFactory::unauthorized();
        co_return co_await next(std::move(ctx));
    };
}

http::Middleware tag_middleware() {
    return [](http::RequestContext ctx, const http::NextHandler& next) -> http::AsyncResponse {
        auto r = co_await next(std::move(ctx));
        r.add_header("X-Traced", "1");
        co_return r;
    };
}
```

This matches `tests/unit_tests/component/http/routing/test_middleware.cpp`: the short-circuit case is
`ShortCircuitSkipsHandler` (lines 80-90), the post-process case is `PostHandlerResponseMutation`
(lines 92-103).

## Configuration

`server.json` is the config used by the minimal example above:

```json
{
  "listeners": [
    { "bind": "127.0.0.1", "port": 8080, "transport": "tcp", "protocols": ["http1"] }
  ],
  "threads": 2,
  "timeouts": { "header_ms": 10000, "body_ms": 30000, "idle_ms": 60000 },
  "body_limit": 1048576,
  "request_arena_size": 8192,
  "drain_timeout_ms": 5000,
  "path_normalization": "collapse_trailing_slash"
}
```

Key by key:

- `listeners`: an array of `{ bind, port, transport, protocols }` (plus an optional `tls` block for
  `tls`/`quic` transports). `transport` is `"tcp"`, `"tls"`, or `"quic"`; `protocols` is an array of
  `"http1"` / `"http2"` / `"http3"` and, for a TLS listener, its order becomes the ALPN server-preference
  order.
- `threads`: worker thread count, consumed only by `run_standalone` (an injected-executor server takes
  its thread count from whoever drives the executors).
- `timeouts`: `header_ms` / `body_ms` / `idle_ms`, the three per-phase deadlines a connection is held to.
- `body_limit`: maximum request body size in bytes; an oversize body is rejected before the handler runs.
- `request_arena_size`: size in bytes of the per-connection arena that a request's headers, body view,
  and response are allocated from.
- `drain_timeout_ms`: how long graceful shutdown waits for in-flight requests to finish before
  force-cancelling stragglers.
- `path_normalization`: `"none"`, `"collapse_trailing_slash"` (default), or `"collapse_multi_slash"`.

Loading it is one call, returning an `Outcome` instead of throwing on a bad file:

```cpp
auto loaded = http::load_server_config("server.json");
if (loaded.is_error()) {
    // loaded.holds_error<http::ConfigFileError>() / ConfigParseError / ConfigSchemaError
    return 1;
}
http::ServerConfig cfg = std::move(loaded).value();
```

**TLS.** A `tls` listener needs a `TlsConfig`, built either from JSON (a `tls` block with `cert_file` /
`key_file`) or programmatically through its own builder:

```cpp
const auto tls = http::TlsConfig::Builder{}
                      .cert_file("/etc/ssl/cert.pem")
                      .key_file("/etc/ssl/key.pem")
                      .min_version(http::TlsConfig::MinVersion::tls13)
                      .require_client_cert(true)
                      .finalize();
```

(`tests/unit_tests/component/http/config/test_tls_config.cpp-18`). `min_version` is `"tls12"` (default) or
`"tls13"`; `key_passphrase` is a secret field read from JSON but never re-emitted when the config is
dumped back out. Once loaded, `attach_default_listeners(server)` reads `cfg.listeners()` and adds the
matching listener + driver for each entry - the same call regardless of how many listeners, or which
transports, the config declares.

## Error handling

A handler can fail two ways: a typed `Outcome` error, or a thrown exception.

**Typed errors.** A handler returning `AsyncOutcome<Response, E1, E2, ...>` gets each `Ei` converted to
a `Response` through an ADL-found `to_http_response(const Ei&)` - this is a compile-time contract, so a
handler whose error pack contains a type with no such overload fails to compile. The built-in error
types and their status codes:

| Error type | Status |
|---|---|
| `BadRequestError`, `JsonParseError`, `FormParseError`, `MultipartParseError` | 400 |
| `UnauthorizedError` | 401 |
| `ForbiddenError` | 403 |
| `NotFoundError` | 404 |
| `MethodNotAllowedError` | 405, with `Allow:` populated from the matched route's verbs |
| `ConflictError` | 409 |
| `PayloadTooLargeError`, `BodyLimitExceeded` | 413 |
| `UnprocessableEntityError` | 422 |

A handler never has to build the error response itself:

```cpp
static http::AsyncOutcome<http::Response, http::NotFoundError> get_thing(http::RequestContext ctx) {
    const auto id = ctx.path_param<std::string>("id").value_or("");
    if (id.empty()) {
        co_return menagerie::beavers::err(http::NotFoundError{"thing", id});
    }
    co_return ctx.ok("thing:" + id);
}
```

`component/http/types/errors/errors.hpp` has the full struct + `to_http_response` list; a user-defined
error type only needs its own `to_http_response` overload discoverable by ADL next to the type.

**Exceptions.** An exception that escapes a handler as a raw `throw` is not translated by routing - it
propagates up to the driver, which catches it and turns it into a 500, forcing the connection closed
(`keep_alive = false`) since the handler's partial state is unknown:

```cpp
static http::AsyncResponse boom(http::RequestContext) {
    throw std::runtime_error{"handler exploded"};
}
```

`GET /boom` here returns a 500 response, not a dropped connection
(`tests/integration_tests/component/http/test_http_tcp.cpp-57,132-136`). Routing misses use the same mechanism
under the hood: an unmatched path is a `NotFoundError`, a matched path with the wrong verb is a
`MethodNotAllowedError` - both collapse through `to_http_response` exactly like a handler's own typed
error would.

## Shutdown

`run_standalone` installs SIGINT/SIGTERM handlers once setup succeeds, so Ctrl+C is enough to trigger a
graceful stop of a standalone server. Driving `Server` directly, the sequence is `stop()` then
`wait_until_stopped()`:

```cpp
server.stop();
server.wait_until_stopped();
```

`stop()` is non-blocking, idempotent, and callable from any executor thread, including from inside a
request handler or observer; calling it twice, or after shutdown has already completed, is a documented
no-op (`tests/integration_tests/component/http/test_http_server_lifecycle.cpp-85`). `wait_until_stopped()`
blocks the calling thread until graceful shutdown finishes; a caller already running on the executor
being drained must use the awaitable `async_wait_stopped()` instead, or it would deadlock the shutdown
it is waiting on.

Graceful shutdown guarantees, in order: every accept loop is cancelled first, so no new connection is
accepted once `stop()` has been called; in-flight requests are then drained until `drain_timeout`
elapses, at which point any still-running request is force-cancelled. A slow handler that is already in
flight when `stop()` is called still gets to finish and its response still reaches the client, as long as
it completes within the drain timeout:

```cpp
server_->stop();
server_->wait_until_stopped();
const auto res = client.read_response();
EXPECT_EQ(res.result_int(), 200u);
EXPECT_EQ(res.body(), "slow done");
```

(`tests/integration_tests/component/http/test_http_server_lifecycle.cpp-159`, `GracefulShutdownCompletesInFlightRequests`).
A handler that ignores cancellation past the drain deadline delays shutdown rather than corrupting it -
`wait_until_stopped()` does not return until every force-cancelled connection has actually unwound, not
just been told to.

`Server` never stops the executor(s) it was handed - only `run_standalone`, which owns them outright,
does that as part of its own teardown. Whether you use `run_standalone` or drive your own executor(s),
the rule is the same: keep driving every executor until `wait_until_stopped()` / `async_wait_stopped()`
returns before stopping or destroying it.
