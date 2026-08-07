#pragma once

#include <algorithm>

#include "basic.hpp"

namespace menagerie::db {
    /// `left Op right`, e.g. `left = right` for Op = OpEqual. Left/Right are usually a column and a Literal,
    /// but can be any expression node (nesting binary expressions builds compound conditions).
    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr : public Expression<BinaryExpr<Left, Right, Op>> {
    public:
        /// Wraps l and r as the left/right operands.
        template <typename LeftTp, typename RightTp>
            requires std::constructible_from<Left, LeftTp> && std::constructible_from<Right, RightTp>
        constexpr BinaryExpr(LeftTp&& l, RightTp&& r) noexcept
            : left_(std::forward<LeftTp>(l)),
              right_(std::forward<RightTp>(r)) {
        }

        /// The left-hand operand.
        template <typename Self>
        [[nodiscard]] constexpr auto&& left(this Self&& self) noexcept {
            return std::forward_like<Self>(self.left_);
        }

        /// The right-hand operand.
        template <typename Self>
        [[nodiscard]] constexpr auto&& right(this Self&& self) noexcept {
            return std::forward_like<Self>(self.right_);
        }

        /// Splits *this into {left, right} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.left_), std::forward_like<Self>(self.right_));
        }

    private:
        Left left_;
        Right right_;
    };

    /// `Op operand`, e.g. `NOT operand`, or `operand Op` when Op::is_postfix (e.g. `operand IS NULL`).
    template <typename Operand, IsOperator Op>
    class UnaryExpr : public Expression<UnaryExpr<Operand, Op>> {
    public:
        /// Wraps op as the operand.
        template <typename OperandTp>
            requires std::constructible_from<Operand, OperandTp>
        constexpr explicit UnaryExpr(OperandTp&& op) noexcept
            : operand_(std::forward<OperandTp>(op)) {
        }

        /// The wrapped operand.
        template <typename Self>
        [[nodiscard]] constexpr auto&& operand(this Self&& self) noexcept {
            return std::forward_like<Self>(self.operand_);
        }

    private:
        Operand operand_;
    };

    /// Builds `NOT operand`.
    template <IsDbOperand T>
    constexpr auto operator!(T operand) {
        return UnaryExpr<T, OpNot>{std::move(operand)};
    }

    // Helper functions for special operators
    /// Builds `operand IS NULL`.
    template <IsDbOperand T>
    constexpr auto is_null(T operand) {
        return UnaryExpr<T, OpIsNull>{std::move(operand)};
    }

    /// Builds `operand IS NOT NULL`.
    template <IsDbOperand T>
    constexpr auto is_not_null(T operand) {
        return UnaryExpr<T, OpIsNotNull>{std::move(operand)};
    }

    // Comparison operators - constrained to require at least one database operand
    /// Builds `left = right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator==(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpEqual>{std::move(lv), std::move(rv)};
    }

    /// Builds `left != right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator!=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpNotEqual>{std::move(lv), std::move(rv)};
    }

    /// Builds `left < right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator<(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLess>{std::move(lv), std::move(rv)};
    }

    /// Builds `left <= right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator<=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLessEqual>{std::move(lv), std::move(rv)};
    }

    /// Builds `left > right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator>(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpGreater>{std::move(lv), std::move(rv)};
    }

    /// Builds `left >= right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator>=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpGreaterEqual>{std::move(lv), std::move(rv)};
    }

    // Logical operators - constrained to require at least one database operand
    /// Builds `left AND right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator&&(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpAnd>{std::move(lv), std::move(rv)};
    }

    /// Builds `left OR right`, auto-wrapping either side in a Literal when it is not already an expression
    /// node.
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator||(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpOr>{std::move(lv), std::move(rv)};
    }

    /// Builds `left LIKE right`.
    template <typename L, typename R>
    constexpr auto like(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLike>{std::move(lv), std::move(rv)};
    }

    /// Builds `left NOT LIKE right`.
    template <typename L, typename R>
    constexpr auto not_like(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpNotLike>{std::move(lv), std::move(rv)};
    }

    // IN with subquery - for variadic IN with individual values, use in() from in_list_exp.hpp
    /// Builds `col IN (sq)` against a subquery; for a fixed value list use in(...) from in_list_exp.hpp.
    template <typename ColumnTp, IsQuery Query>
        requires IsColumnLike<ColumnTp>
    constexpr auto in(const ColumnTp& col, const Subquery<Query>& sq) {
        return BinaryExpr<ColumnTp, Subquery<Query>, OpIn>{col, sq};
    }
}  // namespace menagerie::db
