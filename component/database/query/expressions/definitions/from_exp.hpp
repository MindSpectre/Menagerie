#pragma once

#include <utility>

#include "basic.hpp"

namespace menagerie::db {
    /// `select FROM table [AS alias]`; the fluent target of a query's `.from(table)`. Exposes GROUP
    /// BY/ORDER BY/LIMIT/JOIN/WHERE as its next possible clauses.
    template <IsQuery Select, IsTable TableT>
    class FromTableExpr : public AliasableExpression<FromTableExpr<Select, TableT>>,
                          public QueryOperations<FromTableExpr<Select, TableT>,
                                                 AllowGroupBy,
                                                 AllowOrderBy,
                                                 AllowLimit,
                                                 AllowJoin,
                                                 AllowWhere>,
                          public TableHolder<TableT> {
    public:
        /// Wraps select_q as the preceding SELECT and table as the source table.
        template <typename SelectTp, typename TableTp>
            requires std::constructible_from<Select, SelectTp> && std::constructible_from<TableT, TableTp>
        constexpr FromTableExpr(SelectTp&& select_q, TableTp&& table) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(table)},
              select_{std::forward<SelectTp>(select_q)} {
        }

        /// The preceding SELECT.
        template <typename Self>
        [[nodiscard]] constexpr auto&& select(this Self&& self) noexcept {
            return std::forward_like<Self>(self.select_);
        }

        /// Splits *this into {select, table, alias} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.select_),
                                         std::forward_like<Self>(self.table),
                                         std::forward_like<Self>(self.alias));
        }

    private:
        Select select_;
    };

    /// `select FROM cte_name`; the fluent target of a query's `.from(cte)` when cte is a CteExpr rather than
    /// a table. Exposes the same next-clause set as FromTableExpr.
    template <IsQuery Select, IsCteExpr CteQuery>
    class FromCteExpr : public Expression<FromCteExpr<Select, CteQuery>>,
                        public QueryOperations<FromCteExpr<Select, CteQuery>,
                                               AllowGroupBy,
                                               AllowOrderBy,
                                               AllowLimit,
                                               AllowJoin,
                                               AllowWhere> {
    public:
        /// Wraps select_q as the preceding SELECT and expr as the source CTE.
        template <typename SelectTp, typename CteQueryTp>
            requires std::constructible_from<Select, SelectTp> && std::constructible_from<CteQuery, CteQueryTp>
        constexpr FromCteExpr(SelectTp&& select_q, CteQueryTp&& expr) noexcept
            : select_{std::forward<SelectTp>(select_q)},
              query_{std::forward<CteQueryTp>(expr)} {
        }

        /// The preceding SELECT.
        template <typename Self>
        [[nodiscard]] constexpr auto&& select(this Self&& self) noexcept {
            return std::forward_like<Self>(self.select_);
        }

        /// The source CTE.
        template <typename Self>
        [[nodiscard]] constexpr auto&& cte_query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// Splits *this into {select, cte_query} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.select_), std::forward_like<Self>(self.query_));
        }

    private:
        Select select_;
        CteQuery query_;
    };
}  // namespace menagerie::db
