# Roadmap

The direction of work after v1.0, in order. Each arc has a GitHub milestone of the
same name; research issues under a milestone are the entry point for that arc.

## 1. WebSockets

Upgrade handshake from the HTTP/1.1 driver, framing, and integration with the
router and graceful-shutdown drain. Nothing exists yet; this is the first new
protocol surface after v1.0.

## 2. HTTP/2

Real driver behind the existing `Http2Driver` scaffold. ALPN negotiation and the
TLS listener are already wired; `serve()` currently logs a warning and closes.
The open design question is multiplexed streams vs the one-request-per-connection
arena model.

## 3. HTTP/3

QUIC transport behind the existing `QuicConnection`/`QuicListener`/`Http3Driver`
scaffolds. Depends on picking a QUIC library and a UDP socket ownership model.

## 4. HTTP client

Client counterpart to the server stack: connection pooling, request building on
the existing types layer, TLS/ALPN reuse, sync + coroutine async surfaces.

## 5. PostgreSQL capabilities

Rounding out the provider. In rough order:

- Pipeline mode (`capabilities/pipeline/` is a scaffold)
- Prepared statement caching (`auto_prepare` exists as config, not yet applied)
- COPY IN/OUT and LISTEN/NOTIFY capabilities
- Session unification (BlockingSession + LockFreeSession -> one Session)
- Connection pool: validation-on-checkout and dynamic resizing
- `RETURNING` clause and expression-based `SET` in the query builder
- Type-safe result-to-struct mapping
- Additional field types: timestamp, date, time-of-day, UUID, decimal/numeric,
  JSON(B), arrays

## 6. Redis

New provider behind the existing `Providers::Redis` gate: RESP3 protocol layer,
session model reuse from the PostgreSQL provider, mapping a query-builder subset
onto Redis commands. MySQL and SQLite remain unplanned until after Redis.

## Standing items

- Crypto: `common/crypto/` has no tests and no call sites - needs coverage before
  anything depends on it.
- Integrations under research: Consul, OpenTelemetry, OpenAPI, generic watchdog.
