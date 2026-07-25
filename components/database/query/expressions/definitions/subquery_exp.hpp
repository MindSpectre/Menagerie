#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// `(query) [AS alias]`; wraps a query so it can be used as an operand (e.g. inside IN (...) or as a FROM
    /// source) rather than compiled as the top-level statement.
    template <IsQuery Query>
    class Subquery : public AliasableExpression<Subquery<Query>> {
    public:
        /// Wraps q as the wrapped query.
        template <typename QueryTp>
            requires std::constructible_from<Query, QueryTp>
        constexpr explicit Subquery(QueryTp&& q) noexcept
            : query_{std::forward<QueryTp>(q)} {
        }

        /// The wrapped query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// Splits *this into {query, alias} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_), std::forward_like<Self>(self.alias));
        }

    private:
        Query query_;
    };

    /// Wraps query as a Subquery.
    template <typename QueryTp>
    constexpr auto subquery(QueryTp&& query) noexcept {
        return Subquery<std::remove_cvref_t<QueryTp>>{std::forward<QueryTp>(query)};
    }
}  // namespace menagerie::db
