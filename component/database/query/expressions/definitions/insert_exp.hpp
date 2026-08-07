#pragma once

#include <algorithm>
#include <format>
#include <stdexcept>

#include "basic.hpp"

namespace menagerie::db {
    namespace detail {
        /**
         * @brief Cold path behind InsertExpr::values(record)/batch(records): reports an into(...) column the
         *        record's schema does not define.
         *
         * Kept out of line and non-template so the message building is emitted once for the program rather
         * than once per InsertExpr<TableT>, and never sits in the caller's row loop. There is exactly one
         * throw on the way out - the lookup that detects the mismatch is Record::get_field, which does not
         * throw, so nothing is caught and rethrown and no exception type beyond the standard one is involved.
         */
        [[noreturn]] inline void
        throw_missing_insert_column(const std::string_view column, const Record& record, const std::size_t row_index) {
            std::string defined;
            std::size_t listed = 0;
            for (const Field& field : record) {
                if (constexpr std::size_t max_listed = 32; listed == max_listed) {
                    defined += std::format(", ... ({} fields total)", record.field_count());
                    break;
                }
                if (listed != 0) {
                    defined += ", ";
                }
                defined += field.name();
                ++listed;
            }
            if (defined.empty()) {
                defined = "<none>";
            }

            throw std::out_of_range{
                std::format(R"(INSERT row {}: into(...) asks for column "{}", but the Record passed carries )"
                            R"(schema "{}", which defines no such field (defines: {}))",
                            row_index,
                            column,
                            record.schema().table_name(),
                            defined)};
        }
    }  // namespace detail

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

        /**
         * @brief Appends one row by reading each into(...) column out of record.
         *
         * Unlike the initializer_list overload this can fail for a reason the caller can act on - the
         * record's schema and the into(...) column list disagreeing - so it is not noexcept.
         *
         * @throw std::out_of_range if record's schema defines no field for one of the into(...) columns.
         *        The message names the row index, the column, the record's schema and the fields that
         *        schema does define. Rows appended by earlier values()/batch() calls are kept, and when
         *        record is an rvalue the fields read before the missing one have already been moved out
         *        of it.
         */
        template <typename Self, typename RecordTp>
            requires std::same_as<std::remove_cvref_t<RecordTp>, Record>
        auto&& values(this Self&& self, RecordTp&& record) {
            std::vector<FieldValue> row;
            row.reserve(self.columns_.size());
            for (const auto& col : self.columns_) {
                // get_field, not operator[]: the non-throwing lookup lets the diagnostic be built here,
                // where the row index and the requesting column are both in hand, instead of inside Record.
                auto* field = record.get_field(col);
                if (field == nullptr) [[unlikely]] {
                    detail::throw_missing_insert_column(col, record, self.rows_.size());
                }
                if constexpr (std::is_rvalue_reference_v<RecordTp&&>) {
                    row.push_back(std::move(*field).raw_value());
                } else {
                    row.push_back(field->raw_value());
                }
            }
            self.rows_.push_back(std::move(row));
            return std::forward<Self>(self);
        }

        /// Appends one row per record via values(record).
        /// @throw std::out_of_range under the same conditions as values(record); the rows appended for the
        ///        records before the failing one are kept.
        template <typename Self, typename RecordsTp>
            requires std::same_as<std::remove_cvref_t<RecordsTp>, std::vector<Record>>
        auto&& batch(this Self&& self, RecordsTp&& records) {
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
