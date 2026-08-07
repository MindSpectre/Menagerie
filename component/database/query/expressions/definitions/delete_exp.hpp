#pragma once

#include <menagerie/beavers>

#include "basic.hpp"

namespace menagerie::db {
    /// `DELETE FROM table`. `.where(cond)` narrows it to a DeleteWhereExpr.
    template <IsTable TableT>
    class DeleteExpr : public Expression<DeleteExpr<TableT>>, public TableHolder<TableT> {
    public:
        /// Wraps t as the target table.
        template <IsTable TableTp = std::remove_cvref_t<TableT>>
        constexpr explicit DeleteExpr(TableTp&& t)
            : TableHolder<TableT>{std::forward<TableTp>(t)} {
        }

        /// Narrows to `DELETE FROM table WHERE cond`.
        template <typename Self, IsCondition ConditionTp>
        [[nodiscard]] constexpr auto where(this Self&& self, ConditionTp&& cond) noexcept {
            return DeleteWhereExpr<TableT, std::remove_cvref_t<ConditionTp>>{std::forward<Self>(self),
                                                                             std::forward<ConditionTp>(cond)};
        }
    };

    // 1. Pointer
    /// Builds `DELETE FROM table` for a runtime DynamicTablePtr.
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, std::remove_cvref_t<DynamicTablePtrTp>> &&
                 (!beavers::IsStringLike<DynamicTablePtrTp>)
    auto delete_from(DynamicTablePtrTp&& table) noexcept {
        return DeleteExpr<DynamicTablePtr>{std::forward<DynamicTablePtrTp>(table)};
    }

    // 2. std::string (general)
    /// @overload
    template <typename StringTp>
        requires beavers::IsStringLike<StringTp> && (!beavers::IsStringViewLike<StringTp>)
    constexpr auto delete_from(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string>{std::forward<StringTp>(table_name)};
    }

    // 3. string_view (more specific - wins due to subsumption)
    /// @overload
    template <typename StringTp>
        requires beavers::IsStringViewLike<StringTp>
    constexpr auto delete_from(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }

    // 4. Static table
    /// Builds `DELETE FROM table` for a compile-time StaticTable.
    template <IsStaticTable TableTp>
    constexpr auto delete_from(const TableTp& table) noexcept {
        return DeleteExpr<TableTp>{table};
    }
}  // namespace menagerie::db
