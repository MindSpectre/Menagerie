#pragma once

#include <menagerie/beavers>
#include <string>
#include <string_view>

namespace menagerie::db {

    class Column;

    /**
     * @brief A column reference carrying a compile-time C++ type tag.
     *
     * Like Column, but carries `value_type` (CppType - the C++ type this
     * column maps to: int, std::string, bool, ...) for type-safe parameter
     * binding and compile-time type assertions. Produced by
     * StaticTable::column<>().
     */
    template <typename CppType>
    class TypedColumn {
    public:
        using value_type = CppType;  ///< The C++ type this column maps to.

        /// Constructs a column named `name`, qualified by `table`.
        template <beavers::IsStringLike StringTp1, beavers::IsStringLike StringTp2>
        constexpr TypedColumn(StringTp1&& name, StringTp2&& table)
            : name_{std::forward<StringTp1>(name)},
              table_{std::forward<StringTp2>(table)} {
        }

        /// Constructs a column named `name`, qualified by `table`, with SQL alias `alias`.
        template <beavers::IsStringLike StringTp1, beavers::IsStringLike StringTp2, beavers::IsStringLike StringTp3>
        constexpr TypedColumn(StringTp1&& name, StringTp2&& table, StringTp3&& alias)
            : name_{std::forward<StringTp1>(name)},
              table_{std::forward<StringTp2>(table)},
              alias_{std::forward<StringTp3>(alias)} {
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

        /// Returns a copy of this column with `alias` as its SQL alias.
        template <beavers::IsStringLike StringTp>
        [[nodiscard]] constexpr TypedColumn as(StringTp&& alias) const {
            return TypedColumn{name_, table_, std::forward<StringTp>(alias)};
        }

        /// Erases the compile-time value type, returning an untyped Column.
        [[nodiscard]] constexpr Column as_column() const;

        /// Dispatches to `visitor.visit(*this)` for expression-tree traversal/rendering.
        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    private:
        std::string name_;
        std::string table_;
        std::string alias_;
    };

    /// Whether T is a TypedColumn: exposes `value_type` and the column
    /// accessors, and is not itself an untyped Column.
    template <typename T>
    concept IsTypedColumn = requires { typename T::value_type; } && requires(const T& t) {
        { t.name() } -> std::convertible_to<std::string_view>;
        { t.table_name() } -> std::convertible_to<std::string_view>;
        { t.alias() } -> std::convertible_to<std::string_view>;
    } && !std::is_same_v<std::remove_cvref_t<T>, Column>;

}  // namespace menagerie::db
