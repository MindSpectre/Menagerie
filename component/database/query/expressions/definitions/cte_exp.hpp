#pragma once

#include <string>

#include "basic.hpp"

namespace menagerie::db {
    /// `name AS (query)`, optionally `WITH RECURSIVE`; built by with(...)/with_recursive(...) and consumed
    /// through FromCteExpr (a query's `.from(cte)`).
    template <IsQuery Query>
    class CteExpr : public Expression<CteExpr<Query>> {
    public:
        /// Wraps name/q/recursive as the CTE's name, body query, and RECURSIVE flag.
        template <typename StringTp = std::string, typename QueryTp = std::remove_cvref_t<Query>>
        constexpr CteExpr(StringTp&& name, QueryTp&& q, const bool recursive = false) noexcept
            : cte_name_{std::forward<StringTp>(name)},
              query_{std::forward<QueryTp>(q)},
              recursive_{recursive} {
        }

        /// The CTE's name.
        template <typename Self>
        [[nodiscard]] constexpr auto&& name(this Self&& self) noexcept {
            return std::forward_like<Self>(self.cte_name_);
        }

        /// The CTE's body query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// Whether this is a `WITH RECURSIVE` CTE.
        [[nodiscard]] constexpr bool recursive() const noexcept {
            return recursive_;
        }

        /// Splits *this into {name, query, recursive} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.cte_name_),
                                         std::forward_like<Self>(self.query_),
                                         std::forward_like<Self>(self.recursive_));
        }

    private:
        std::string cte_name_;
        Query query_;
        bool recursive_ = false;
    };

    /// Builds `WITH name AS (query)`.
    template <IsQuery Query, typename StringTp = std::string>
    constexpr auto with(StringTp&& name, Query&& query) {
        return CteExpr<std::remove_cvref_t<Query>>{std::forward<StringTp>(name), std::forward<Query>(query), false};
    }

    /// Builds `WITH RECURSIVE name AS (query)`.
    template <IsQuery Query, typename StringTp = std::string>
    constexpr auto with_recursive(StringTp&& name, Query&& query) {
        return CteExpr<std::remove_cvref_t<Query>>{std::forward<StringTp>(name), std::forward<Query>(query), true};
    }
}  // namespace menagerie::db
