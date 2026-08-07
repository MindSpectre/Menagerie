#pragma once

#include <menagerie/beavers>

#include <dialect_concepts.hpp>
#include <sql_params.hpp>

/// PostgreSQL provider: the libpq-backed implementation of the database component's
/// provider-agnostic core (dialect, type mappings, OID/format registries, connection
/// pooling, and the session/executor/transaction stack).
namespace menagerie::db::postgres {

    /// The PostgreSQL IsSqlDialect implementation: quoting, placeholders, and value
    /// formatting for PostgreSQL's SQL syntax and the $N parameter style.
    struct PostgresDialect : DialectBase<PostgresDialect> {
        /// Wraps `name` in double quotes, doubling any embedded double quote.
        template <Appendable StringT>
        static constexpr void quote_identifier(StringT& query, std::string_view name) {
            query += '"';
            escape_identifier(query, name);
            query += '"';
        }

        /// Appends a PostgreSQL positional placeholder ("$1", "$2", ...) for `index`.
        template <Appendable StringT>
        static constexpr void placeholder(StringT& query, const std::size_t index) {
            query += '$';
            query += beavers::constexpr_to_string(index);
        }

        /// Appends " LIMIT <limit>" and, if offset > 0, " OFFSET <offset>".
        template <Appendable StringT>
        static constexpr void limit_clause(StringT& query, const std::size_t limit, const std::size_t offset) {
            query += " LIMIT ";
            query += beavers::constexpr_to_string(limit);
            if (offset > 0) {
                query += " OFFSET ";
                query += beavers::constexpr_to_string(offset);
            }
        }

        /// Appends `value` as a PostgreSQL SQL literal, dispatching on its held type.
        template <Appendable StringT>
        static constexpr void format_value(StringT& query, const FieldValue& value) {
            format_value_impl(query, value);
        }

        /// PostgreSQL supports RETURNING clauses.
        [[nodiscard]] static constexpr bool supports_returning() noexcept {
            return true;
        }
        /// PostgreSQL supports LATERAL joins.
        [[nodiscard]] static constexpr bool supports_lateral_joins() noexcept {
            return true;
        }
        /// Identifies this dialect as Providers::PostgreSQL.
        [[nodiscard]] static constexpr Providers type() noexcept {
            return Providers::PostgreSQL;
        }

        // Runtime only - param binding

        /// Creates the runtime ParamSink (paired with its native Params packet) that
        /// accumulates $N-bound query arguments for this dialect.
        static DialectBindPacket make_param_sink(std::pmr::memory_resource* memory_resource);

    private:
        template <Appendable StringT>
        static constexpr void format_value_impl(StringT& query, const FieldValue& value);

        // -------- Per-type formatters --------

        template <Appendable StringT>
        static constexpr void format_null(StringT& query);

        template <Appendable StringT>
        static constexpr void format_bool(StringT& query, bool val);

        template <Appendable StringT>
        static constexpr void format_char(StringT& query, char val);

        template <Appendable StringT, std::floating_point FloatT>
        static constexpr void format_floating(StringT& query, FloatT val);

        template <Appendable StringT, std::integral IntT>
        static constexpr void format_integral(StringT& query, IntT val);

        template <Appendable StringT, typename StringValT>
        static constexpr void format_string_value(StringT& query, const StringValT& val);

        template <Appendable StringT, typename Container>
        static constexpr void format_binary(StringT& query, const Container& val);

        // -------- Helpers --------

        template <Appendable StringT>
        static constexpr void escape_char(StringT& out, char c);

        template <Appendable StringT>
        static constexpr void escape_string(StringT& out, std::string_view str);

        template <Appendable StringT>
        static constexpr void escape_identifier(StringT& out, std::string_view name);

        template <typename Container>
        static constexpr std::string format_binary_data(const Container& data);
    };

    static_assert(IsSqlDialect<PostgresDialect>);

}  // namespace menagerie::db::postgres

#include "detail/postgres_dialect.inl"
