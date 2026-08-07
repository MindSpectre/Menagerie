# HTTP Component

The HTTP component (`component/http/`) is Menagerie's server-side HTTP stack. Today it serves HTTP/1.1 over
plain TCP and over TLS; HTTP/2 and HTTP/3 exist as compiling scaffolds (ALPN negotiation and a QUIC listener are
already wired up, but their driver `serve()` bodies log a warning and close the connection) so the protocol work
can land later without touching the surrounding architecture. Three goals show up in every layer: **one-way
layering**, so application code depends on routing, routing depends on HTTP semantics, and neither the transport
nor the protocol drivers know anything about controllers; a **zero-additional-allocation hot path**, where a
successful request reuses a per-connection arena and the only unavoidable heap allocation is the user's own
response-body bytes; and an **explicit, typed error model** built on `beavers::Outcome<Response, Errors...>`, so
a handler's failure modes are part of its signature instead of an exception thrown into the void.

## Layer map

The tree is organized as eight directories under `component/http/`, each a separate static library joined by the
umbrella export. Dependencies point strictly downward; nothing below `server` knows about controllers, and
nothing below `drivers` knows about a byte stream's protocol.

- **`types/`**: the protocol-agnostic HTTP vocabulary: `Request`, `Response`,
  `Headers`, `Body`, `RequestContext`, the built-in error types with their `to_http_response` ADL overloads,
  the `AsyncOutcome`/`AsyncResponse` coroutine aliases, `http_enums.hpp` (`HttpMethod`, `HttpStatus`,
  `HttpVersion`, `Protocol`), and `executor.hpp` (the concrete `Executor`/`Strand` aliases everything else is
  typed on). Depends on nothing else in the component: it is the base of the tree.
- **`routing/`**: the application-facing route table: `RouteRegistry` (exact +
  parametric matching, baked at startup), `HttpController` (the verb DSL controllers subclass),
  `GroupBinding` (prefix-scoped mounting), `Middleware`, and `Router` (the dispatch facade the drivers call).
  Depends only on `types`.
- **`connection/`**: one peer's byte stream and lifecycle: `RequestArena`
  (the per-connection arena), `TcpConnection`, `TlsConnection`, the scaffold `QuicConnection`, and the
  `IsConnection`/`IsStreamConnection` concepts. Depends only on `types`.
- **`drivers/`**: one protocol's wire format, each implemented as a
  `serve(connection, Router&)` coroutine: `Http11Driver` (implemented, using Boost.Beast for parsing and a
  hand-rolled serializer for writes), `Http2Driver` and `Http3Driver` (scaffolds). Depends on `types`, `connection` (drivers
  template `serve()` on the connection type via `IsStreamConnection`/`IsConnection`), and `routing`
  (`Router::dispatch`).
- **`listeners/`**: accepts connections and pairs them with a driver:
  `ListenerBase` (the one virtual seam in the runtime path), `TcpListener<Driver>`, `TlsListener<Drivers...>`
  (ALPN-multiplexed), the scaffold `QuicListener<Http3Driver>`, `ConnectionTracker` (in-flight bookkeeping and
  the deadline sweep), and `build_ssl_context`. Depends on `connection`, `drivers`, `routing` (it forwards
  `Router&` into `driver.serve()`), and `config` (`TlsConfig`).
- **`config/`**: the JSON-loadable configuration surface: `ServerConfig`,
  `ListenerConfig`, `TlsConfig`, `Timeouts`, and `load_server_config`/`dump_server_config`. Depends on `types`
  only (`Protocol`'s string codec, and a forward-declared `Response` for its own `to_http_response` overloads);
  everything else is `serialization::ConfigInterface` plumbing shared with the rest of the project.
- **`server/`**: the orchestrator: `Server` (owns the route registry, the
  listeners, the observer list; drives setup, stop, and graceful shutdown), `ServerObserver`, `run_standalone`
  (owns an `io_context` + worker threads for the "HTTP owns the process" case), and `attach_default_listeners`
  (config-driven listener/driver wiring). Depends on every other layer.
- **`export/menagerie/http`**: the umbrella header consumers
  `#include`. No logic of its own; it aggregates the public leaf headers of all seven layers above. Consumers
  link `Menagerie::Component::Http` and write `#include <menagerie/http>`.

```text
+-----------------------------------------------------------------------+
|  export        menagerie/http umbrella header                         |
+-----------------------------------------------------------------------+
|  server        Server, ServerObserver, run_standalone,                |
|                 attach_default_listeners                              |
+-----------------------------------------------------------------------+
|  listeners     ListenerBase, TcpListener, TlsListener, QuicListener,  |
|                 ConnectionTracker, build_ssl_context                  |
+----------------------------+------------------------------------------+
|  drivers                   |  config                                  |
|  Http11Driver,             |  ServerConfig, ListenerConfig,           |
|  Http2Driver (scaffold),   |  TlsConfig, Timeouts,                    |
|  Http3Driver (scaffold)    |  load_server_config                      |
+-----------------+-----------------------------------------------------+
|  connection     |  routing                                            |
|  RequestArena,  |  RouteRegistry, HttpController,                     |
|  TcpConnection, |  GroupBinding, Middleware, Router                   |
|  TlsConnection  |                                                     |
+-----------------------------------------------------------------------+
|  types          Request, Response, Headers, Body, RequestContext,     |
|                  errors + to_http_response, ResponseFactory,          |
|                  AsyncOutcome, http_enums, Executor/Strand            |
+-----------------------------------------------------------------------+
```

`connection` and `routing` sit side by side: neither depends on the other, both depend only on `types`. `drivers`
merges them. `config` is independent of `drivers`/`connection`/`routing`: it only reaches down into `types` for
the `Protocol` string codec. `listeners` is the first layer that needs all three of `drivers`, `connection`, and
`config` together (a `TlsListener` builds its SSL context from `TlsConfig` and dispatches by ALPN to one of
several driver instances). `server` sits on top of everything.

## Request lifecycle

For an `Http11Driver` connection, one request's journey from accept to response write is:

1. **Accept.** `TcpListener::run()` (or `TlsListener::run()`) accepts a socket onto a strand bound to the next
   executor in round-robin order, sets `TCP_NODELAY`, and constructs a `TcpConnection`/`TlsConnection` as a
   `shared_ptr`, sized with `ServerConfig::request_arena_size()` (default 8 KiB). **This is where the
   `RequestArena` is created**: once, for the whole life of the connection, not per request
   (`request_arena.hpp`). The connection
   registers with the listener's `ConnectionTracker` (an RAII `Handle`) and `driver.serve(conn, router)` is
   `co_spawn`ed, detached, on that same strand. A `TlsListener` additionally performs the TLS handshake first
   (recording the ALPN-negotiated protocol) and only reaches a driver whose `id()` matches it.
2. **Reset.** At the top of every keep-alive iteration, `Http11Driver::serve` calls `conn.reset_request_arena()`
   - a bump-pointer rewind of the arena to its initial block, not a free/realloc.
3. **Parse.** A `beast::http::request_parser` is constructed with the arena allocator for both the string body
   and the field storage (`BeastFields = basic_fields<pmr::polymorphic_allocator<char>>`). The driver drives it
   manually via `parser.put()`, fed from a session-scope `flat_buffer`, hitting the socket only when the parser
   reports it needs more bytes: this replaces Beast's `async_read_header`/`async_read` composed operations,
   which the driver's comments measure at ~10k cycles/request against the raw-asio floor.
4. **Deadlines.** Per-phase timeouts are armed via `conn.set_deadline_after(...)`, a plain atomic store, not
   Beast's per-op `expires_after`; a periodic sweep enforces them (see Threading model below).
5. **Dispatch.** On a fully parsed message, `detail::build_request_context(req, conn.arena_alloc())`
   (`http11_driver.cpp`) wraps it into a
   `RequestContext`: `Headers::view_of_beast(req.base())` is a zero-copy view over Beast's already arena-backed
   field storage, and `Body::beast_view(...)` is a zero-copy span over the parsed body. An arena-bound
   `Response response{conn.arena_alloc()}` is constructed, then `co_await router.dispatch(std::move(ctx))` -
   `Router::dispatch` resolves the route via `RouteRegistry::find_route`, injects path parameters into the
   context, and invokes the frozen registry's baked `ContextHandler` (the composed middleware chain wrapping the
   controller method or lambda). The whole call is wrapped in `try { ... } catch (...)`; anything that escapes
   the Outcome system becomes `ResponseFactory::internal_error()` with `keep_alive` forced to `false`.
6. **Serialize.** `Http11Driver::serialize_response` flat-renders the `Response` (status line, headers - stamping
   `Date`/`Server` only if the handler did not set them - `Content-Length`, and `Connection: close` when
   `!keep_alive`) directly into a session-scope `std::string outbuf`, bypassing Beast's response serializer
   entirely. Because one socket read can pull in more than one pipelined request, the driver keeps appending
   serialized responses to `outbuf` and issues a single `async_write` only once the input buffer runs dry (or
   keep-alive ends, or the batch reaches 256 KiB): one write per *batch*, not per response.
7. **Loop or close.** If the connection stays keep-alive and more pipelined bytes remain, the loop repeats from
   step 2. Otherwise the driver flushes any remaining `outbuf` and calls `conn.async_close()` (a half-close
   shutdown; the socket itself closes when the connection object is destroyed).
8. **Arena teardown.** The arena's *contents* are invalidated by the next iteration's `reset_request_arena()`
   (step 2): not immediately after dispatch, because the response built in step 5 is still being read out of it
   during the write in step 6. The arena's backing heap block is freed only when the `TcpConnection`/
   `TlsConnection` object itself is destroyed at the end of the session.

## Error model

A handler returns either `AsyncResponse` (`awaitable<Response, Strand>`) or `AsyncOutcome<Response, Errors...>`
(`awaitable<beavers::Outcome<Response, Errors...>, Strand>`). The bake step in
`controller.hpp` wraps an Outcome-returning handler so
that after `co_await`ing it, `detail::collapse_outcome` calls `.visit()` on the result: the `Response`
alternative passes through unchanged, and any error alternative `E` converts via an ADL-found
`to_http_response(const E&)`. This is a compile-time contract, not a runtime one: `HasToHttpResponse<E>` is part
of `IsRouteHandler`, so a handler whose `Errors...` pack contains a type with no `to_http_response` overload
fails to compile, naming the offending type.

The built-in error types and their status mapping
(`errors.hpp` /
`errors.cpp`):

| Error type | Status |
|---|---|
| `BadRequestError`, `JsonParseError`, `FormParseError`, `MultipartParseError` | 400 |
| `UnauthorizedError` | 401 |
| `ForbiddenError` | 403 |
| `NotFoundError` | 404 |
| `MethodNotAllowedError` | 405, with `Allow:` populated from the matched path's registered verbs |
| `ConflictError` | 409 |
| `PayloadTooLargeError`, `BodyLimitExceeded` | 413 |
| `UnprocessableEntityError` | 422 |

All of these build their `Response` through the static, global-heap `ResponseFactory` rather than a request
arena: error responses are the cold path, so `to_http_response(const E&)` stays a clean one-argument free
function with no allocator to thread through every user override.

Routing misses use the same mechanism: `RouteRegistry::find_route` returns
`beavers::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError>`, and `Router::dispatch`
(`router.cpp`) converts a miss via the same
`to_http_response` overloads.

Handler exceptions - anything that escapes the Outcome system as a raw `throw` - are **not** translated by the
routing layer. `Router::dispatch`'s hookless fast path lets such an exception propagate out of
`router.dispatch()` unchanged; `Http11Driver::serve` is the layer that catches it (`catch (...)`) and turns it
into `ResponseFactory::internal_error()`, additionally forcing `keep_alive = false`: the connection is not kept
open after an exception, since the handler's partial state is unknown. When one or more `ServerObserver`s are
registered, `Router::dispatch_with_hooks` runs instead: it still catches the exception, fans it to every
observer's `on_unhandled_exception(exception_ptr)`, and then **rethrows** it, so the driver's catch-all still
produces the 500: observing an exception never suppresses it.

## Memory model

The invariant is: **one arena per connection, reused across every keep-alive request on it, reset (not
reallocated) between requests.** `RequestArena`
(`request_arena.hpp`) wraps a single heap
block of `ServerConfig::request_arena_size()` bytes (default 8192) in a `std::pmr::monotonic_buffer_resource`;
`reset()` calls `resource_.release()`, which rewinds the resource to that same block. A request that needs more
than the block holds grows via ordinary upstream `new`/`delete` blocks (still monotonic: never individually
freed until the whole resource is reset).

What allocates **from** the arena on a request:

- Beast's parser: both the string-body payload and the `BeastFields` field storage are templated on
  `std::pmr::polymorphic_allocator<char>` bound to the connection's arena.
- `RequestContext`'s path/query parameter `small_vector`s and its type-keyed middleware bag.
- The `Response` built via `ctx.ok()` / `ctx.json()` / `ctx.created()` / etc.: both its `Headers` and its
  *stored* allocator (`Response::alloc`) point at the arena, so even middleware that mutates the response after
  the handler returns (`auto r = co_await next(ctx); r.add_header(...);`) keeps that allocation in the arena.
- Percent-decoded path-parameter captures (`url_decode_arena`, used by
  `route_registry.cpp`'s `match_template`).

What must **not** land in the arena, and does not:

- The driver's session-scope `outbuf` (the pipelined-response accumulator) and `flat_buffer` (the read buffer) -
  both must survive the per-request arena reset, so both are plain heap containers reused across the whole
  session, never arena-backed.
- `Body`'s buffered helpers (`read_to_string`, `read_json`, `read_form`, `read_multipart`): their results are
  handler-local user data and allocate on the global heap by design.
- Cold-path error responses built through the static `ResponseFactory` (`to_http_response` overloads): see
  Error model above.

`Body` itself is a small-buffer-optimized value type (48-byte inline storage, an internal vtable, no
`unique_ptr` node): `EmptyBody`, small `OwnedBufferPayload` strings, and the zero-copy `BeastRequestBody` span
all live inline; a payload that does not fit is a `static_assert` failure at compile time, not a silent heap
spill (see `body.hpp`). `Headers` is a tagged union
(`std::variant<BeastBacking, OwnedBacking>`): `BeastBacking` is a zero-copy read-only view over Beast's
already-arena-backed fields, and `OwnedBacking` is a `pmr::vector` bound to whichever allocator
`Headers::owned(alloc)` was given. Mutating a `BeastBacking` view asserts unless `promote_to_owned(alloc)` has
first copied it into an owned backing.

The practical corollary: nothing built from the arena may outlive the connection's next
`reset_request_arena()` call. A value that must survive longer (a cache entry, a background task's payload) has
to be copied out onto its own allocator before the handler returns.

## Threading model

`Server` is handed one or more executors and never creates, drives, or stops one itself
(`Server(ServerConfig, Executor)` / `Server(ServerConfig, std::vector<Executor>)`,
`server.hpp`). `Executor` is
`boost::asio::io_context::executor_type` - **not** `boost::asio::any_io_executor`
(`executor.hpp-42`). This is a deliberate deviation from
a type-erased executor: `any_io_executor`'s virtual dispatch plus shared-ptr refcounting on every I/O operation
measured at 14.25% of all cycles on the request hot path, so the Server, connections, and drivers are typed to
the concrete executor instead. The cost is that the Server can no longer be driven by a foreign executor type
such as a `thread_pool`; every real construction site already passed `io_context::executor_type` anyway.
`Strand` is an alias for the same concrete `Executor`, not `asio::strand<Executor>`: the concurrency contract is
that each injected executor is driven by **at most one thread** (an io_context-per-worker-thread topology), so
that single runner already serializes everything on its connections; a real strand would add a queue round-trip
for no correctness benefit and measurably hurt throughput.

When `Server` is given a `std::vector<Executor>`, `execs.front()` is the **control** executor: the setup
barrier, observer notifications, and the shutdown coroutine all run there. `TcpListener`/`TlsListener` place
each newly accepted connection's strand on the *next* executor in the vector, round-robin
(`next_exec_ = (next_exec_ + 1) % execs_.size()`), so with an io_context-per-thread caller, connections spread
across worker loops with no shared scheduler between them. `run_standalone(cfg, threads, configure)` builds
exactly that topology: one `io_context` per worker thread (held in a `std::deque` since `io_context` is
immovable), one `jthread` running each, and the vector of `.get_executor()`s handed to `Server`.

**Connection-to-thread affinity.** Once a connection is accepted onto a strand, every I/O operation for the rest
of its life - parsing, dispatch, writing, every keep-alive iteration - stays on that same strand/executor
(`TcpListener::run()`'s `co_spawn(strand, ...)`). Driver coroutines use `use_strand_awaitable` (a
`use_awaitable_t<Strand>` token) rather than the default `use_awaitable`, so no `co_await` inside the request
loop re-erases the coroutine frame back to `any_io_executor`.

**Deadline enforcement is per-listener, not per-connection.** Rather than a watchdog timer per socket (measured
to stall single-runner workers: 256 connections at a 500 ms tick cost 35% of pipeline-depth-1 throughput), each
connection just stores its current deadline in a relaxed atomic
(`TcpConnection::set_deadline_after`, `tcp_connection.hpp-75`),
and `ConnectionTracker::start_sweep` runs **one** coroutine per listener, on the listener's home executor, that
walks every registered connection every 500 ms
(`connection_tracker.hpp`,
`connection_tracker.cpp-63`) and
force-cancels whichever have expired.

**Cancellation.** Each connection owns its own `asio::cancellation_signal`; each listener's accept loop runs on
its own strand bound to its own signal (`Server::setup()`: a slot holds at most one handler, so a signal cannot
be shared across listeners). Shutdown and deadline-sweep cancellations are `asio::dispatch`ed onto the target's
own strand so the emit is serialized with that connection's or listener's in-flight turns.

## Configuration

Every config type is `serialization::ConfigInterface<Self, Json::Value>` - the project-wide JSON pattern: a
private default constructor, a fluent `Builder`, a `fields()` tuple for the (de)serialization walk, and a
`validate()` that both `Builder::finalize()` and `load_server_config` call.

- **`ServerConfig`**: `listeners`
  (`vector<ListenerConfig>`), `threads` (consumed only by `run_standalone`; the injected-executor path takes its
  thread count from whoever drives the executors), `timeouts` (a `Timeouts`), `body_limit` (default 16 MiB),
  `request_arena_size` (default 8192), `drain_timeout` (JSON key `drain_timeout_ms`, default 30 s), and
  `path_normalization` (`"none"` / `"collapse_trailing_slash"` (default) / `"collapse_multi_slash"`).
- **`ListenerConfig`**: `bind` (address),
  `port`, `transport` (`"tcp"` / `"tls"` / `"quic"`), `protocols` (array of `"http1"` / `"http2"` / `"http3"`; an
  empty array defaults per transport via `effective_protocols()`: `http1` for tcp/tls, `http3` for quic), and an
  optional `tls` block (required for tls/quic, rejected for tcp). The order of `protocols` is meaningful for a
  TLS listener: it becomes the ALPN server-preference order.
- **`TlsConfig`**: `cert_file`, `key_file`,
  `key_passphrase` (`FieldPolicy::Secret`: read from JSON, never re-emitted by `dump_server_config`),
  `dh_params_file`, `ca_file`, `min_version` (`"tls12"` (default) / `"tls13"`), `session_cache` (default `true`),
  `require_client_cert`. It feeds
  `build_ssl_context`: a `tls_server`
  context with `NO_SSLv2|NO_SSLv3|NO_TLSv1|NO_TLSv1_1|NO_COMPRESSION|SINGLE_DH_USE` (plus `NO_TLSv1_2` when
  `min_version` is `tls13`), a fixed modern AEAD cipher list for the TLS 1.2 floor (a
  `SSL_CTX_set_cipher_list` failure throws rather than silently falling back to OpenSSL's defaults), the
  certificate chain and private key (with an optional passphrase callback), optional DH params and client-cert
  verification, a session-cache mode, and the ALPN selection callback that picks the first server-advertised
  protocol the client also offers.
- **`Timeouts`**: `header_ms` / `body_ms` / `idle_ms`
  (defaults 10 s / 30 s / 60 s), mapped by `attach_default_listeners` onto `Http11Config`'s three phase timeouts
  (`max_header_bytes` has no `ServerConfig` field and keeps its 16 KiB struct default).
- **`load_server_config(path)`**
  returns `beavers::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError>`: the path must
  be a regular file, JSON is parsed via jsoncpp's `CharReader`, unknown JSON keys are ignored, a missing key
  keeps its declared default, and an unknown enum string or a `validate()` failure surfaces as
  `ConfigSchemaError`. `dump_server_config` round-trips a config back to JSON, omitting the secret passphrase
  field.
- **`attach_default_listeners(Server&)`**
  walks `cfg.listeners()` and adds the matching listener + driver instances. Supported `(transport, protocols)`
  combinations are `tcp+[http1]`, `tls+[http1]`, `tls+[http2]`, `tls+[http1,http2]` or `[http2,http1]` (JSON order
  becomes ALPN preference order, which becomes template-argument order), and `quic+[http3]`; anything else -
  notably `tcp+[http2]` (h2c is unsupported) - throws `std::invalid_argument`. Programmatic
  `add_tcp_listener`/`add_tls_listener`/`add_quic_listener` calls compose freely with config-driven ones; an
  empty `listeners` array is a no-op, which is how
  `examples/http/minimal_http_server.cpp` falls back to a
  programmatic listener when no config file is given:

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

## Lifecycle and shutdown

**Build phase.** `add_tcp_listener` / `add_tls_listener` / `add_quic_listener`, `add_controller` /
`in_group(prefix).add_controller`, and `add_observer` are all single-threaded and all throw `std::logic_error`
once called after `setup()`.

**`Server::setup()`**
(`server.cpp`):

1. `registry_.freeze()`; a non-empty conflict set throws `RouteConflictAggregateError` carrying every conflict at
   once.
2. `bind()` is attempted on **every** listener (best-effort-all, so the operator sees every bad endpoint in one
   error); on any failure the whole listener set is cleared - RAII closes whatever did bind - and
   `ListenerBindError` is thrown aggregating every failure. No listening-but-unserved socket is ever left behind.
3. Observer fan-out hooks are wired into the `Router`: skipped entirely when there are no observers, which
   keeps `Router::dispatch`'s hookless fast path (no dispatch coroutine frame at all) as the common case.
4. State moves to `starting`: a `stop()` arriving from here on latches `stop_requested_` instead of silently
   no-oping.
5. If any observers are registered, their `on_setup_complete()` is awaited sequentially, in add order, as a
   barrier on `execs_.front()`: this **blocks** the calling thread, so the executor must already be driven by
   another thread whenever observers are registered.
6. Each listener's `run(router_)` is `co_spawn`ed onto its own strand of one of `execs_` (round-robin), each
   bound to its own `cancellation_signal`.
7. State moves to `running`; a `stop()` latched in step 4 is honored now instead of being lost.

**`Server::stop()`** is non-blocking, idempotent, and callable from any executor thread (signal handlers, request
handlers). It CASes `running` to `stopping` and `co_spawn`s `graceful_shutdown()` detached on `execs_.front()`;
before `setup()` it is a documented no-op, and racing `starting` latches as above. It never stops an executor.

**`graceful_shutdown()`** runs these steps, numbered in the source as 1, 1.5, 2, 2.5, 3, 4, 5:

1. Cancel every accept loop: one `cancellation_signal` per listener, each `emit` dispatched onto that
   listener's own strand.
2. (1.5) Poll until every accept loop has exited; from this point new connections are provably refused (each
   listener's acceptor closes on every `run()` exit path), never silently backlogged.
3. (2) Drain in-flight requests: `listener->drain_until(deadline)` for every listener against one shared deadline
   (`now() + cfg_.drain_timeout()`); `ConnectionTracker::drain_until` polls the in-flight counter and, at the
   deadline, force-cancels every surviving connection.
4. (2.5) Poll until `total_in_flight() == 0` across all listeners. `drain_until` only *dispatches* the
   force-cancels; it does not wait for the cancelled `serve()` coroutines to actually unwind, and destroying a
   listener with frames still suspended would be a use-after-free. This step is deliberately unbounded: a
   handler that ignores cancellation delays shutdown rather than corrupting it.
5. (3) Await every observer's `on_shutdown_started()`, sequentially, on the still-driven executor, so real async
   cleanup finishes before completion is reported; a throw is caught and fanned to every observer's
   `on_unhandled_exception`.
6. (4) Call every observer's `on_shutdown_complete()` (sync, `noexcept`).
7. (5) Latch `shutdown_complete_`, set state to `stopped`, and notify under the shutdown mutex: the unblock
   point for `wait_until_stopped()`, and, per the source comment, the coroutine's last touch of `*this`.

**The shutdown-ordering contract holds under either executor topology:** after `stop()`, the caller must keep
driving **every** injected executor until `wait_until_stopped()` (blocking;
must not be called from an executor thread) or `async_wait_stopped()` (an exponential-backoff poll, 5 ms up to a
320 ms cap, for callers already on an executor) returns: only then may the executor(s) be stopped or destroyed.
`Server` never stops an executor itself, since it may be shared with subsystems (a DB pool, a logger) that must
outlive HTTP's shutdown.

**`~Server()`** is an RAII backstop: if the Server is destroyed while still `starting`/`running`/`stopping`, the
destructor logs a warning and runs `stop()` + `wait_until_stopped()` itself. If the executor is not being driven
- or the destructor runs on an executor thread - this blocks forever, loudly, which is judged strictly better
than freeing members out from under still-live coroutine frames.

**`run_standalone(cfg, threads, configure)`** performs the whole stop -> wait -> executor-stop -> thread-join
sequence for the "HTTP owns the process" case: SIGINT/SIGTERM handlers (installed only after `setup()` succeeds)
call `server.stop()`. An external thread must not call `stop()` on a `run_standalone`-owned Server: that would
race the internal `io_context`'s teardown against the stop call's post; shutdown must be triggered by a signal or
from within a handler/observer running on the internal executor.

**Observers** (`ServerObserver`,
`server_observer.hpp`) split into two
groups: the awaitable lifecycle hooks above (`on_setup_complete`, `on_shutdown_started`, `on_shutdown_complete`),
and the synchronous, `noexcept` per-request hooks `on_request`, `on_response`, and `on_unhandled_exception`, which
may fire concurrently from any executor thread (one strand per connection) and so must be thread-safe and cheap.
`on_response` receives a `RequestInfo` snapshot (`method` + `target`) captured at dispatch entry rather than the
`RequestContext` itself, because the context is consumed by value through the handler chain and no longer exists
once a response comes back.

## Extension points

**Controllers.** Subclass `HttpController` and implement `configure_routes()`, registering routes through the
verb DSL (`Get`, `Post`, `Put`, `Patch`, `Delete`, `Head`, `Options`: see
`controller.hpp`). Each verb accepts three handler
shapes: a member-function pointer returning `AsyncResponse`; a member-function pointer returning
`AsyncOutcome<Response, Errors...>`; or any free function/lambda satisfying `IsRouteHandler` in either return
shape. `configure_routes()` runs exactly once, at bake time; calling a verb method or `add_middleware` after
baking throws `std::logic_error`.

**Mounting.** `server.add_controller(ctrl)` is shorthand for `server.in_group("").add_controller(ctrl)`;
`server.in_group(prefix).add_controller(ctrl)` mounts under a prefix, and
`GroupBinding::in_group(sub_prefix)` nests further. Mounting bakes the controller immediately: its local route
table drains into the server's `RouteRegistry` with the prefix applied and the middleware chain and
Outcome-to-Response conversion pre-composed, and the `Server` keeps the controller alive (`shared_ptr`) for as
long as its routes exist.

**Middleware.** `using Middleware = std::function<AsyncResponse(RequestContext, const NextHandler&)>`
(`middleware.hpp`). Attach with
`controller.add_middleware(mw)` (first added is outermost) or the `add_basic_middleware(ctrl, mw1, mw2, ...)`
helper. A middleware can short-circuit (return without calling `next`), enrich the context
(`ctx.set<T>(value)` then call `next`), or post-process
(`auto r = co_await next(std::move(ctx)); ...; co_return r;`). `next` is always passed as `const NextHandler&`,
never copied by value: the composed chain lives in the frozen registry and outlives any single request, and
copying a capturing `std::function` per layer per request would allocate on the hot path.

**Typed errors.** A handler returning `AsyncOutcome<Response, E1, E2, ...>` gets each `Ei` collapsed to a
`Response` through an ADL-found `to_http_response(const Ei&)`. The built-in set lives in
`types/errors/`; a user-defined error type only needs its own
`to_http_response` overload discoverable by ADL next to the type. A missing overload is a compile error naming
the offending type, not a runtime surprise.

**A worked example.** `examples/http/minimal_http_server.cpp`
shows all of the above together: a `GreeterController` with a path-parameter handler
(`ctx.path_param_or<std::string>("name", ...)`), a plain JSON handler, and a typed-error handler that turns a
`BodyLimitExceeded` Outcome error into a 413 without the handler ever building an error response itself; a
post-processing middleware that stamps a response header; and both the config-file and programmatic listener
wiring paths side by side.

**Observers** are the extension point for cross-cutting server concerns - metrics, structured request logging,
coordinated startup/shutdown of a resource a handler depends on - rather than per-route logic; see Lifecycle and
shutdown above.
