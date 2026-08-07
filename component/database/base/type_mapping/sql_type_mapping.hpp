#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

#include <providers.hpp>

namespace menagerie::db {

    /// Maps a C++ type to its SQL type string for a given provider. The
    /// primary template is intentionally left undefined, so a missing
    /// mapping is a compile error rather than a silent fallback; providers
    /// supply the mapping through explicit specializations exposing a
    /// `sql_type` member convertible to std::string_view.
    template <typename T, Providers Provider>
    struct SqlTypeMapping;

    /// Whether a SqlTypeMapping<T, P> specialization exists.
    template <typename T, Providers P>
    concept HasSqlTypeMapping = requires {
        { SqlTypeMapping<std::remove_cvref_t<T>, P>::sql_type } -> std::convertible_to<std::string_view>;
    };

    /// Compile-time accessor for T's SQL type string under provider P.
    /// static_asserts (rather than silently falling back) when no mapping exists.
    template <typename T, Providers P>
    [[nodiscard]] constexpr std::string_view sql_type_for() noexcept {
        static_assert(HasSqlTypeMapping<T, P>,
                      "No SQL type mapping exists for this type and provider. "
                      "Add a specialization of SqlTypeMapping<T, Provider>.");
        return SqlTypeMapping<std::remove_cvref_t<T>, P>::sql_type;
    }

    /// Runtime accessor for T's SQL type string, dispatching on a runtime
    /// Providers value through a compile-time-generated switch. Assumes
    /// `provider` is never Providers::None: the add_field<T>(name) call path
    /// (db_table_mapping.inl) checks for None and throws std::logic_error
    /// before ever reaching here, but a direct call with Providers::None
    /// hits std::unreachable() itself.
    template <typename T>
    [[nodiscard]] constexpr std::string_view sql_type(const Providers provider) noexcept {
        switch (provider) {
            case Providers::None:
                std::unreachable();
            case Providers::PostgreSQL:
                return sql_type_for<T, Providers::PostgreSQL>();
        }
        std::unreachable();
    }

}  // namespace menagerie::db

#include "detail/db_table_mapping.inl"
