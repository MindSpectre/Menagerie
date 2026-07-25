#pragma once

#include "basic.hpp"

namespace menagerie::db {

    /// `UPDATE table SET col1 = v1, col2 = v2, ...`; built up fluently with one or more `.set(...)` calls.
    /// `.where(cond)` narrows it to an UpdateWhereExpr.
    template <IsTable TableT>
    class UpdateExpr : public Expression<UpdateExpr<TableT>>, public TableHolder<TableT> {
    public:
        /// Wraps t as the target table.
        template <IsTable TableTp = std::remove_cvref_t<TableT>>
        constexpr explicit UpdateExpr(TableTp&& t) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)} {
        }

        /// Appends one `column = value` assignment.
        template <typename Self, typename StringTp, typename FieldValueTp>
            requires std::constructible_from<std::string, StringTp> && std::constructible_from<FieldValue, FieldValueTp>
        constexpr auto&& set(this Self&& self, StringTp&& column, FieldValueTp&& value) {
            self.assignments_.emplace_back(std::forward<StringTp>(column), std::forward<FieldValueTp>(value));
            return std::forward<Self>(self);
        }

        /// @overload
        template <typename Self>
        constexpr auto&& set(this Self&& self,
                             const std::initializer_list<std::pair<std::string, FieldValue>> assigns) {
            for (auto& a : assigns) {
                self.assignments_.push_back(a);
            }
            return std::forward<Self>(self);
        }

        /// Narrows to `UPDATE table SET ... WHERE cond`.
        template <typename Self, IsCondition ConditionTp>
        constexpr auto where(this Self&& self, ConditionTp&& cond) {
            return UpdateWhereExpr<TableT, std::remove_cvref_t<ConditionTp>>{std::forward<Self>(self),
                                                                             std::forward<ConditionTp>(cond)};
        }

        /// The accumulated `column = value` assignments, in call order.
        template <typename Self>
        [[nodiscard]] constexpr auto&& assignments(this Self&& self) noexcept {
            return std::forward_like<Self>(self.assignments_);
        }

        /// Splits *this into {table, assignments} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.table),
                                         std::forward_like<Self>(self.assignments_));
        }

    private:
        std::vector<std::pair<std::string, FieldValue>> assignments_;
    };

    /// Starts an UPDATE for a runtime DynamicTablePtr.
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, DynamicTablePtrTp> &&
                 (!std::constructible_from<std::string, DynamicTablePtrTp>)
    constexpr auto update(DynamicTablePtrTp&& table) {
        return UpdateExpr<DynamicTablePtr>{std::forward<DynamicTablePtrTp>(table)};
    }

    /// @overload
    template <typename StringTp>
        requires std::constructible_from<std::string, StringTp>
    constexpr auto update(StringTp&& table_name) {
        return UpdateExpr<std::string>{std::forward<StringTp>(table_name)};
    }

    /// @overload
    template <typename StringTp>
        requires std::is_same_v<std::remove_cvref_t<StringTp>, std::string_view> ||
                 std::is_same_v<std::remove_cvref_t<StringTp>, const char*>
    constexpr auto update(StringTp&& table_name) noexcept {
        return UpdateExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }

    /// Starts an UPDATE for a compile-time StaticTable.
    template <IsStaticTable TableTp>
    constexpr auto update(const TableTp& table) {
        return UpdateExpr<TableTp>{table};
    }
}  // namespace menagerie::db
