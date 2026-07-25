#pragma once

#include <menagerie/beavers>

#include <boost/unordered/unordered_map.hpp>
#include <db_field.hpp>

namespace menagerie::db {

    class DynamicTable;

    /**
     * @brief Database record representing a single row with type-safe field access
     *
     * Record provides a container for database row data with efficient field access
     * patterns. Uses vector<Field> for storage with unordered_map for O(1) name-to-index
     * mapping. The schema is immutably shared via shared_ptr<const DynamicTable> ensuring
     * consistent field definitions across record instances.
     *
     * Thread-unsafe: concurrent access requires external synchronization,
     * though multiple threads may safely read from different Record
     * instances sharing the same schema simultaneously.
     *
     * Field access by name involves a hash lookup (O(1) average, O(n) worst
     * case); field access by index is O(1) with bounds checking in debug
     * builds. Iterator invalidation follows std::vector semantics: stable
     * unless the underlying field vector is modified.
     */
    class Record final {
    public:
        /**
         * @brief Constructs a Record with the specified table schema; every
         *        field is initialized to its schema default value.
         * @throw std::invalid_argument if schema is null.
         */
        explicit Record(std::shared_ptr<const DynamicTable> schema);

        // Copy and move constructors are automatically generated and safe

        /**
         * @brief Accesses the field named `field_name`.
         * @throw std::out_of_range if field_name is not defined in the schema.
         */
        Field& operator[](std::string_view field_name);

        /**
         * @brief Accesses the field named `field_name` (const).
         * @throw std::out_of_range if field_name is not defined in the schema.
         */
        const Field& operator[](std::string_view field_name) const;

        /**
         * @brief Accesses the field at `index`.
         * @throw std::out_of_range if index >= field_count().
         */
        Field& at(std::size_t index);

        /**
         * @brief Accesses the field at `index` (const).
         * @throw std::out_of_range if index >= field_count().
         */
        [[nodiscard]] const Field& at(std::size_t index) const;

        /// Non-throwing lookup by name; returns nullptr if not found.
        Field* get_field(std::string_view name);

        /// Non-throwing lookup by name (const); returns nullptr if not found.
        [[nodiscard]] const Field* get_field(std::string_view name) const;

        /// The table schema this record was constructed with.
        [[nodiscard]] constexpr const DynamicTable& schema() const {
            return *schema_;
        }
        /// The shared_ptr backing schema(), for sharing schema ownership
        /// across multiple Records.
        [[nodiscard]] constexpr std::shared_ptr<const DynamicTable> table_ptr() const {
            return schema_;
        }

        /// The number of fields, as defined by the schema.
        [[nodiscard]] constexpr std::size_t field_count() const {
            return fields_.size();
        }

        /// Iterator to the first field.
        [[nodiscard]] constexpr std::vector<Field>::iterator begin() noexcept {
            return fields_.begin();
        }

        /// Iterator past the last field.
        [[nodiscard]] constexpr std::vector<Field>::iterator end() noexcept {
            return fields_.end();
        }

        /// Const iterator to the first field.
        [[nodiscard]] constexpr std::vector<Field>::const_iterator begin() const {
            return fields_.begin();
        }

        /// Const iterator past the last field.
        [[nodiscard]] constexpr std::vector<Field>::const_iterator end() const {
            return fields_.end();
        }

    private:
        std::shared_ptr<const DynamicTable> schema_;
        std::vector<Field> fields_;
        boost::unordered_map<std::string, std::size_t, beavers::StringHash, beavers::StringEqual> field_index_;
    };
}  // namespace menagerie::db
