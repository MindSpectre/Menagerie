#pragma once

#include <menagerie/beavers>
#include <string_view>

namespace menagerie::db {
    /// Field-schema constraint tags, applied as StaticFieldSchema<CppType,
    /// Name, Constraints...> template arguments.
    namespace constraints {
        // Simple tag constraints
        struct PrimaryKey {};  ///< Marks the field as the table's primary key.
        struct NotNull {};     ///< Marks the field as non-nullable.
        struct Unique {};      ///< Marks the field as UNIQUE.
        struct Indexed {};     ///< Marks the field as indexed.

        // Parameterized constraints
        /// References `Column` in `Table`.
        template <beavers::FixedString Table, beavers::FixedString Column>
        struct ForeignKey {};

        /// Attaches a literal default value.
        template <beavers::FixedString Value>
        struct Default {};

        /// Caps the field's maximum length at N.
        template <std::size_t N>
        struct MaxLength {};

        /// Overrides the field's inferred SQL type with a literal one.
        template <beavers::FixedString SqlType>
        struct DbType {};

    }  // namespace constraints

    /// Compile-time detection/extraction helpers for the parameterized
    /// constraints::ForeignKey/Default/MaxLength/DbType tags within a
    /// Constraints... pack.
    namespace detail {


        /// Whether T is a constraints::ForeignKey<...> specialization.
        template <typename T>
        struct is_foreign_key : std::false_type {};

        template <beavers::FixedString T, beavers::FixedString C>
        struct is_foreign_key<constraints::ForeignKey<T, C>> : std::true_type {};

        /// Whether T is a constraints::Default<...> specialization.
        template <typename T>
        struct is_default : std::false_type {};

        template <beavers::FixedString V>
        struct is_default<constraints::Default<V>> : std::true_type {};

        /// Whether T is a constraints::MaxLength<...> specialization.
        template <typename T>
        struct is_max_length : std::false_type {};

        template <std::size_t N>
        struct is_max_length<constraints::MaxLength<N>> : std::true_type {};

        /// Whether T is a constraints::DbType<...> specialization.
        template <typename T>
        struct is_db_type : std::false_type {};

        template <beavers::FixedString S>
        struct is_db_type<constraints::DbType<S>> : std::true_type {};

        /// Finds the ForeignKey<...> constraint in Cs..., if any; found is
        /// true and table()/column() are available only when one is present.
        template <typename... Cs>
        struct extract_foreign_key {
            static constexpr bool found = false;  ///< True only when the pack contains a ForeignKey constraint.
        };

        template <typename First, typename... Rest>
        struct extract_foreign_key<First, Rest...> : extract_foreign_key<Rest...> {};

        /// Match case: a ForeignKey<Table, Column> constraint was found in the pack.
        template <beavers::FixedString Table, beavers::FixedString Column, typename... Rest>
        struct extract_foreign_key<constraints::ForeignKey<Table, Column>, Rest...> {
            static constexpr bool found = true;  ///< Always true for this match case.
            /// The referenced table name.
            static constexpr std::string_view table() noexcept {
                return Table.view();
            }
            /// The referenced column name.
            static constexpr std::string_view column() noexcept {
                return Column.view();
            }
        };

        /// Finds the Default<...> constraint in Cs..., if any; found is true
        /// and value() is available only when one is present.
        template <typename... Cs>
        struct extract_default {
            static constexpr bool found = false;  ///< True only when the pack contains a Default constraint.
        };

        template <typename First, typename... Rest>
        struct extract_default<First, Rest...> : extract_default<Rest...> {};

        /// Match case: a Default<Value> constraint was found in the pack.
        template <beavers::FixedString Value, typename... Rest>
        struct extract_default<constraints::Default<Value>, Rest...> {
            static constexpr bool found = true;  ///< Always true for this match case.
            /// The default value literal.
            static constexpr std::string_view value() noexcept {
                return Value.view();
            }
        };

        /// Finds the MaxLength<...> constraint in Cs..., if any; found is
        /// true and value is available only when one is present.
        template <typename... Cs>
        struct extract_max_length {
            static constexpr bool found = false;  ///< True only when the pack contains a MaxLength constraint.
        };

        template <typename First, typename... Rest>
        struct extract_max_length<First, Rest...> : extract_max_length<Rest...> {};

        /// Match case: a MaxLength<N> constraint was found in the pack.
        template <std::size_t N, typename... Rest>
        struct extract_max_length<constraints::MaxLength<N>, Rest...> {
            static constexpr bool found        = true;  ///< Always true for this match case.
            static constexpr std::size_t value = N;     ///< The max-length limit.
        };

        /// Finds the DbType<...> constraint in Cs..., if any; found is true
        /// and value() is available only when one is present.
        template <typename... Cs>
        struct extract_db_type {
            static constexpr bool found = false;  ///< True only when the pack contains a DbType constraint.
        };

        template <typename First, typename... Rest>
        struct extract_db_type<First, Rest...> : extract_db_type<Rest...> {};

        /// Match case: a DbType<SqlType> constraint was found in the pack.
        template <beavers::FixedString SqlType, typename... Rest>
        struct extract_db_type<constraints::DbType<SqlType>, Rest...> {
            static constexpr bool found = true;  ///< Always true for this match case.
            /// The literal SQL type override.
            static constexpr std::string_view value() noexcept {
                return SqlType.view();
            }
        };
    }  // namespace detail
}  // namespace menagerie::db
