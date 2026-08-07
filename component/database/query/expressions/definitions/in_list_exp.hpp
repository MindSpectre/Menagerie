#pragma once

#include <tuple>

#include "basic.hpp"

namespace menagerie::db {
    /// `operand IN (values...)`, e.g. `col IN (1, 2, 3)`.
    template <typename Operand, typename... Values>
    class InListExpr : public Expression<InListExpr<Operand, Values...>> {
    public:
        /// Wraps op as the operand and vals as the candidate value list.
        template <typename OperandTp, typename... ValuesTp>
            requires std::constructible_from<Operand, OperandTp> && (std::constructible_from<Values, ValuesTp> && ...)
        constexpr explicit InListExpr(OperandTp&& op, ValuesTp&&... vals) noexcept
            : operand_{std::forward<OperandTp>(op)},
              values_{std::forward<ValuesTp>(vals)...} {
        }

        /// The wrapped operand.
        template <typename Self>
        [[nodiscard]] constexpr auto&& operand(this Self&& self) noexcept {
            return std::forward_like<Self>(self.operand_);
        }

        /// The candidate value list, as a tuple.
        template <typename Self>
        [[nodiscard]] constexpr auto&& values(this Self&& self) noexcept {
            return std::forward_like<Self>(self.values_);
        }

        /// Splits *this into {operand, values} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.operand_), std::forward_like<Self>(self.values_));
        }

    private:
        Operand operand_;
        std::tuple<Values...> values_;
    };

    /// Builds `operand IN (values...)`, auto-wrapping each value in a Literal when it is not already an
    /// expression node. For an IN against a subquery, use in(col, subquery) from condition_exp.hpp instead.
    template <typename OperandTp, typename... ValuesTp>
    constexpr auto in(OperandTp&& operand, ValuesTp&&... values) {
        return InListExpr<std::remove_cvref_t<OperandTp>, decltype(detail::make_literal_if_needed(values))...>{
            std::forward<OperandTp>(operand), detail::make_literal_if_needed(std::forward<ValuesTp>(values))...};
    }
}  // namespace menagerie::db
