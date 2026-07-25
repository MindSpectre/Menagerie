#pragma once
#include <optional>

#include "db_field_value.hpp"
#include "schema/db_dynamic_field_schema.hpp"

namespace menagerie::db {

    /**
     * @brief Type-safe database field with schema validation
     *
     * Represents a single database field value with compile-time type safety
     * and runtime schema validation. Provides safe value access with proper
     * error handling for type mismatches and null values.
     */
    class Field final {
    public:
        /**
         * @brief Constructs a field bound to `schema`.
         * @param schema Non-owning; must outlive this Field.
         * @throw std::invalid_argument if schema is null.
         */
        explicit Field(const DynamicFieldSchema* schema)
            : schema_{schema} {
            if (!schema_) {
                throw std::invalid_argument("Schema cannot be null");
            }
        }

        /// Assigns a new value, converting/constructing FieldValue from `value`.
        template <typename T>
        Field& set(T&& value);

        /**
         * @brief Returns the value as T.
         * @throw std::bad_variant_access if the field does not currently hold
         *        a T, including when it is null (holds std::monostate).
         */
        template <typename T>
        constexpr const T& get() const;

        /// Returns the value as T, or nullopt if the field does not
        /// currently hold a T (including when it is null).
        template <typename T>
        constexpr std::optional<T> try_get() const noexcept;

        /// Whether the field currently holds no value (the monostate alternative).
        [[nodiscard]] constexpr bool is_null() const noexcept {
            return std::holds_alternative<std::monostate>(value_);
        }

        /// Direct access to the underlying FieldValue variant.
        [[nodiscard]] constexpr const FieldValue& raw_value() const& noexcept {
            return value_;
        }

        /// Direct access to the underlying FieldValue variant, moved out.
        [[nodiscard]] constexpr FieldValue raw_value() && noexcept {
            return std::move(value_);
        }

        /// This field's schema metadata.
        [[nodiscard]] constexpr const DynamicFieldSchema& schema() const noexcept {
            return *schema_;
        }

        /// This field's name, from its schema.
        [[nodiscard]] constexpr const std::string& name() const noexcept {
            return schema_->name;
        }

    private:
        FieldValue value_;
        const DynamicFieldSchema* schema_;  ///< Non-owning, guaranteed to outlive this field.
    };
}  // namespace menagerie::db

#include "detail/db_field.inl"
