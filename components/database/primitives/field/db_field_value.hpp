#pragma once
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace menagerie::db {

    /**
     * @brief A variant for storing field data, with owning and non-owning (view) forms.
     *
     * The view alternatives (std::string_view, std::span<const uint8_t>) are
     * non-owning: they must not outlive the buffer they point into.
     *
     * Adding a new type here means updating everywhere a FieldValue is bound,
     * decoded, or formatted for a provider: pg_sql_type_mapping.hpp (add a
     * SqlTypeMapping<T, PostgreSQL> specialization), pg_type_registry.hpp
     * (add an OID constant if needed), postgres_params.hpp/.cpp (add a
     * bind_one(T) overload), postgres_result_views.hpp/.cpp (add decode_*()
     * and as<T>() support), postgres_dialect.inl (add formatting in
     * format_value_impl()), and test_type_mapping.cpp (update the static
     * assertions).
     *
     * Types not yet supported that may be worth adding: a timestamp type
     * (TIMESTAMP/TIMESTAMPTZ), a date type, a time-of-day type, UUID, an
     * exact-precision decimal/numeric type, JSON(B), array types (e.g.
     * INTEGER[]), and interval types.
     */
    using FieldValue = std::variant<std::monostate,
                                    bool,
                                    char,
                                    std::int16_t,
                                    std::int32_t,
                                    std::int64_t,
                                    std::uint16_t,
                                    std::uint32_t,
                                    std::uint64_t,
                                    float,
                                    double,
                                    std::string,
                                    std::string_view,  ///< Zero-copy string data.
                                    std::vector<std::uint8_t>,
                                    std::span<const std::uint8_t>  ///< Zero-copy binary data.
                                    >;

    /// Whether T can construct a FieldValue (i.e. is one of its alternatives,
    /// or convertible to one).
    template <typename T>
    concept IsFieldValueType = std::constructible_from<FieldValue, T>;

}  // namespace menagerie::db
