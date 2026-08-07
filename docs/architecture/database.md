# Database Component

The database component (`component/database/`) is a layered PostgreSQL client. A provider-agnostic core --
`base/`, `primitives/`, and `query/` -- describes tables, columns, and SQL expressions in C++ and compiles them
into parameterized SQL text without knowing which database engine will run it. A provider layer supplies the
engine-facing half: today that is `providers/postgresql/` only, built whenever `BUILD_POSTGRESQL` is on (the
default). The dividing line is deliberate: everything above the provider layer can be type-checked and, for
schemas known at compile time, evaluated at compile time; everything in the provider layer talks to libpq,
manages real sockets, and returns `beavers::Outcome<T, ErrorContext>` instead of throwing on the ordinary failure
paths (pool exhaustion, a bad connection, a constraint violation). Two independent connection-pooling strategies
live side by side in the PostgreSQL provider -- a lock-free ring-buffer pool for the fail-fast path and a classic
mutex-plus-FIFO pool for strict waiter fairness -- both exposed through the same small `CapabilityProvider`
surface so executor and transaction code does not need to know which pool it is borrowing from.

## Layer map

- **`features/`**: the `Providers` enum (`None`, `PostgreSQL`, and a
  `Redis` enumerator gated behind a `REDIS_ENABLED` macro that no build option currently defines) plus the
  `POSTGRESQL_ENABLED`/`REDIS_ENABLED` compile definitions derived from `BUILD_POSTGRESQL`/a future
  `BUILD_REDIS`. It has no internal dependencies of its own; almost everything else in the component depends on
  it, directly or transitively, since dialects, type mappings, and table schemas are all keyed on `Providers`.
- **`base/`**: four small, mostly independent pieces that together define
  "what a provider must implement." `dialect/dialect_concepts.hpp` declares `IsSqlDialect` (the compile-time
  contract a dialect type satisfies: `quote_identifier`, `placeholder`, `limit_clause`, `format_value`, and the
  `supports_*()` feature flags) and `DialectBase<T>`, a CRTP base providing sane feature-flag defaults so a
  concrete dialect only overrides what differs. `errors/` holds the exception hierarchy (`database_exception` and
  its `client_error`/`server_error`/`fatal_error` branches, all `boost::exception`-derived) and the
  `ErrorCode`/`ClientErrorCode`/`ServerErrorCode`/`FatalErrorCode` value-type error model used everywhere else
  instead of exceptions on the hot path. `params/sql_params.hpp` declares `ParamSink`, the interface a dialect's
  runtime parameter binder implements. `type_mapping/sql_type_mapping.hpp` declares the
  `SqlTypeMapping<T, Provider>` trait template -- intentionally left undefined in the primary template, so a
  missing mapping is a compile error, not a silent fallback -- and the `sql_type_for<T, P>()` /
  `sql_type<T>(provider)` accessors built on it. `base.Dialect` depends on `base.Params` and `Features`;
  `base.TypeMapping` depends on `Features` and `base.Dialect`; `base.Params` depends on `primitives` for
  `FieldValue`.
- **`primitives/`**: the schema and value vocabulary shared by every
  layer above it. `FieldValue` is a closed `std::variant` of every storable C++ type (bool, the fixed-width
  integers, float/double, owning and view string/binary forms); `Field` is a schema-validated value holder;
  `Column`/`TypedColumn<CppType>` name a column, optionally table-qualified and aliased; `Record` is a
  schema-backed row keyed by a `shared_ptr<const DynamicTable>`. Two table flavors exist side by side:
  `StaticTable<TableName, FieldSchemas...>` takes its name as a compile-time `beavers::FixedString` NTTP and its
  fields as `StaticFieldSchema<CppType, Name, Constraints...>` template parameters -- no heap allocation for the
  schema itself, and `column<"name">()` is a compile-time name lookup that returns a `TypedColumn<CppType>` with
  the value type auto-deduced -- while `DynamicTable` registers fields at runtime through `add_field(...)` and
  resolves them by name through `get_field_schema(...)`. Depends only on `Features` and common Beavers utilities.
- **`query/`**: the expression tree
  (`query/expressions/`) and its compiler (`query/compiler/`); see Query building below for how the two fit
  together. Depends on `primitives` (columns and tables are expression operands) and on `base.Dialect` /
  `base.TypeMapping` (the SQL-generating visitor calls into whichever dialect and type-mapping traits the caller
  selects).
- **`providers/postgresql/`**: the only implemented provider,
  and the only layer that depends on `query/` (its executors accept `CompiledStaticQuery`/`CompiledDynamicQuery`)
  and links libpq directly. It supplies `PostgresDialect` (satisfying `IsSqlDialect`), the
  `SqlTypeMapping<T, Providers::PostgreSQL>` type-mapping definitions, the OID/format/SQL-type registries that
  results are decoded through, and the session/executor/transaction stack described below. See PostgreSQL
  provider below.

## Query building

Every expression node -- `SelectExpr`, `WhereExpr`, `JoinExpr`, a bare column, a literal -- derives from
`Expression<Derived>` (CRTP) and implements `accept(visitor)`, so the whole tree is walked with the visitor
pattern rather than virtual dispatch. Fluent methods are constrained by feature tags: `QueryOperations<Derived,
AllowedFeatures...>` only exposes `.where(...)`, `.group_by(...)`, `.having(...)`, `.join(...)`, `.order_by(...)`,
`.limit(...)` when the concrete expression's `AllowedFeatures...` pack contains the matching `AllowWhere` /
`AllowGroupBy` / ... tag, so calling `.having(...)` on a query shape that does not support it is a compile error,
not a runtime one. Raw values passed to `select(...)` or compared against a column are wrapped in `Literal<T>`
automatically (`lit(value)` does this explicitly; `detail::make_literal_if_needed` does it implicitly for
operands that are not already expression nodes), and comparison operators on columns and literals build
`BinaryExpr<Left, Right, Op>` nodes (`OpEqual`, `OpAnd`, `OpLike`, ...) that satisfy `IsCondition` for `.where()`.

`QueryCompiler<DialectT, DefaultMode>` (`query/compiler/query_compiler.hpp`) turns a tree into SQL through
`SqlGeneratorVisitor`, in one of two modes selected by `ParamMode`: `compile_static(...)` is the constexpr path --
it writes into a fixed-capacity `beavers::InlineString<MaxLen>`, allocates nothing on the heap, and can run at
compile time when the expression is itself `constexpr` -- while `compile_dynamic(...)` is the runtime path,
allocating a `std::pmr::monotonic_buffer_resource` arena and writing into a `std::pmr::string`. `ParamMode` itself
has three values: `Inline` (literal values are formatted directly into the SQL text through the dialect's
`format_value`, no parameter list), `Tuple` (placeholders in the SQL, values collected into a `std::tuple` -- the
default for `compile_static`), and `Sink` (placeholders in the SQL, values pushed one at a time into a
dialect-provided `ParamSink` -- the default for `compile_dynamic`, since a runtime-sized parameter list cannot
live in a `std::tuple`).

```cpp
using UsersTable = StaticTable<"users",
                                StaticFieldSchema<int, "id", constraints::PrimaryKey, constraints::NotNull>,
                                StaticFieldSchema<std::string, "name">>;

UsersTable users{};
auto id_col = users.column<"id">();

auto query = select(id_col, users.column<"name">())
                 .from(users)
                 .where(id_col == lit(1));

QueryCompiler<postgres::PostgresDialect> compiler;
auto compiled = compiler.compile_dynamic<ParamMode::Inline>(query);
// compiled.sql() is a SELECT ... FROM "users" WHERE "id" = 1 statement,
// quoted and formatted per PostgresDialect
```

## PostgreSQL provider

`CapabilityProvider` (`capabilities/provider/capability_provider.hpp`) is a concept, not a base class: a type
satisfies it by providing `with_sync() -> Outcome<SyncExecutor, ErrorContext>` and
`with_async(exec) -> Outcome<AsyncExecutor, ErrorContext>`. Two session types implement it against two different
pools:

- **`LockFreeSession`** (`session/free_lock/`) owns a `ConnectionPool`: a fixed-size vector of connection slots
  acquired with a CAS scan from a hint cursor, no dynamic growth, and INACTIVE slots lazily promoted to FREE the
  first time an acquire finds nothing free. A background `PoolJanitor` (a `std::jthread` with a `stop_token`)
  periodically replaces DEAD slots and health-checks FREE ones past their `max_lifetime`. `with_sync()` /
  `with_async(exec)` fail fast on exhaustion; the timed overloads (`with_sync(timeout)`,
  `with_async(exec, timeout)`) retry acquisition against a bounded sleep-retry budget rather than parking on a
  wait primitive. `LockFreeSession` is the session type that actually satisfies `CapabilityProvider`.
- **`BlockingSession`** (`session/blocking/`) owns a `BlockingPool`: a classic mutex-guarded pool with a single
  FIFO deque holding both blocking waiters (`Waiter`, parked on a condition variable) and async waiters
  (`AsyncWaiter`, registered through `boost::asio::async_initiate` and resumed by the same release path) --
  fairness holds regardless of which acquisition mode competing callers use. Each method has three variants:
  `try_*` (non-blocking, fails immediately on exhaustion), a timed overload (bounded wait), and an unbounded
  overload (waits until a slot frees or `shutdown()` drains every waiter). Because its unbounded and timed
  `with_async(...)` overloads return a `boost::asio::awaitable` that suspends the caller's coroutine rather than
  a synchronous `Outcome`, `BlockingSession` does not itself satisfy `CapabilityProvider`.

Both pools hand out a `std::weak_ptr<ConnectionHolder>` rather than a raw `PGconn*`. `ConnectionHolder`
(`connection_holder/connection_holder.hpp`) is the polymorphic base every pool-managed connection handle
implements; on a capability's destruction the weak pointer is locked to a temporary `shared_ptr` and `reset()` is
called, which runs any pending cleanup SQL and then returns the connection through a path chosen by the owning
pool (the lock-free pool's `SlotHolder` clears its slot bit; the blocking pool's `QueuedHolder` hands the
connection straight to the head waiter under the pool mutex if one is waiting, rather than round-tripping through
the free deque). The cleanup SQL is opt-in per borrow: `do_cleanup(CleanupQuery)` on an executor, transaction, or
auto-transaction sets one of `ResetAll` / `DeallocateAll` / `DiscardTemp` / `DiscardAll` to run on release; with
no explicit choice, the pool's own configured cleanup runs instead.

`SyncExecutor` and `AsyncExecutor` (`capabilities/executors/`) are the two capabilities implemented today; the
`copier/`, `notifier/`, and `pipeline/` directories exist as empty scaffold headers for capabilities not yet
built. Both executors are move-only, constructible either standalone from a raw `PGconn*` (caller keeps
ownership) or from a pool's `std::weak_ptr<ConnectionHolder>` (locked once at construction), and share one
`execute(...)` overload set: a bare query string, a query plus a `Params` block, variadic
`FieldValue`-constructible arguments (bound through a stack-allocated `ParamSink` for parameter sets under 2 KiB),
tuple arguments, or a `CompiledStaticQuery`/`CompiledDynamicQuery` from the query compiler. `AsyncExecutor`
additionally wraps the connection's socket in a `boost::asio::posix::stream_descriptor` and drives libpq's async
protocol (`PQsendQuery` / `PQconsumeInput` / `PQisBusy`) so a suspended coroutine never blocks its `io_context`
thread; it is documented as not thread-safe, so concurrent access needs a strand.

`Transaction` (`transaction/base/transaction.hpp`) is a move-only wrapper holding a `std::weak_ptr<ConnectionHolder>`,
`TransactionOptions` (isolation level, access mode, `deferrable` -- valid only with `SERIALIZABLE` + `READ ONLY`,
enforced in `to_begin_sql()`), and a `TransactionStatus` (`IDLE` / `ACTIVE` / `COMMITTED` / `ROLLED_BACK`).
`begin()` / `commit()` / `rollback()` are explicit and state-checked: calling `with_sync()`/`with_async()` before
`begin()`, or committing/rolling back a transaction that already finished, returns
`ErrorContext{ClientErrorCode::InvalidState}` instead of misbehaving. `Session::begin_transaction(opts[, timeout])`
borrows a slot exactly like `with_sync()` and returns an `IDLE` `Transaction` the caller must `begin()` itself;
`begin_auto_transaction(opts[, timeout])` issues the `BEGIN` immediately and returns an already-`ACTIVE`
`AutoTransaction` -- a thin wrapper delegating to an inner `Transaction` that satisfies `CapabilityProvider` on its
own. Destroying a `Transaction`/`AutoTransaction` without an explicit `commit()` drops the connection holder's
weak reference and runs the same `ConnectionHolder::reset()` cleanup path an executor going out of scope would --
so an abandoned transaction is discarded by whatever cleanup the pool or a `do_cleanup()` call configured for that
slot, not by rollback logic that belongs to the transaction's own destructor. `Savepoint` (`tx.savepoint(name)`) is a
scoped `SAVEPOINT` / `RELEASE SAVEPOINT` / `ROLLBACK TO SAVEPOINT` wrapper holding the raw `PGconn*` directly
rather than another weak pointer, since a savepoint never outlives the transaction that created it.

Pool sizing and connection parameters are configured through two builders: `PoolConfig::Builder` (`capacity` --
validated as a power of two, `min_connections`, `connect_timeout`, `idle_timeout`, `health_check_interval`,
`max_lifetime`, `cleanup_sql`, with `minimal()`/`standard()`/`high_performance()` presets) and
`ConnectionConfig::Builder` (host/port/dbname/user/password via `ConnectionCredentials`, SSL mode and
certificates, statement/lock/idle-in-transaction timeouts, node `role`/`priority`/`cluster_name` for
read/write-split-style deployments, and protocol flags such as `binary_protocol`/`pipeline_mode`, with
`local_dev()`/`testing()`/`production()` presets). `ConnectionConfig::to_connection_string()` renders the full
libpq DSN, including `sslmode` and any extra options.

## Usage sketch

```cpp
#include <postgres_session.hpp>
#include <query_compiler.hpp>
#include <query_expressions.hpp>

using namespace menagerie::db;
using namespace menagerie::db::postgres;

auto config = ConnectionConfig::Builder{}
                  .host("localhost")
                  .port(5432)
                  .dbname("app")
                  .user("app_user")
                  .password("secret")
                  .finalize();

LockFreeSession session{config, PoolConfig::standard()};

using UsersTable = StaticTable<"users",
                                StaticFieldSchema<int, "id", constraints::PrimaryKey>,
                                StaticFieldSchema<std::string, "name">>;
UsersTable users{};

QueryCompiler<PostgresDialect> compiler;
auto query = compiler.compile_dynamic<ParamMode::Inline>(
    select(users.column<"id">(), users.column<"name">()).from(users).where(users.column<"id">() == lit(42)));

auto exec_outcome = session.with_sync();
if (!exec_outcome.is_success()) {
    // exec_outcome.error<ErrorContext>() -- PoolExhausted, WaitTimeout, ...
    return;
}
auto exec = std::move(exec_outcome).value();

auto result = exec.execute(query);
if (result.is_success()) {
    const auto& block = result.value();
    for (std::size_t row = 0; row < block.rows(); ++row) {
        auto name = block.get<std::string>(row, 1);
    }
}

session.shutdown();
```

The same session also drives a transaction: `session.begin_transaction(opts)` returns an `IDLE` `Transaction`
that must be `begin()`-ed explicitly, while `session.begin_auto_transaction(opts)` returns one that is already
`ACTIVE`. Either way, `tx.with_sync()` / `tx.with_async(exec)` hand back the same `SyncExecutor`/`AsyncExecutor`
types `execute(...)` is called on above, so query-building and result-reading code does not change between a
plain session borrow and a transaction.
