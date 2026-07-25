#pragma once

#include <menagerie/beavers>
#include <string_view>
#include <type_traits>

#include "db_constraints.hpp"

namespace menagerie::db {

    /**
     * @brief Compile-time field schema: CppType, a FixedString column name,
     *        and a Constraints... pack of tag types from db_constraints.hpp.
     *
     * Every property is evaluated at compile time from the Constraints pack:
     * is_primary_key/is_nullable/is_unique/is_indexed detect the simple tag
     * constraints; is_foreign_key/has_default/has_max_length/has_db_type
     * detect the parameterized ones, and the matching foreign_table()/
     * foreign_column()/default_value()/max_length()/db_type() accessors are
     * only well-formed when the corresponding constraint is present.
     */
    template <typename CppType, beavers::FixedString Name, typename... Constraints>
    class StaticFieldSchema {
    public:
        using value_type = CppType;  ///< The C++ type this field maps to.

        static constexpr auto name_literal = Name;  ///< The field name as a compile-time FixedString.

        /// The field name as a std::string_view.
        static constexpr std::string_view name() noexcept {
            return std::string_view{Name};
        }

        // Simple tag constraints
        static constexpr bool is_primary_key = (std::is_same_v<Constraints, constraints::PrimaryKey> || ...);  ///< True when a PrimaryKey tag is present.
        static constexpr bool is_nullable    = !(std::is_same_v<Constraints, constraints::NotNull> || ...);    ///< True unless a NotNull tag is present.
        static constexpr bool is_unique      = (std::is_same_v<Constraints, constraints::Unique> || ...);      ///< True when a Unique tag is present.
        static constexpr bool is_indexed     = (std::is_same_v<Constraints, constraints::Indexed> || ...);     ///< True when an Indexed tag is present.

        // Parameterized constraints - detection
        static constexpr bool is_foreign_key = (detail::is_foreign_key<Constraints>::value || ...);  ///< True when a ForeignKey constraint is present.
        static constexpr bool has_default    = detail::extract_default<Constraints...>::found;       ///< True when a Default constraint is present.
        static constexpr bool has_max_length = detail::extract_max_length<Constraints...>::found;    ///< True when a MaxLength constraint is present.
        static constexpr bool has_db_type    = detail::extract_db_type<Constraints...>::found;        ///< True when a DbType constraint is present.

        // Parameterized constraints - value access
        /// The referenced table; only defined when a ForeignKey constraint is present.
        static constexpr std::string_view foreign_table() noexcept
            requires(detail::extract_foreign_key<Constraints...>::found)
        {
            return detail::extract_foreign_key<Constraints...>::table();
        }

        /// The referenced column; only defined when a ForeignKey constraint is present.
        static constexpr std::string_view foreign_column() noexcept
            requires(detail::extract_foreign_key<Constraints...>::found)
        {
            return detail::extract_foreign_key<Constraints...>::column();
        }

        /// The default value; only defined when a Default constraint is present.
        static constexpr std::string_view default_value() noexcept
            requires(detail::extract_default<Constraints...>::found)
        {
            return detail::extract_default<Constraints...>::value();
        }

        /// The maximum length; only defined when a MaxLength constraint is present.
        static constexpr std::size_t max_length() noexcept
            requires(detail::extract_max_length<Constraints...>::found)
        {
            return detail::extract_max_length<Constraints...>::value;
        }

        /// The literal SQL type; only defined when a DbType constraint is present.
        static constexpr std::string_view db_type() noexcept
            requires(detail::extract_db_type<Constraints...>::found)
        {
            return detail::extract_db_type<Constraints...>::value();
        }
    };

    /// Whether T is a compile-time field schema: exposes value_type, name(),
    /// is_primary_key, and is_nullable.
    template <typename T>
    concept IsStaticFieldSchema = requires {
        typename T::value_type;
        { T::name() } -> std::convertible_to<std::string_view>;
        { T::is_primary_key } -> std::convertible_to<bool>;
        { T::is_nullable } -> std::convertible_to<bool>;
    };

}  // namespace menagerie::db
