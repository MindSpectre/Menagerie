#pragma once

#include <algorithm>

#include "basic.hpp"

namespace menagerie::db {
    /// `query HAVING condition`; built by a GROUP BY query's `.having(cond)`. Exposes ORDER BY/LIMIT as the
    /// next possible clauses.
    template <IsQuery Query, IsCondition Condition>
    class HavingExpr : public Expression<HavingExpr<Query, Condition>>,
                       public QueryOperations<HavingExpr<Query, Condition>, AllowOrderBy, AllowLimit> {
    public:
        /// Wraps q as the preceding GROUP BY query and c as the HAVING condition.
        template <typename QueryTp, typename ConditionTp>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<Condition, ConditionTp>
        constexpr HavingExpr(QueryTp&& q, ConditionTp&& c)
            : query_{std::forward<QueryTp>(q)},
              condition_{std::forward<ConditionTp>(c)} {
        }

        /// The preceding GROUP BY query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// The HAVING condition.
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
