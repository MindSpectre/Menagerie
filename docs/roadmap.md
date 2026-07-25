# Roadmap

## HTTP

- HTTP/1.1 client (only a server exists today)
- HTTP/2 driver (ALPN negotiation and the listener are wired up; `serve()` still logs a
  warning and closes the connection)
- HTTP/3 driver (QUIC listener is wired up; `serve()` still logs a warning and closes the
  connection)

## Database

- Connection pool: validation-on-checkout and dynamic resizing (idle timeout and max
  lifetime are already implemented)
- Prepared statement caching (`auto_prepare` exists as a config flag but is not yet
  applied by the provider)
- Pipeline mode capability (`capabilities/pipeline/` is still a scaffold)
- `RETURNING` clause: `InsertExpr` has no `.returning()` API yet, though the PostgreSQL
  dialect already declares `supports_returning() = true`
- Expression-based `SET` in `UPDATE` (`.set(col, col + 1)`); `UpdateExpr::set()` currently
  only accepts a `column, FieldValue` pair
- Type-safe result-to-struct mapping (`ResultBlock` currently returns raw `get<T>(row, col)`
  values)
- Additional field types: timestamp, date, time-of-day, UUID, exact-precision
  decimal/numeric, JSON(B), array types
- Additional providers: Redis (enum scaffolding exists, gated behind an undefined
  `REDIS_ENABLED`), MySQL, SQLite

## Crypto

- Test coverage: `common/crypto/` currently has no tests and no call sites anywhere else
  in the tree
