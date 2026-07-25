# Serialization Library

The serialization library (`common/serialization/`) is Menagerie's field-descriptor serialization framework:
declare a `Field<&T::member, "key", Policy>` for every member a type wants serialized, and
`ConfigInterface<Derived, Formats...>` provides `serialize<Format>()` / `Derived::deserialize<Format>(...)`
for free, without hand-writing per-field code. It underlies every Builder-pattern config type across the
codebase -- `ServerConfig`, the PostgreSQL provider's `ConnectionConfig`, `ConsoleSinkConfig`, `FileSinkConfig`,
and more all derive from `ConfigInterface`. The only shipped wire format today is JSON (via jsoncpp's `Json::Value`), added
through `formats/json/`. Everything is reached through `#include <menagerie/serialization>`
(`export/menagerie/serialization`).

## Key types

- **`Field<Ptr, Name, Policy = FieldPolicy::Normal>`** -- one
  member's serialization descriptor: `ptr` is the member pointer, `name` a `beavers::FixedString` JSON key,
  and `owner_type` / `value_type` are recovered from `Ptr`'s type via `member_pointer_traits`.
- **`FieldPolicy`** -- `Normal` (serialize + deserialize),
  `Secret` (deserialize only -- e.g. passwords, so a re-serialized config never echoes the value back out),
  `Excluded` (skip both directions), `ReadOnly` (serialize only).
- **`ConfigInterface<Derived, Formats...>`** -- CRTP
  base providing `serialize<Format>()` and the static `deserialize<Format>(input)`; both call
  `Derived::validate()` and walk `Derived::fields()` field-by-field unless `Derived` opts into
  `custom_serialize`/`custom_deserialize`.
- **`HasFields<T>`** -- detects a `T::fields()`
  static member, so a nested `ConfigInterface`-derived member serializes as a nested object instead of a
  scalar.
- **`HasCustomSerialize<T, Format>` / `HasCustomDeserialize<T, Format>`**
  -- escape hatches: a type defining `custom_serialize(std::type_identity<Format>)` /
  `static custom_deserialize(const Format&)` skips the generic field walk entirely for that format.
- **`FieldName`** -- the key parameter every `write_field` /
  `read_field` overload takes; a domain type rather than a bare string so the call stays a two-phase-lookup
  ADL hit into `menagerie::serialization` even though the format headers (e.g. `json.hpp`) are usually
  included after `config_interface.hpp`.
- **`write_field(...)` / `read_field(...)` overloads for `Json::Value`**
  -- cover `std::string`, `std::string_view`, `int`, `std::size_t`, `std::uint16_t`, `bool`, `double`,
  `std::filesystem::path`, `std::map<std::string, std::string>`, `std::chrono::duration<Rep, Period>`, any
  enum, `std::optional<T>`, any `HasFields` type (nested object), and `std::vector<T>` (JSON array; `HasFields`
  elements nest as objects, everything else round-trips through a single-key wrap object).

## Usage

```cpp
#include <menagerie/serialization>

namespace ser = menagerie::serialization;

class RetryConfig final : public ser::ConfigInterface<RetryConfig, Json::Value> {
public:
    constexpr void validate() const override {
        if (max_attempts_ < 1) {
            throw std::invalid_argument("max_attempts must be >= 1");
        }
    }

    [[nodiscard]] int max_attempts() const noexcept { return max_attempts_; }

    static constexpr auto fields() {
        return std::tuple{
            ser::Field<&RetryConfig::max_attempts_, "max_attempts">{},
            ser::Field<&RetryConfig::api_key_, "api_key", ser::FieldPolicy::Secret>{},
        };
    }

    class Builder;

private:
    friend class ConfigInterface;
    constexpr RetryConfig() = default;

    int max_attempts_ = 3;
    std::string api_key_;
};

// ... RetryConfig::Builder mirrors ConsoleSinkConfig::Builder: a `this Self&&`
// setter per field plus `finalize() &&` that calls validate().

const auto json = cfg.serialize<Json::Value>();
const auto back = RetryConfig::deserialize<Json::Value>(json);
```

This shape (private default constructor, `friend class ConfigInterface`, a nested `Builder` with `this
Self&&`-deduced setters) matches
`common/crow/sink/console/config/console_sink_config.hpp`
and is exercised end-to-end in
`tests/unit_tests/serialization/test_config_interface.cpp`.

## Design notes

`FieldName` wrapping a `std::string_view` instead of taking a bare string is not stylistic: `write_field` /
`read_field` are called through a template defined in `config_interface.hpp`, which is normally included
*before* the format headers that supply the overloads (`json.hpp`). Ordinary unqualified lookup at the
template's definition point would see no overloads at all; two-phase lookup only reaches ones added later via
argument-dependent lookup on the arguments' associated namespaces. A bare `std::string` carries no namespace
ADL could search, but `FieldName` lives in `menagerie::serialization`, making that namespace an associated
namespace of every `write_field(out, FieldName{...}, ...)` / `read_field(...)` call -- so overloads defined
after `config_interface.hpp` (in `json.hpp`, or a future format header) are still found. `Secret` and
`ReadOnly` are asymmetric by design, not by omission: `Secret` fields are written *into* a config (so a
`Builder::secret(...)` setter and the deserialize path both work) but never read back out by `serialize()`,
while `ReadOnly` is the mirror image -- present in output, ignored on input.
