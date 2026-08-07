#pragma once

#include <menagerie/beavers>
#include <tuple>

namespace menagerie::db {

    /**
     * @brief Result of QueryCompiler::compile_static(...): SQL text plus its parameter tuple, both usable at
     *        compile time.
     *
     * SqlStringT is a fixed-capacity `beavers::InlineString<MaxLen>` (never std::string) so the whole object
     * can live in a constexpr context with no heap allocation; Params... are the literal values collected in
     * source order while walking the expression tree.
     */
    template <typename SqlStringT, typename... Params>
    struct CompiledStaticQuery {
        SqlStringT sql_;                ///< The compiled SQL text, stored inline (no heap allocation).
        std::tuple<Params...> params_;  ///< The captured literal values, in source order.

        /// The compiled SQL text.
        [[nodiscard]] constexpr std::string_view sql() const noexcept {
            return std::string_view{sql_};
        }

        /// The compiled SQL text as a NUL-terminated C string.
        [[nodiscard]] constexpr const char* c_sql() const noexcept {
            return sql_.c_str();
        }

        /// The captured parameter tuple, in source order.
        [[nodiscard]] constexpr const std::tuple<Params...>& params() const noexcept {
            return params_;
        }

        /// Number of captured parameters (sizeof...(Params)).
        [[nodiscard]] constexpr std::size_t size() const noexcept {
            beavers::force_non_static(this);
            return sizeof...(Params);
        }
    };

    // Deduction guide
    /// Deduces SqlStringT/Params... from the sql/params arguments passed to the aggregate-init constructor.
    template <typename SqlStringT, typename... Params>
    CompiledStaticQuery(SqlStringT, std::tuple<Params...>) -> CompiledStaticQuery<SqlStringT, Params...>;

}  // namespace menagerie::db
