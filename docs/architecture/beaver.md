# Beaver Library

The beaver library (`common/core-beaver/`) is Menagerie's foundation layer: a header-only grab bag of the
primitives every other component builds on top of -- fixed-capacity strings, class-trait mixins,
meta-programming concepts and templates, byte-order conversion, transparent string hashing, and a handful of
compile-time-friendly utility functions. It has no dependencies on any other Menagerie library. Everything is
reached through the umbrella `#include <menagerie/beaver>` (`export/menagerie/beaver`); there is no single
"front door" type, since each header solves an unrelated problem.

## Key types

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
  `OneOf<T, Args...>`, `IsStringLike<T>`, `IsDuration<T>`, `IsInterface<T>`,
  `HasStaticNameMember<T>` / `HasStaticNameFunction<T>`, and related trait checks used as template constraints
  across the codebase.
- **`meta_programming/templates.hpp`** -- a trait for
  testing whether a type instantiates a given class template, `overloaded` (the `std::visit`
  callable-aggregator), `all_variant_types_satisfy_v`, and a minimal `type_list<Ts...>`.
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
#include <menagerie/beaver>

using namespace menagerie::beaver;

InlineString<31> name;
name.assign(std::string_view{"connection-pool"});

std::unordered_map<std::string, int, StringHash, StringEqual> counts;
counts["alpha"] = 1;
auto it = counts.find(std::string_view{"alpha"});  // heterogeneous lookup - no temporary std::string
```

## Design notes

Beaver deliberately has no result type of its own. Fallible hot-path functions across the database and HTTP
components return `std::expected<T, E>`; a function with several distinct failure modes returns
`std::expected<T, std::variant<E1, E2, ...>>`, so the error taxonomy is still spelled out in the signature and a
caller that handles only a subset fails to compile inside `std::visit` rather than failing at runtime. Failures
are built with `std::unexpected(e)`; a `std::unexpected<E>` converts into any `std::expected` whose error type
(or error variant) is constructible from `E`, so a single-error producer propagates into a multi-error consumer
unchanged. An earlier in-house `Outcome<T, Errors...>` (a flat `std::variant<T, Errors...>` with
error-set-widening `and_then`) was removed in favour of the standard type: the widening combinators had no
callers, and `std::expected` gives the same single-branch `has_value()` check and an identical layout in the
single-error case.
