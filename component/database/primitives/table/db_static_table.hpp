#pragma once

#include <array>
#include <menagerie/beavers>
#include <string_view>

#include <db_typed_column.hpp>
#include <providers.hpp>
#include <schema/db_static_field_schema.hpp>

namespace menagerie::db {

    /// The Ith type from a parameter pack.
    template <std::size_t I, typename... Ts>
    using pack_element_t = std::tuple_element_t<I, std::tuple<Ts...>>;

    /**
     * @brief Compile-time table parameterized on TableName and a
     *        FieldSchemas... pack of StaticFieldSchema<CppType, Name,
     *        Constraints...> types.
     *
     * Primary table type for schemas known at compile time. TableName is an
     * NTTP (FixedString), eliminating runtime storage and SSO dependency;
     * every type in FieldSchemas must satisfy IsStaticFieldSchema.
     */
    template <beavers::FixedString TableName, typename... FieldSchemas>
        requires(IsStaticFieldSchema<FieldSchemas> && ...)
    class StaticTable {
    public:
        static constexpr std::size_t N = sizeof...(FieldSchemas);  ///< Number of fields.

        /// Constructs the table, optionally bound to `provider`.
        [[nodiscard]] constexpr explicit StaticTable(const Providers provider = Providers::None) noexcept
            : provider_{provider} {
        }

        /// Compile-time name lookup: auto-deduces the value type and returns
        /// a TypedColumn. A name not present in FieldSchemas is a compile error.
        template <beavers::FixedString Name>
        [[nodiscard]] constexpr auto column() const {
            constexpr auto I = find_index<Name>();
            static_assert(I < N, "Column name not found in StaticTable");
            if constexpr (I < N) {
                using T = pack_element_t<I, FieldSchemas...>::value_type;
                return TypedColumn<T>{Name.view(), table_name()};
            }
        }

        /// Positional access: returns a TypedColumn with the name and type
        /// deduced from FieldSchemas[I].
        template <std::size_t I>
            requires(I < N)
        [[nodiscard]] constexpr auto column() const {
            using Schema = pack_element_t<I, FieldSchemas...>;
            using T      = Schema::value_type;
            return TypedColumn<T>{Schema::name(), table_name()};
        }

        /// This table's name.
        [[nodiscard]] static constexpr std::string_view table_name() noexcept {
            return TableName.view();
        }

        /// Number of fields in FieldSchemas.
        [[nodiscard]] constexpr std::size_t field_count() const noexcept {
            beavers::force_non_static(this);
            return N;
        }

        /// The provider this table is bound to, or Providers::None if unset.
        [[nodiscard]] constexpr Providers provider() const noexcept {
            return provider_;
        }

        /// Invokes `v.operator()<FieldSchema>()` for each field schema, in
        /// declaration order, entirely at compile time.
        template <typename Visitor>
        constexpr void for_each_field(Visitor&& v) const {
            beavers::force_non_static(this);
            (v.template operator()<FieldSchemas>(), ...);
        }

    private:
        template <beavers::FixedString Name>
        static constexpr std::size_t find_index() {
            constexpr std::array<std::string_view, N> names = {FieldSchemas::name()...};
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (names[i] == std::string_view{Name})
                    return i;
            }
            return N;  // sentinel - triggers static_assert at call site
        }

        Providers provider_ = Providers::None;
    };

}  // namespace menagerie::db
