#pragma once

#include <cstdint>
#include <typeindex>

namespace menagerie::spider {

    /// Identifier distinguishing multiple registered instances of the same
    /// type in a Spider (default ID 0).
    using spider_id_t = std::uint32_t;

    namespace detail {

        /// Registry lookup key: a service's type plus its instance ID.
        struct Key {
            std::type_index type;  ///< The registered service's type.
            spider_id_t id;        ///< The instance ID distinguishing same-type registrations.

            /// True if type and id both match.
            bool operator==(const Key& o) const noexcept {
                return type == o.type && id == o.id;
            }
        };

        /// Hasher for Key, combining the type's hash_code with id.
        struct KeyHash {
            /// Combines k.type's hash_code with k.id via a splitmix-style mix.
            std::uint64_t operator()(const Key& k) const noexcept {
                // 64-bit splitmix-style combine
                std::uint64_t h  = k.type.hash_code();
                h               ^= static_cast<std::uint64_t>(k.id) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                return h;
            }
        };

    }  // namespace detail
}  // namespace menagerie::spider
