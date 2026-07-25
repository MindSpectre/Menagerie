#pragma once

#include "db_dynamic_table.hpp"
#include "db_static_table.hpp"

namespace menagerie::db {

    /// The shared-ownership handle every Record and runtime table consumer
    /// holds a DynamicTable schema through.
    using DynamicTablePtr = std::shared_ptr<const DynamicTable>;

    /// Whether TablePtrT can construct a DynamicTablePtr.
    template <typename TablePtrT>
    concept IsDynamicTablePtr = std::constructible_from<DynamicTablePtr, std::remove_cvref_t<TablePtrT>>;

    /// Whether T is a StaticTable-shaped compile-time table: exposes
    /// table_name()/field_count()/provider() and is neither a
    /// DynamicTablePtr nor a bare table-name string.
    template <typename T>
    concept IsStaticTable = requires(const T& t) {
        { t.table_name() } -> std::convertible_to<std::string_view>;
        { t.field_count() } -> std::convertible_to<std::size_t>;
        { t.provider() } -> std::convertible_to<Providers>;
    } && !IsDynamicTablePtr<T> && !std::constructible_from<std::string, std::remove_cvref_t<T>>;

    /// Whether TableT is usable as a table operand: a DynamicTablePtr, a
    /// StaticTable, or a bare table-name string(_view).
    template <typename TableT>
    concept IsTable = IsDynamicTablePtr<std::remove_cvref_t<TableT>> || IsStaticTable<std::remove_cvref_t<TableT>> ||
                      std::constructible_from<std::string, std::remove_cvref_t<TableT>> ||
                      std::constructible_from<std::string_view, std::remove_cvref_t<TableT>>;
}  // namespace menagerie::db
