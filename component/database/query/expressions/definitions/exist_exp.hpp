#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// `EXISTS (query)`.
    template <IsQuery Query>
    class ExistsExpr : public Expression<ExistsExpr<Query>> {
    public:
        /// Wraps sq as the subquery to test.
        template <IsQuery SubQueryT>
        constexpr explicit ExistsExpr(SubQueryT&& sq) noexcept
            : query_{std::forward<SubQueryT>(sq)} {
        }

        /// The wrapped subquery.
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        /// Splits *this into {query} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_));
        }

    private:
        Query query_;
    };

    /// Builds `EXISTS (query)`.
    template <IsQuery Query>
    constexpr auto exists(Query&& query) {
        return ExistsExpr<std::remove_cvref_t<Query>>{std::forward<Query>(query)};
    }
}  // namespace menagerie::db
