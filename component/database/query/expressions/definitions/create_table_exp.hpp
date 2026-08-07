#pragma once

#include <menagerie/beavers>

#include "basic.hpp"

namespace menagerie::db {

    /// `CREATE TABLE [IF NOT EXISTS] table (...)`. Column definitions are read from TableT's schema at
    /// compilation time (see SqlGeneratorVisitor::visit_create_table_columns), not stored here.
    template <IsTable TableT>
    class CreateTableExpr : public Expression<CreateTableExpr<TableT>>, public TableHolder<TableT> {
    public:
        /// Wraps t as the table to create, with if_not_exists selecting `IF NOT EXISTS`.
        template <IsTable TableTp = std::remove_cvref_t<TableT>>
        constexpr explicit CreateTableExpr(TableTp&& t, const bool if_not_exists = false) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)},
              if_not_exists_{if_not_exists} {
        }

        /// Whether this emits `IF NOT EXISTS`.
        [[nodiscard]] constexpr bool if_not_exists() const noexcept {
            return if_not_exists_;
        }

        /// Splits *this into {table, if_not_exists} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.table),
                                         std::forward_like<Self>(self.if_not_exists_));
        }

    private:
        bool if_not_exists_ = false;
    };

    // Factory: DynamicTablePtr (requires full schema for DDL generation)
    /// Builds CREATE TABLE for a runtime DynamicTable schema.
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, std::remove_cvref_t<DynamicTablePtrTp>> &&
                 (!beavers::IsStringLike<DynamicTablePtrTp>)
    constexpr auto create_table(DynamicTablePtrTp&& table, const bool if_not_exists = false) noexcept {
        return CreateTableExpr<DynamicTablePtr>{std::forward<DynamicTablePtrTp>(table), if_not_exists};
    }

    // Factory: StaticTable
    /// Builds CREATE TABLE for a compile-time StaticTable schema.
    template <IsStaticTable StaticTableTp>
    constexpr auto create_table(const StaticTableTp& table, const bool if_not_exists = false) noexcept {
        return CreateTableExpr<StaticTableTp>{table, if_not_exists};
    }

}  // namespace menagerie::db
