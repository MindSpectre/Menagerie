#pragma once

#include "basic.hpp"
#include "delete_exp.hpp"

namespace menagerie::db {
    /// `DELETE FROM table WHERE condition`; built by DeleteExpr::where(cond).
    template <IsTable TableT, IsCondition Condition>
    class DeleteWhereExpr : public Expression<DeleteWhereExpr<TableT, Condition>> {
    public:
        /// Wraps d as the base DELETE and c as the WHERE condition.
        template <typename DeleteExprTp, typename ConditionTp>
        constexpr DeleteWhereExpr(DeleteExprTp&& d, ConditionTp&& c)
            : del_{std::forward<DeleteExprTp>(d)},
              condition_{std::forward<ConditionTp>(c)} {
        }

        /// The base DELETE (target table).
        template <typename Self>
        [[nodiscard]] constexpr auto&& del(this Self&& self) {
            return std::forward_like<Self>(self.del_);
        }

        /// The WHERE condition.
        template <typename Self>
        [[nodiscard]] constexpr auto&& condition(this Self&& self) {
            return std::forward_like<Self>(self.condition_);
        }

        /// Splits *this into {del, condition} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.del_), std::forward_like<Self>(self.condition_));
        }

    private:
        DeleteExpr<TableT> del_;
        Condition condition_;
    };
}  // namespace menagerie::db
