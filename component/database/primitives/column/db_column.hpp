#pragma once
#include <menagerie/beavers>
#include <string>
#include <string_view>
#include <utility>

#include "db_typed_column.hpp"

namespace menagerie::db {

    /// An untyped column reference: a name, optionally table-qualified and
    /// aliased. Produced directly (via col()) or from a TypedColumn/
    /// StaticTable::column<>() when its compile-time value type is not
    /// needed by the caller.
    class Column {
    public:
        /// Constructs a column named `name`, qualified by `table`.
        template <beavers::IsStringLike StringTp1, beavers::IsStringLike StringTp2>
        constexpr explicit Column(StringTp1&& name, StringTp2&& table)
            : name_{std::forward<StringTp1>(name)},
              table_{std::forward<StringTp2>(table)} {
        }

        /// Constructs a column named `name`, with no table qualifier.
        template <beavers::IsStringLike StringTp>
        constexpr explicit Column(StringTp&& name)
            : name_{std::forward<StringTp>(name)} {
        }

        /// The column's name.
        [[nodiscard]] constexpr std::string_view name() const noexcept {
            return name_;
        }

        /// The column's table qualifier, or empty if unqualified.
        [[nodiscard]] constexpr std::string_view table_name() const noexcept {
            return table_;
        }

        /// The column's SQL alias, or empty if none was set.
        [[nodiscard]] constexpr std::string_view alias() const noexcept {
            return alias_;
        }

        /// Sets the table qualifier; returns *this for chaining.
        template <beavers::IsStringLike StringTp>
        constexpr Column& set_table(StringTp&& table) {
            table_ = std::forward<StringTp>(table);
            return *this;
        }

        /// Sets the column name; returns *this for chaining.
        template <beavers::IsStringLike StringTp>
        constexpr Column& set_name(StringTp&& name) {
            name_ = std::forward<StringTp>(name);
            return *this;
        }

        /// Returns a copy of this column with `alias` as its SQL alias.
        template <beavers::IsStringLike StringTp>
        [[nodiscard]] constexpr Column as(StringTp&& alias) const {
            Column result{name_, table_};
            result.alias_ = std::forward<StringTp>(alias);
            return result;
        }

        /// Dispatches to `visitor.visit(*this)` for expression-tree traversal/rendering.
        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    private:
        std::string name_;
        std::string table_{};
        std::string alias_{};
    };

    /// A `table.*` (or bare `*`) column-list wildcard, usable anywhere an
    /// expression operand is expected.
    class AllColumns {
    public:
        /// Constructs a `table.*` wildcard.
        template <beavers::IsStringLike StringTp>
        constexpr explicit AllColumns(StringTp&& table)
            : table_{std::forward<StringTp>(table)} {
        }

        constexpr AllColumns() noexcept = default;

        /// The table this wildcard is qualified by, or empty for a bare `*`.
        [[nodiscard]] constexpr const std::string& table_name() const noexcept {
            return table_;
        }

        /// Renders this wildcard as a Column named "*".
        [[nodiscard]] constexpr Column as_column() const {
            return Column{"*", table_name()};
        }

        /// Dispatches to `visitor.visit(*this)` for expression-tree traversal/rendering.
        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    private:
        std::string table_;
    };

    template <typename CppType>
    [[nodiscard]] constexpr Column TypedColumn<CppType>::as_column() const {
        return Column{std::string{name_}, std::string{table_}};
    }

    /// Constructs an untyped Column named `name`, with no table qualifier.
    constexpr Column col(const char* name) {
        return Column{std::string{name}};
    }

    /// Constructs an untyped Column named `name`, qualified by `table`.
    constexpr Column col(const char* name, const char* table) {
        return Column{std::string{name}, std::string{table}};
    }

    /// Constructs a `table.*` wildcard.
    constexpr AllColumns all(std::string table) {
        return AllColumns{std::move(table)};
    }

    /// Constructs a bare `*` wildcard.
    constexpr AllColumns all() {
        return AllColumns{};
    }

    /// Whether T is (after removing cv/ref) AllColumns.
    template <typename T>
    concept IsAllColumns = std::is_same_v<std::remove_cvref_t<T>, AllColumns>;

    /// Whether T is (after removing cv/ref) Column.
    template <typename T>
    concept IsColumn = std::is_same_v<std::remove_cvref_t<T>, Column>;

    /// Whether T behaves like a column operand: exposes name()/table_name()/
    /// alias(), or is an AllColumns wildcard.
    template <typename T>
    concept IsColumnLike = (requires(const std::remove_cvref_t<T>& c) {
                               { c.name() } -> std::convertible_to<std::string_view>;
                               { c.table_name() } -> std::convertible_to<std::string_view>;
                               { c.alias() } -> std::convertible_to<std::string_view>;
                           }) || IsAllColumns<T>;

}  // namespace menagerie::db
