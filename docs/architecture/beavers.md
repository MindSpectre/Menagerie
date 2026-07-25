# Beavers Library

The beavers library (`common/beavers/`) is Menagerie's foundation layer: a header-only grab bag of the
primitives every other component builds on top of -- a typed result type, fixed-capacity strings, class-trait
mixins, meta-programming concepts and templates, byte-order conversion, transparent string hashing, and a
handful of compile-time-friendly utility functions. It has no dependencies on any other Menagerie library.
Everything is reached through the umbrella `#include <menagerie/beavers>`
(`export/menagerie/beavers`); there is no single "front door"
type, since each header solves an unrelated problem.

## Key types

- **`Outcome<T, Errors...>`** -- sum type holding either a `T` or one
  of a fixed list of error types; the hot-path replacement for exceptions used throughout the database and HTTP
  components.
- **`BadOutcomeAccess`** -- exception thrown by `Outcome::value()` /
  `error<E>()` when called against the alternative not currently held.
- **`ok(v)` / `err(e)`** -- tag-constructor helpers that build an
  `Outcome` from a success or error value without repeating the full `Outcome<T, Errors...>` type at the call
  site.
- **`chain_outcomes(...)`** -- combines several `Outcome`s
  left-to-right, short-circuiting on the first error and merging every input's error set into the result.
- **`FixedString<N>`** -- compile-time string usable as a non-type
  template parameter (e.g. `StaticTable<"users", ...>`'s table name).
- **`InlineString<Capacity>`** -- fixed-capacity, non-allocating
  string; throws on overflow at `consteval` (a compile error for literal sources) and truncates silently at
  runtime.
- **`NonCopyable` / `Immovable` / `Immutable`** --
  mixins that delete the matching copy/move member functions.
- **`InterfaceBundle<Interfaces...>` / `InterfaceBundleFor<T>`**
  -- composes a pack of abstract interfaces into one concrete type without hand-writing a class per combination.
- **Concepts** (`meta_programming/concepts.hpp`) --
  `OneOf<T, Args...>`, `IsOutcome<T>`, `IsStringLike<T>`, `IsDuration<T>`, `IsInterface<T>`,
  `HasStaticNameMember<T>` / `HasStaticNameFunction<T>`, and related trait checks used as template constraints
  across the codebase.
- **`meta_programming/templates.hpp`** -- a trait for
  testing whether a type instantiates a given class template, `overloaded` (the `std::visit`
  callable-aggregator), `all_unique_v`, `merge_variants_t`, and a minimal `type_list<Ts...>`.
- **`StringHash` / `StringEqual`** -- transparent hash/equality
  functors enabling heterogeneous lookup (`std::string`, `std::string_view`, `const char*`) in unordered
  containers.
- **`ntoh<T>` / `hton<T>`** -- network/host byte-order conversion
  for unsigned integers.
- **`literals::_kb` / `_mb` / `_gb` / `_tb` and
  `get_type_name<T>()`** -- byte-size literals and a compile-time
  type-name string for common arithmetic/string types.
- **`UNRECOVERABLE_NOEXCEPT`** -- macro expanding to `noexcept`
  when `UNRECOVERABLE_EXCEPTIONS_TERMINATE` is set (the default), marking functions whose only possible
  exception is an unrecoverable `std::bad_alloc`.

## Usage

```cpp
#include <menagerie/beavers>

using namespace menagerie::beavers;

enum class IOError { FileNotFound, PermissionDenied, DiskFull };

Outcome<int, IOError> read_value() {
    if (!available()) {
        return err(IOError::FileNotFound);
    }
    return ok(42);
}

if (auto result = read_value(); result.is_success()) {
    use(result.value());
} else {
    handle(result.error<IOError>());
}

InlineString<31> name;
name.assign(std::string_view{"connection-pool"});
```

## Design notes

`Outcome<T, Errors...>` stores its errors as a closed `std::variant<T, Errors...>` rather than a single opaque
error type: every function's failure modes are enumerated in its return type instead of hidden behind a generic
`ErrorContext` or an exception hierarchy. `and_then(...)` widens the error variant to the union of the current
errors and the callable's errors, so a chain of calls with disjoint error sets composes into a result that can
hold any of them without the caller pre-declaring the widest variant up front; `chain_outcomes(...)` follows the
same widening rule. `or_else(...)` instead REPLACES the error variant with the callable's result type -- the
original error set is gone after recovery. This is why `Outcome` shows up as the return type
throughout the database and HTTP components instead of `throw`/`catch`: the error taxonomy is part of the
function signature, and a caller that only handles a subset of errors fails to compile rather than failing at
runtime.
