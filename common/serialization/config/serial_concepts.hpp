#pragma once

#include <concepts>
#include <type_traits>

namespace menagerie::serialization {

    /// Detects a T::fields() static member; a nested ConfigInterface-derived
    /// member serializes as a nested object instead of a scalar when this holds.
    template <typename T>
    concept HasFields = requires { T::fields(); };

    /// Detects T::custom_serialize(std::type_identity<Format>), an escape hatch
    /// that skips the generic field walk for that Format.
    template <typename T, typename Format>
    concept HasCustomSerialize = requires(const T& t) {
        { t.custom_serialize(std::type_identity<Format>{}) } -> std::same_as<Format>;
    };

    /// Detects a static T::custom_deserialize(const Format&), an escape hatch
    /// that skips the generic field walk for that Format.
    template <typename T, typename Format>
    concept HasCustomDeserialize = requires(const Format& f) {
        { T::custom_deserialize(f) } -> std::same_as<T>;
    };

}  // namespace menagerie::serialization
