#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// `query LIMIT count OFFSET offset`; the terminal clause of a fluent query chain (no further
    /// QueryOperations are exposed).
    template <IsQuery Query>
    class LimitExpr : public Expression<LimitExpr<Query>> {
    public:
        /// Wraps query as the preceding query, with limit/offset as the row window.
        template <typename QueryTp>
            requires std::constructible_from<Query, QueryTp>
        constexpr LimitExpr(QueryTp&& query, const std::size_t limit, const std::size_t offset) noexcept
            : query_{std::forward<QueryTp>(query)},
              count_{limit},
              offset_{offset} {
        }

        /// The preceding query this LIMIT narrows.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// The maximum number of rows to return.
        [[nodiscard]] constexpr std::size_t count() const noexcept {
            return count_;
        }

        /// The number of rows to skip before returning results.
        [[nodiscard]] constexpr std::size_t offset() const noexcept {
            return offset_;
        }

        /// Returns a copy of *this with the offset replaced by new_offset.
        template <typename Self>
        [[nodiscard]] constexpr auto offset(this Self&& self, const std::size_t new_offset) noexcept {
            return LimitExpr{std::forward_like<Self>(self.query_), self.count_, new_offset};
        }

        /// Splits *this into {query, count, offset} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_),
                                         std::forward_like<Self>(self.count_),
                                         std::forward_like<Self>(self.offset_));
        }

    private:
        Query query_;
        std::size_t count_;
        std::size_t offset_;
    };
}  // namespace menagerie::db
