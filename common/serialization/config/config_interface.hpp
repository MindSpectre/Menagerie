#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "field.hpp"
#include "serial_concepts.hpp"

/// Field-descriptor serialization framework: declare a Field per member and
/// ConfigInterface provides serialize()/deserialize() without hand-written
/// per-field code.
namespace menagerie::serialization {

    /**
     * @brief CRTP base providing serialize<Format>() and static
     *        deserialize<Format>(input) generated from Derived::fields().
     *
     * Both entry points call Derived::validate() and walk Derived::fields()
     * field-by-field, unless Derived opts into custom_serialize/custom_deserialize
     * for a given Format.
     */
    template <typename Derived, typename... Formats>
    class ConfigInterface {
    public:
        virtual ~ConfigInterface() = default;

        /// Validates the derived config; called before every serialize() and
        /// after every deserialize().
        constexpr virtual void validate() const = 0;

        /// Serializes *this to Format, via custom_serialize if Derived provides
        /// one, otherwise by walking Derived::fields().
        template <typename Format>
            requires(std::same_as<Format, Formats> || ...)
        [[nodiscard]] Format serialize() const {
            static_cast<const Derived&>(*this).validate();
            if constexpr (HasCustomSerialize<Derived, Format>) {
                return static_cast<const Derived&>(*this).custom_serialize(std::type_identity<Format>{});
            } else {
                return auto_serialize<Format>();
            }
        }

        /// Builds a Derived from a Format, via custom_deserialize if Derived
        /// provides one, otherwise by walking Derived::fields().
        template <typename Format>
            requires(std::same_as<Format, Formats> || ...)
        static Derived deserialize(const Format& input) {
            if constexpr (HasCustomDeserialize<Derived, Format>) {
                return Derived::custom_deserialize(input);
            } else {
                return auto_deserialize<Format>(input);
            }
        }

    private:
        template <typename Format>
        [[nodiscard]] Format auto_serialize() const {
            Format out{};
            constexpr auto fs = Derived::fields();
            std::apply(
                [&](const auto&... f) { (serialize_one_field(out, static_cast<const Derived&>(*this), f), ...); }, fs);
            return out;
        }

        template <typename Format, typename F>
        static void serialize_one_field(Format& out, const Derived& d, F) {
            if constexpr (F::policy != FieldPolicy::Secret && F::policy != FieldPolicy::Excluded) {
                // FieldName (not a bare string) keeps menagerie::serialization
                // an associated namespace of this dependent call - the only
                // route by which two-phase lookup reaches the format overloads
                // (see field.hpp).
                write_field(out, FieldName{F::name.view()}, d.*F::ptr);
            }
        }

        template <typename Format>
        static Derived auto_deserialize(const Format& input) {
            auto builder      = []() { return typename Derived::Builder{}; }();
            constexpr auto fs = Derived::fields();
            std::apply([&](const auto&... f) { (deserialize_one_field(input, builder, f), ...); }, fs);
            return std::move(builder).finalize();
        }

        template <typename Format, typename BuilderT, typename F>
        static void deserialize_one_field(const Format& input, BuilderT& builder, F) {
            if constexpr (F::policy != FieldPolicy::Excluded && F::policy != FieldPolicy::ReadOnly) {
                // Read straight into the builder's member: no temporary, so a
                // field's type need not be default-constructible at namespace
                // scope (nested ConfigInterface types keep their private
                // framework constructors), and a missing key leaves the
                // member's declared default untouched.
                read_field(input, FieldName{F::name.view()}, builder.config_.*F::ptr);
            }
        }
    };

}  // namespace menagerie::serialization
