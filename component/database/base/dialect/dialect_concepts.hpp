#pragma once

#include <concepts>
#include <string>
#include <string_view>

#include <db_field_value.hpp>
#include <providers.hpp>

namespace menagerie::db {

    /// A type that string-like values can be appended to with operator+=.
    template <typename T>
    concept Appendable = requires(T& s, std::string_view sv, char c) {
        { s += sv };
        { s += c };
    };

    /**
     * @brief The compile-time contract a SQL dialect type satisfies.
     *
     * Checked with std::string as the concrete Appendable type, since the
     * dialect methods themselves are templates constrained on Appendable so
     * they can also write into fixed-capacity buffers (e.g. beavers::InlineString).
     */
    template <typename T>
    concept IsSqlDialect = requires(std::string& s,
                                    std::string_view name,
                                    std::size_t idx,
                                    std::size_t limit,
                                    std::size_t offset,
                                    const FieldValue& fv) {
        { T::quote_identifier(s, name) };
        { T::placeholder(s, idx) };
        { T::limit_clause(s, limit, offset) };
        { T::format_value(s, fv) };
        { T::supports_returning() } -> std::same_as<bool>;
        { T::supports_cte() } -> std::same_as<bool>;
        { T::supports_window_functions() } -> std::same_as<bool>;
        { T::supports_lateral_joins() } -> std::same_as<bool>;
        { T::type() } -> std::same_as<Providers>;
    };

    /// CRTP base providing default feature-support flags for dialect
    /// implementations; a concrete dialect only overrides what differs.
    template <typename>
    struct DialectBase {
        /// Whether the dialect supports WITH (common table expression) queries.
        [[nodiscard]] static constexpr bool supports_cte() noexcept {
            return true;
        }
        /// Whether the dialect supports window functions (OVER clauses).
        [[nodiscard]] static constexpr bool supports_window_functions() noexcept {
            return true;
        }
        /// Whether the dialect supports LATERAL joins.
        [[nodiscard]] static constexpr bool supports_lateral_joins() noexcept {
            return false;
        }
        /// Whether the dialect supports RETURNING clauses on INSERT/UPDATE/DELETE.
        [[nodiscard]] static constexpr bool supports_returning() noexcept {
            return false;
        }
    };

}  // namespace menagerie::db
