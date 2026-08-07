#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// `left op right`, e.g. `left UNION right`. Exposes ORDER BY/LIMIT as the next possible clauses.
    template <IsQuery Left, IsQuery Right>
    class SetOpExpr : public Expression<SetOpExpr<Left, Right>>,
                      public QueryOperations<SetOpExpr<Left, Right>, AllowOrderBy, AllowLimit> {
    public:
        /// Wraps l/r as the left/right queries and o as the combining operation.
        template <typename LeftTp, typename RightTp>
            requires std::constructible_from<Left, LeftTp> && std::constructible_from<Right, RightTp>
        constexpr SetOpExpr(LeftTp&& l, RightTp&& r, const SetOperation o) noexcept
            : left_{std::forward<LeftTp>(l)},
              right_{std::forward<RightTp>(r)},
              op_{o} {
        }

        /// The left-hand query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& left(this Self&& self) noexcept {
            return std::forward_like<Self>(self.left_);
        }

        /// The right-hand query.
        template <typename Self>
        [[nodiscard]] constexpr auto&& right(this Self&& self) noexcept {
            return std::forward_like<Self>(self.right_);
        }

        /// The set-combination operation (UNION/UNION ALL/INTERSECT/EXCEPT).
        [[nodiscard]] constexpr SetOperation op() const noexcept {
            return op_;
        }

        /// Splits *this into {left, right, op} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.left_),
                                         std::forward_like<Self>(self.right_),
                                         std::forward_like<Self>(self.op_));
        }

    private:
        Left left_;
        Right right_;
        SetOperation op_;
    };

    // Set operation functions
    /// Builds `left UNION right`.
    template <typename LeftTp, typename RightTp>
    constexpr auto union_query(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::UNION};
    }

    /// Builds `left UNION ALL right`.
    template <typename LeftTp, typename RightTp>
    constexpr auto union_all(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::UNION_ALL};
    }

    /// Builds `left INTERSECT right`.
    template <typename LeftTp, typename RightTp>
    constexpr auto intersect(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::INTERSECT};
    }

    /// Builds `left EXCEPT right`.
    template <typename LeftTp, typename RightTp>
    constexpr auto except(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::EXCEPT};
    }

    // Set operation binary operators
    // | for UNION (set union without duplicates)
    /// Builds `left UNION right`.
    template <IsQuery L, IsQuery R>
    constexpr auto operator|(L&& left, R&& right) {
        return union_query(std::forward<L>(left), std::forward<R>(right));
    }

    // + for UNION ALL (set union with duplicates)
    /// Builds `left UNION ALL right`.
    template <IsQuery L, IsQuery R>
    constexpr auto operator+(L&& left, R&& right) {
        return union_all(std::forward<L>(left), std::forward<R>(right));
    }

    // & for INTERSECT (set intersection)
    /// Builds `left INTERSECT right`.
    template <IsQuery L, IsQuery R>
    constexpr auto operator&(L&& left, R&& right) {
        return intersect(std::forward<L>(left), std::forward<R>(right));
    }

    // - for EXCEPT (set difference)
    /// Builds `left EXCEPT right`.
    template <IsQuery L, IsQuery R>
    constexpr auto operator-(L&& left, R&& right) {
        return except(std::forward<L>(left), std::forward<R>(right));
    }
}  // namespace menagerie::db
