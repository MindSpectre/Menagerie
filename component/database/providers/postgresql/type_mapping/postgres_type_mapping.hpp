#pragma once

#include <menagerie/beavers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <db_field_value.hpp>
#include <postgres_sql_type_registry.hpp>
#include <sql_type_mapping.hpp>

namespace menagerie::db {

    // -------- PostgreSQL SqlTypeMapping specializations --------
    // One per C++ type FieldValue can hold (see sql_type_mapping.hpp for the
    // primary template's contract). Signed integer and floating-point types map
    // to their same-width Postgres equivalent; the unsigned integer types below
    // are widened since PostgreSQL has no native unsigned integer type.

    /// Maps C++ `bool` to PostgreSQL's boolean type.
    template <>
    struct SqlTypeMapping<bool, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::boolean;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `char` to PostgreSQL's fixed single-character CHAR(1) type.
    template <>
    struct SqlTypeMapping<char, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::char_type<1>();  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::int16_t` to PostgreSQL's SMALLINT type.
    template <>
    struct SqlTypeMapping<std::int16_t, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::smallint;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::int32_t` to PostgreSQL's INTEGER type.
    template <>
    struct SqlTypeMapping<std::int32_t, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::integer;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::int64_t` to PostgreSQL's BIGINT type.
    template <>
    struct SqlTypeMapping<std::int64_t, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bigint;  ///< PostgreSQL SQL type-name string.
    };

    /// Widened to INTEGER: Postgres SMALLINT is signed 16-bit and cannot hold
    /// the full uint16_t range.
    template <>
    struct SqlTypeMapping<std::uint16_t, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::integer;  ///< PostgreSQL SQL type-name string.
    };

    /// Widened to BIGINT: Postgres INTEGER is signed 32-bit and cannot hold the
    /// full uint32_t range.
    template <>
    struct SqlTypeMapping<std::uint32_t, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bigint;  ///< PostgreSQL SQL type-name string.
    };

    /// Mapped to NUMERIC(20,0): PostgreSQL has no native unsigned 64-bit
    /// integer type, and BIGINT is signed 64-bit and cannot hold the full
    /// uint64_t range; NUMERIC(20,0) is the smallest exact type that fits it.
    template <>
    struct SqlTypeMapping<std::uint64_t, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::numeric<20, 0>();  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `float` to PostgreSQL's REAL (single-precision) type.
    template <>
    struct SqlTypeMapping<float, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::real;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `double` to PostgreSQL's DOUBLE PRECISION type.
    template <>
    struct SqlTypeMapping<double, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::double_precision;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::string` to PostgreSQL's TEXT type.
    template <>
    struct SqlTypeMapping<std::string, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::text;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::string_view` to PostgreSQL's TEXT type.
    template <>
    struct SqlTypeMapping<std::string_view, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::text;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::vector<std::uint8_t>` to PostgreSQL's BYTEA (binary) type.
    template <>
    struct SqlTypeMapping<std::vector<std::uint8_t>, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bytea;  ///< PostgreSQL SQL type-name string.
    };

    /// Maps C++ `std::span<const std::uint8_t>` to PostgreSQL's BYTEA (binary) type.
    template <>
    struct SqlTypeMapping<std::span<const std::uint8_t>, Providers::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bytea;  ///< PostgreSQL SQL type-name string.
    };

}  // namespace menagerie::db

namespace menagerie::db::postgres {

    /// Compile-time accessor for T's PostgreSQL SQL type string.
    template <typename T>
    constexpr std::string_view sql_type_for() {
        return db::sql_type_for<T, Providers::PostgreSQL>();
    }

    /// Compile-time detection helper backing the static_assert below.
    namespace detail {
        /// Whether T has a PostgreSQL SqlTypeMapping specialization; std::monostate
        /// (SQL NULL) is trivially satisfied since it needs no SQL type.
        template <typename T>
        struct has_postgres_mapping {
            /// True when T has a PostgreSQL SqlTypeMapping specialization (or is
            /// std::monostate, which is trivially satisfied).
            static constexpr bool value =
                std::is_same_v<T, std::monostate> || db::HasSqlTypeMapping<T, Providers::PostgreSQL>;
        };
    }  // namespace detail

    // This will cause a compile error if any FieldValue type is missing a PostgreSQL mapping
    static_assert(beavers::all_variant_types_satisfy_v<FieldValue, detail::has_postgres_mapping>,
                  "Missing PostgreSQL SQL type mapping for one or more FieldValue types. "
                  "Add SqlTypeMapping<T, Providers::PostgreSQL> specialization for the missing type(s).");

}  // namespace menagerie::db::postgres
