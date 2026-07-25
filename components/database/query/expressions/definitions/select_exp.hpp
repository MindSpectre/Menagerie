#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// `SELECT columns...`; the root of a fluent query chain. `.from(table)` narrows it to a
    /// FromTableExpr/FromCteExpr, which is where WHERE/GROUP BY/JOIN/ORDER BY/LIMIT become available.
    template <IsSelectable... Columns>
    class SelectExpr : public Expression<SelectExpr<Columns...>> {
    public:
        /// Wraps cols as the selected column list.
        template <typename... ColumnsTp>
            requires(std::constructible_from<Columns, ColumnsTp> && ...)
        constexpr explicit SelectExpr(ColumnsTp&&... cols) noexcept
            : columns_{std::forward<ColumnsTp>(cols)...} {
        }

        /// Sets/clears SELECT DISTINCT and returns *this for chaining.
        template <typename Self>
        constexpr auto&& set_distinct(this Self&& self, const bool d = true) noexcept {
            self.distinct_ = d;
            return std::forward<Self>(self);
        }

        /// The selected column list, as a tuple.
        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) noexcept {
            return std::forward_like<Self>(self.columns_);
        }

        /// Whether this SELECT emits DISTINCT.
        [[nodiscard]] constexpr bool distinct() const noexcept {
            return distinct_;
        }


        /// Narrows to `SELECT ... FROM table` for a runtime DynamicTablePtr.
        template <typename Self, typename TableTp>
            requires std::constructible_from<DynamicTablePtr, std::remove_cvref_t<TableTp>>
        [[nodiscard]] constexpr auto from(this Self&& self, TableTp&& table) {
            return FromTableExpr<SelectExpr, DynamicTablePtr>{std::forward<Self>(self), std::forward<TableTp>(table)};
        }

        /// @overload
        template <typename Self, typename TableTp>
            requires beavers::IsStringLike<TableTp> && (!beavers::IsStringViewLike<TableTp>)
        [[nodiscard]] constexpr auto from(this Self&& self, TableTp&& table_name) {
            return FromTableExpr<SelectExpr, std::string>{std::forward<Self>(self), std::forward<TableTp>(table_name)};
        }

        /// @overload
        template <typename Self>
        [[nodiscard]] constexpr auto from(this Self&& self, const char* table_name) {
            return FromTableExpr<SelectExpr, std::string_view>{std::forward<Self>(self), table_name};
        }

        /// Narrows to `SELECT ... FROM table`, taking the table from a Record's own schema.
        template <typename Self, typename T>
            requires std::same_as<std::remove_cvref_t<T>, Record>
        [[nodiscard]] constexpr auto from(this Self&& self, T&& record) {
            return FromTableExpr<SelectExpr, DynamicTablePtr>{std::forward<Self>(self),
                                                              std::forward<T>(record).table_ptr()};
        }

        /// Narrows to `SELECT ... FROM table` for a compile-time StaticTable.
        template <typename Self, IsStaticTable TableTp>
        [[nodiscard]] constexpr auto from(this Self&& self, const TableTp& table) {
            return FromTableExpr<SelectExpr, TableTp>{std::forward<Self>(self), table};
        }

        /// Narrows to `SELECT ... FROM cte_name`, sourcing from a CteExpr rather than a table.
        template <typename Self, IsCteExpr Query>
        [[nodiscard]] constexpr auto from(this Self&& self, Query&& query) {
            return FromCteExpr<SelectExpr, std::remove_cvref_t<Query>>{std::forward<Self>(self),
                                                                       std::forward<Query>(query)};
        }

    private:
        std::tuple<Columns...> columns_;
        bool distinct_ = false;
    };

    /// Builds `SELECT columns...`, auto-wrapping any raw value in columns in a Literal.
    template <typename... ColumnsTp>
    constexpr auto select(ColumnsTp&&... columns) {
        return SelectExpr<decltype(detail::make_literal_if_needed(std::forward<ColumnsTp>(columns)))...>{
            detail::make_literal_if_needed(std::forward<ColumnsTp>(columns))...};
    }

    /// Builds `SELECT DISTINCT columns...`.
    template <typename... ColumnsTp>
    constexpr auto select_distinct(ColumnsTp&&... columns) {
        return SelectExpr<decltype(detail::make_literal_if_needed(std::forward<ColumnsTp>(columns)))...>{
            detail::make_literal_if_needed(std::forward<ColumnsTp>(columns))...}
            .set_distinct(true);
    }

    /// Builds `SELECT * FROM schema`, reading the table name straight from schema.
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, DynamicTablePtrTp>
    constexpr auto select_from_schema(DynamicTablePtrTp&& schema) {
        return SelectExpr{all(schema->table_name())}.from(std::forward<DynamicTablePtrTp>(schema));
    }
}  // namespace menagerie::db
