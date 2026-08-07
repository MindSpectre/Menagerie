#pragma once

#include <menagerie/beavers>

#include <boost/unordered_map.hpp>
#include <providers.hpp>
#include <schema/db_dynamic_field_schema.hpp>

namespace menagerie::db {
    // Forward declarations
    class Column;

    /// Runtime table schema: fields are declared through add_field(...)
    /// calls and resolved by name through get_field_schema(...)/column(...),
    /// in contrast to StaticTable's compile-time template-parameter fields.
    class DynamicTable {
    public:
        /// Constructs an empty table named `table_name`, with no provider set.
        template <beavers::IsStringLike StringTp>
        constexpr explicit DynamicTable(StringTp&& table_name) noexcept
            : table_name_{std::forward<StringTp>(table_name)} {
        }

        /// Constructs an empty table named `table_name`, bound to `provider`
        /// (enables the provider-inferring add_field<T>(name) overload).
        template <beavers::IsStringLike StringTp>
        constexpr explicit DynamicTable(StringTp&& table_name, const Providers provider) noexcept
            : table_name_{std::forward<StringTp>(table_name)},
              provider_{provider} {
        }

        /// Adds a field named `name` with the literal SQL type `db_type`.
        template <typename T, beavers::IsStringLike StringTp1, beavers::IsStringLike StringTp2>
        DynamicTable& add_field(StringTp1&& name, StringTp2&& db_type);

        /// Adds a field named `name` with the literal SQL type `db_type`;
        /// `cpp_type` is accepted for future type-checking but not yet used.
        template <beavers::IsStringLike StringTp1, beavers::IsStringLike StringTp2>
        DynamicTable& add_field(StringTp1&& name, StringTp2&& db_type, std::type_index cpp_type);

        /**
         * @brief Adds a field named `name`, inferring its SQL type from T and
         *        this table's stored provider.
         * @throw std::logic_error if no provider was set; use the
         *        DynamicTable(name, provider) constructor, or one of the
         *        two/three-argument add_field overloads, instead.
         */
        template <typename T, beavers::IsStringLike StringTp>
        DynamicTable& add_field(StringTp&& name);

        /**
         * @brief Returns an untyped Column referencing the field named `field_name`.
         * @throw std::runtime_error if field_name is not defined in the schema.
         */
        [[nodiscard]] Column column(std::string_view field_name) const;

        // Runtime builders: each is a silent no-op if field_name is not defined.
        /// Marks `field_name` as the primary key (and implicitly non-nullable).
        DynamicTable& primary_key(std::string_view field_name);
        /// Sets `field_name`'s nullability.
        DynamicTable& nullable(std::string_view field_name, bool is_null = true);
        /// Marks `field_name` as a foreign key referencing `ref_table.ref_column`.
        DynamicTable& foreign_key(std::string_view field_name, std::string_view ref_table, std::string_view ref_column);
        /// Marks `field_name` as UNIQUE.
        DynamicTable& unique(std::string_view field_name);
        /// Marks `field_name` as indexed.
        DynamicTable& indexed(std::string_view field_name);

        /// Looks up the field named `name`; returns nullptr if not defined.
        [[nodiscard]] const DynamicFieldSchema* get_field_schema(std::string_view name) const;
        /// Looks up the field named `name`; returns nullptr if not defined.
        [[nodiscard]] DynamicFieldSchema* get_field_schema(std::string_view name);

        /// This table's name.
        [[nodiscard]] constexpr const std::string& table_name() const noexcept {
            return table_name_;
        }

        /// Number of fields currently defined.
        [[nodiscard]] constexpr std::size_t field_count() const noexcept {
            return fields_.size();
        }

        /// The defined fields, in declaration order.
        [[nodiscard]] constexpr const std::vector<DynamicFieldSchema>& fields() const noexcept {
            return fields_;
        }

        /// The provider this table is bound to, or Providers::None if unset.
        [[nodiscard]] constexpr Providers provider() const noexcept {
            return provider_;
        }

        /// The names of every field, in schema order.
        [[nodiscard]] std::vector<std::string> field_names() const;

        /// Constructs a DynamicTable named `name` and returns it as a shared_ptr.
        [[nodiscard]] static std::shared_ptr<DynamicTable> make_ptr(std::string name) {
            return std::make_shared<DynamicTable>(std::move(name));
        }

    private:
        std::string table_name_;
        std::vector<DynamicFieldSchema> fields_;
        boost::unordered_map<std::string, std::size_t, beavers::StringHash, beavers::StringEqual> field_index_;
        Providers provider_ = Providers::None;
    };
}  // namespace menagerie::db

#include "detail/db_dynamic_table.inl"
