#pragma once

#include <utility>

#include "basic.hpp"

namespace menagerie::db {
    /// `COUNT(column)`, or `COUNT(*)` when ColT is AllColumns; dist selects DISTINCT.
    template <IsColumnLike ColT>
    class CountExpr : public AliasableExpression<CountExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        /// Wraps col as the counted column and dist as whether to emit DISTINCT.
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr CountExpr(ColumnTp&& col, const bool dist) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(col)},
              distinct_{dist} {
        }

        /// Whether this COUNT emits DISTINCT.
        [[nodiscard]] constexpr bool distinct() const noexcept {
            return distinct_;
        }

    private:
        bool distinct_ = false;
    };

    /// `SUM(column)`.
    template <IsColumnLike ColT>
    class SumExpr : public AliasableExpression<SumExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        /// Wraps column as the summed column.
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit SumExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    /// `AVG(column)`.
    template <IsColumnLike ColT>
    class AvgExpr : public AliasableExpression<AvgExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        /// Wraps column as the averaged column.
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit AvgExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    /// `MAX(column)`.
    template <IsColumnLike ColT>
    class MaxExpr : public AliasableExpression<MaxExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        /// Wraps column as the max-aggregated column.
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit MaxExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    /// `MIN(column)`.
    template <IsColumnLike ColT>
    class MinExpr : public AliasableExpression<MinExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        /// Wraps column as the min-aggregated column.
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit MinExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    // Unified factories - IsColumnLike (Column, TypedColumn<T>, AllColumns)

    /// Builds `COUNT(col)`.
    template <IsColumnLike ColT>
    constexpr auto count(ColT&& col) noexcept {
        return CountExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col), false};
    }

    /// Builds `COUNT(DISTINCT col)`.
    template <IsColumnLike ColT>
    constexpr auto count_distinct(ColT&& col) noexcept {
        return CountExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col), true};
    }

    /// Builds `COUNT(*)`.
    constexpr auto count_all() noexcept {
        return CountExpr<AllColumns>{AllColumns{}, false};
    }

    /// Builds `COUNT(DISTINCT *)`.
    constexpr auto count_all_distinct() noexcept {
        return CountExpr<AllColumns>{AllColumns{}, true};
    }

    /// Builds `SUM(col)`.
    template <IsColumnLike ColT>
    constexpr auto sum(ColT&& col) noexcept {
        return SumExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    /// Builds `AVG(col)`.
    template <IsColumnLike ColT>
    constexpr auto avg(ColT&& col) noexcept {
        return AvgExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    /// Builds `MAX(col)`.
    template <IsColumnLike ColT>
    constexpr auto max(ColT&& col) noexcept {
        return MaxExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    /// Builds `MIN(col)`.
    template <IsColumnLike ColT>
    constexpr auto min(ColT&& col) noexcept {
        return MinExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    // Convenience factories - const char* (constructs Column inline)

    /// @overload
    constexpr auto count(const char* name) noexcept {
        return CountExpr<Column>{Column{name}, false};
    }

    /// @overload
    constexpr auto count_distinct(const char* name) noexcept {
        return CountExpr<Column>{Column{name}, true};
    }

    /// @overload
    constexpr auto sum(const char* name) noexcept {
        return SumExpr<Column>{Column{name}};
    }

    /// @overload
    constexpr auto avg(const char* name) noexcept {
        return AvgExpr<Column>{Column{name}};
    }

    /// @overload
    constexpr auto max(const char* name) noexcept {
        return MaxExpr<Column>{Column{name}};
    }

    /// @overload
    constexpr auto min(const char* name) noexcept {
        return MinExpr<Column>{Column{name}};
    }
}  // namespace menagerie::db
