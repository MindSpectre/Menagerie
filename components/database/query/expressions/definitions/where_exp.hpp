#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// `query WHERE condition`; built by a query's `.where(cond)`. Exposes GROUP BY/ORDER BY/LIMIT as the
    /// next possible clauses.
    template <IsQuery Query, IsCondition Condition>
    class WhereExpr : public Expression<WhereExpr<Query, Condition>>,
                      public QueryOperations<WhereExpr<Query, Condition>, AllowGroupBy, AllowOrderBy, AllowLimit> {
    public:
        /// Wraps q as the preceding query and c as the WHERE condition.
        template <typename QueryTp, typename ConditionTp>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<Condition, ConditionTp>
        constexpr WhereExpr(QueryTp&& q, ConditionTp&& c) noexcept
            : query_{std::forward<QueryTp>(q)},
              condition_{std::forward<ConditionTp>(c)} {
        }

        /// The preceding query this WHERE narrows.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// The WHERE condition.
        template <typename Self>
        [[nodiscard]] constexpr auto&& condition(this Self&& self) noexcept {
            return std::forward_like<Self>(self.condition_);
        }

        /// Splits *this into {query, condition} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_),
                                         std::forward_like<Self>(self.condition_));
        }

    private:
        Query query_;
        Condition condition_;
    };
}  // namespace menagerie::db
