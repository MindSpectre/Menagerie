#pragma once

#include <menagerie/beavers>

#include "basic.hpp"

namespace menagerie::db {

    /// `DROP TABLE [IF EXISTS] table [CASCADE]`.
    template <IsTable TableT>
    class DropTableExpr : public Expression<DropTableExpr<TableT>>, public TableHolder<TableT> {
    public:
        /// Wraps t as the table to drop, with if_exists/cascade selecting `IF EXISTS`/`CASCADE`.
        template <typename TableTp>
            requires std::constructible_from<TableT, TableTp>
        constexpr explicit DropTableExpr(TableTp&& t, const bool if_exists = false, const bool cascade = false) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)},
              if_exists_{if_exists},
              cascade_{cascade} {
        }

        /// Whether this emits `IF EXISTS`.
        [[nodiscard]] constexpr bool if_exists() const noexcept {
            return if_exists_;
        }

        /// Whether this emits `CASCADE`.
        [[nodiscard]] constexpr bool cascade() const noexcept {
            return cascade_;
        }

        /// Splits *this into {table, if_exists, cascade} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.table),
                                         std::forward_like<Self>(self.if_exists_),
                                         std::forward_like<Self>(self.cascade_));
        }

    private:
        bool if_exists_ = false;
        bool cascade_   = false;
    };

    // Factory 1: DynamicTablePtr
    /// Builds DROP TABLE for a runtime DynamicTablePtr.
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, std::remove_cvref_t<DynamicTablePtrTp>> &&
                 (!beavers::IsStringLike<DynamicTablePtrTp>)
    constexpr auto
    drop_table(DynamicTablePtrTp&& table, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<DynamicTablePtr>{std::forward<DynamicTablePtrTp>(table), if_exists, cascade};
    }

    // Factory 2: std::string
    /// @overload
    template <typename StringTp>
        requires beavers::IsStringLike<StringTp> && (!beavers::IsStringViewLike<StringTp>)
    constexpr auto
    drop_table(StringTp&& table_name, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<std::string>{std::forward<StringTp>(table_name), if_exists, cascade};
    }

    // Factory 3: string_view
    /// @overload
    template <typename StringTp>
        requires beavers::IsStringViewLike<StringTp>
    constexpr auto
    drop_table(StringTp&& table_name, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<std::string_view>{std::forward<StringTp>(table_name), if_exists, cascade};
    }

    // Factory 4: StaticTable
    /// Builds DROP TABLE for a compile-time StaticTable.
    template <IsStaticTable StaticTableTp>
    constexpr auto
    drop_table(const StaticTableTp& table, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<StaticTableTp>{table, if_exists, cascade};
    }

}  // namespace menagerie::db
