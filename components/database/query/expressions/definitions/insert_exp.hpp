#pragma once

#include <algorithm>

#include "basic.hpp"

namespace menagerie::db {
    /// `INSERT INTO table (columns...) VALUES (row1...), (row2...), ...`; built up fluently with
    /// `.into({cols...})` then one or more `.values(...)`/`.batch(...)` calls.
    template <IsTable TableT>
    class InsertExpr : public Expression<InsertExpr<TableT>>, public TableHolder<TableT> {
    public:
        /// Wraps t as the target table.
        template <typename TableTp>
            requires std::constructible_from<TableT, TableTp>
        constexpr explicit InsertExpr(TableTp&& t) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)} {
        }

        /// Sets the column list rows are inserted into.
        template <typename Self>
        constexpr auto&& into(this Self&& self, const std::initializer_list<std::string> cols) noexcept {
            self.columns_ = cols;
            return std::forward<Self>(self);
        }

        /// Appends one row of literal values, in the same order as the columns passed to into(...).
        template <typename Self>
        constexpr auto&& values(this Self&& self, std::initializer_list<FieldValue> vals) noexcept {
            self.rows_.emplace_back(vals);
            return std::forward<Self>(self);
        }

        /// Appends one row by reading each into(...) column out of record.
        /// @warning `record[col]` throws std::out_of_range for a missing field, but this function is
        ///          noexcept - a missing field TERMINATES the program. Ensure the record's schema matches
        ///          the insert columns.
        template <typename Self, typename RecordTp>
            requires std::same_as<std::remove_cvref_t<RecordTp>, Record>
        auto&& values(this Self&& self, RecordTp&& record) noexcept {
            std::vector<FieldValue> row;
            row.reserve(self.columns_.size());
            for (const auto& col : self.columns_) {
                if constexpr (std::is_rvalue_reference_v<RecordTp&&>) {
                    row.push_back(std::move(record[col]).raw_value());
                } else {
                    row.push_back(record[col].raw_value());
                }
            }
            self.rows_.push_back(std::move(row));
            return std::forward<Self>(self);
        }

        /// Appends one row per record via values(record).
        /// @warning Same noexcept/std::out_of_range hazard as values(record): a record missing one of the
        ///          into(...) columns TERMINATES the program rather than throwing out to the caller.
        template <typename Self, typename RecordsTp>
            requires std::same_as<std::remove_cvref_t<RecordsTp>, std::vector<Record>>
        auto&& batch(this Self&& self, RecordsTp&& records) noexcept {
            if constexpr (std::is_rvalue_reference_v<RecordsTp&&>) {
                for (auto&& record : records) {
                    self.values(std::move(record));
                }
            } else {
                for (const auto& record : records) {
                    self.values(record);
                }
            }
            return std::forward<Self>(self);
        }

        /// The column list rows are inserted into.
        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) noexcept {
            return std::forward_like<Self>(self.columns_);
        }

        /// The accumulated rows, each in the same order as columns().
        template <typename Self>
        [[nodiscard]] constexpr auto&& rows(this Self&& self) noexcept {
            return std::forward_like<Self>(self.rows_);
        }

        /// Splits *this into {table, columns, rows} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.table),
                                         std::forward_like<Self>(self.columns_),
                                         std::forward_like<Self>(self.rows_));
        }

    private:
        std::vector<std::string> columns_;
        std::vector<std::vector<FieldValue>> rows_;
    };

    // INSERT builder function
    /// Starts an INSERT INTO for a runtime DynamicTablePtr.
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, std::remove_cvref_t<DynamicTablePtrTp>>
    constexpr auto insert_into(DynamicTablePtrTp&& table) noexcept {
        return InsertExpr<DynamicTablePtr>{std::forward<DynamicTablePtrTp>(table)};
    }

    /// @overload
    template <typename StringTp>
        requires std::constructible_from<std::string, std::remove_cvref_t<StringTp>>
    constexpr auto insert_into(StringTp&& table_name) noexcept {
        return InsertExpr<std::string>{std::forward<StringTp>(table_name)};
    }

    /// @overload
    template <typename StringTp>
        requires std::is_same_v<std::remove_cvref_t<StringTp>, std::string_view> ||
                 std::is_same_v<std::remove_cvref_t<StringTp>, const char*>
    constexpr auto insert_into(StringTp&& table_name) noexcept {
        return InsertExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }

    /// Starts an INSERT INTO for a compile-time StaticTable.
    template <IsStaticTable TableTp>
    constexpr auto insert_into(const TableTp& table) noexcept {
        return InsertExpr<TableTp>{table};
    }
}  // namespace menagerie::db
