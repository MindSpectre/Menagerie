#pragma once

#include <cstdint>
#include <menagerie/beavers>
#include <string>
#include <string_view>

namespace menagerie::serialization {

    /// Controls which directions of ConfigInterface::serialize()/deserialize() a
    /// Field participates in.
    enum class FieldPolicy : std::uint8_t {
        Normal,    ///< Serialize and deserialize.
        Secret,    ///< Deserialize only (e.g. passwords: never echoed back out).
        Excluded,  ///< Skip both directions.
        ReadOnly,  ///< Serialize only.
    };

    /// Key wrapper for the read_field/write_field extension points. A domain
    /// type (rather than a bare string) so the machinery's unqualified,
    /// DEPENDENT calls always reach the format overloads: FieldName makes
    /// menagerie::serialization an ASSOCIATED NAMESPACE of every call, which
    /// two-phase lookup requires - the format headers (json.hpp) are normally
    /// included AFTER config_interface.hpp, so ordinary lookup at the template
    /// definition point sees none of them, and a plain string key carries no
    /// namespace ADL could find them through.
    struct FieldName {
        std::string_view value;  ///< The field's serialized key.

        /// Copies value into an owning std::string.
        [[nodiscard]] std::string str() const {
            return std::string{value};
        }
    };

    namespace detail {
        /// Recovers a member pointer's owner and value types.
        template <typename T>
        struct member_pointer_traits;

        /// Specialization for pointer-to-data-member types: recovers the owning
        /// class C and member value type V from `V C::*`.
        template <typename C, typename V>
        struct member_pointer_traits<V C::*> {
            using owner_type = C;  ///< The class the member pointer belongs to.
            using value_type = V;  ///< The pointed-to member's type.
        };
    }  // namespace detail

    /// @brief One member's serialization descriptor: Ptr is the member pointer,
    /// Name a beavers::FixedString JSON key, Policy the serialize/deserialize
    /// direction. owner_type / value_type are recovered from Ptr's type.
    template <auto Ptr, beavers::FixedString Name, FieldPolicy Policy = FieldPolicy::Normal>
    struct Field {
        static constexpr auto ptr    = Ptr;     ///< The described member's pointer.
        static constexpr auto name   = Name;    ///< The described member's serialized key.
        static constexpr auto policy = Policy;  ///< Which directions this field participates in.

        using owner_type = detail::member_pointer_traits<decltype(Ptr)>::owner_type;  ///< Ptr's owning class.
        using value_type = detail::member_pointer_traits<decltype(Ptr)>::value_type;  ///< Ptr's member type.
    };

}  // namespace menagerie::serialization
