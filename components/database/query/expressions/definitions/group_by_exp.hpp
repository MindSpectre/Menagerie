#pragma once

#include "basic.hpp"

namespace menagerie::db {
    // Common base class for GroupBy expressions
    /// CRTP base shared by GroupByColumnExpr and GroupByQueryExpr: owns the pre-GROUP-BY query and exposes
    /// HAVING/ORDER BY/LIMIT as the next possible clauses.
    template <typename Derived, typename PreGroupQuery>
    class GroupByExprBase : public Expression<Derived>,
                            public QueryOperations<Derived, AllowHaving, AllowOrderBy, AllowLimit> {
    public:
        /// Wraps q as the pre-GROUP-BY query.
        template <typename PreGroupQueryTp>
            requires std::constructible_from<PreGroupQuery, PreGroupQueryTp>
        constexpr explicit GroupByExprBase(PreGroupQueryTp&& q) noexcept
            : query_{std::forward<PreGroupQueryTp>(q)} {
        }

        /// The pre-GROUP-BY query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

    protected:
        PreGroupQuery query_;  ///< The pre-GROUP-BY query.
    };

    // Specialized for column-based grouping
    /// `query GROUP BY col1, col2, ...`; built by a query's `.group_by(cols...)`.
    template <IsQuery PreGroupQuery, IsColumnLike... GroupColumns>
    class GroupByColumnExpr : public GroupByExprBase<GroupByColumnExpr<PreGroupQuery, GroupColumns...>, PreGroupQuery> {
        using Base = GroupByExprBase<GroupByColumnExpr, PreGroupQuery>;

    public:
        /// Wraps q as the pre-GROUP-BY query and cols as the grouping columns.
        template <typename PreGroupQueryTp, typename... GroupColumnsTp>
            requires std::constructible_from<PreGroupQuery, PreGroupQueryTp> &&
                         (std::constructible_from<GroupColumns, GroupColumnsTp> && ...)
        constexpr explicit GroupByColumnExpr(PreGroupQueryTp&& q, GroupColumnsTp&&... cols) noexcept
            : Base{std::forward<PreGroupQueryTp>(q)},
              columns_{std::forward<GroupColumnsTp>(cols)...} {
        }

        /// The grouping columns, as a tuple.
        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) noexcept {
            return std::forward_like<Self>(self.columns_);
        }

        /// Splits *this into {query, columns} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_), std::forward_like<Self>(self.columns_));
        }

    private:
        std::tuple<GroupColumns...> columns_;
    };

    // Specialized for query-based grouping
    /// `query GROUP BY (criteria)`, grouping by a sub-query's own grouping expression rather than a fixed
    /// column list; built by a query's `.group_by(criteria_query)`.
    template <IsQuery PreGroupQuery, IsQuery GroupingCriteria>
    class GroupByQueryExpr : public GroupByExprBase<GroupByQueryExpr<PreGroupQuery, GroupingCriteria>, PreGroupQuery> {
        using Base = GroupByExprBase<GroupByQueryExpr, PreGroupQuery>;

    public:
        /// Wraps q as the pre-GROUP-BY query and criteria as the grouping sub-query.
        template <typename GroupByExprBaseTp, typename GroupingCriteriaTp>
            requires std::constructible_from<PreGroupQuery, GroupByExprBaseTp> &&
                         std::constructible_from<GroupingCriteria, GroupingCriteriaTp>
        constexpr GroupByQueryExpr(GroupByExprBaseTp&& q, GroupingCriteriaTp&& criteria) noexcept
            : Base{std::forward<GroupByExprBaseTp>(q)},
              grouping_criteria_{std::forward<GroupingCriteriaTp>(criteria)} {
        }

        /// The grouping sub-query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& criteria(this Self&& self) noexcept {
            return std::forward_like<Self>(self.grouping_criteria_);
        }

        /// Splits *this into {query, criteria} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_),
                                         std::forward_like<Self>(self.grouping_criteria_));
        }

    private:
        GroupingCriteria grouping_criteria_;
    };
}  // namespace menagerie::db
