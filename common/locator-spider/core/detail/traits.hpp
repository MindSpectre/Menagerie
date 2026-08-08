#pragma once

#include <type_traits>

#include "policies.hpp"

namespace menagerie::spider {
    /// Detects a T::spider_policy member, as declared by the SPIDER_WEB(Policy)
    /// macro.
    template <class, class = void>
    struct has_spider_policy : std::false_type {};

    template <class T>
    struct has_spider_policy<T, std::void_t<decltype(T::spider_policy)>> : std::true_type {};

    /// Default-argument machinery for register_singleton/register_instance's lt
    /// parameter: returns T::spider_policy if SPIDER_WEB declared one for T,
    /// Resettable{} otherwise.
    template <class T>
    constexpr Lifetime get_spider_policy() {
        if constexpr (has_spider_policy<T>::value) {
            return T::spider_policy;
        } else {
            return Resettable{};
        }
    }
}  // namespace menagerie::spider
