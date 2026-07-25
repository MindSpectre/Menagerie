#pragma once
#include <string>
#include <string_view>

namespace menagerie::beavers {
    /**
     * @brief Transparent hash functor for unordered containers - enables
     *        heterogeneous lookup with `std::string`, `std::string_view`, and
     *        `const char*` without constructing a temporary `std::string`.
     */
    struct StringHash {
        using is_transparent = void;  ///< Enables heterogeneous lookup.

        /// Hashes `sv` via `std::hash<std::string_view>`.
        size_t operator()(const std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }

        /// Hashes `s` via `std::hash<std::string_view>`, without copying.
        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }

        /// Hashes the null-terminated string `s` via `std::hash<std::string_view>`.
        size_t operator()(const char* s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    /// Transparent equality functor - paired with `StringHash` for
    /// heterogeneous unordered-container lookup.
    struct StringEqual {
        using is_transparent = void;  ///< Enables heterogeneous lookup.

        /// True iff `lhs` and `rhs` hold the same characters.
        bool operator()(const std::string_view lhs, const std::string_view rhs) const noexcept {
            return lhs == rhs;
        }

        /// True iff `lhs` and `rhs` hold the same characters.
        bool operator()(const std::string& lhs, const std::string_view rhs) const noexcept {
            return lhs == rhs;
        }

        /// True iff `lhs` and `rhs` hold the same characters.
        bool operator()(const std::string_view lhs, const std::string& rhs) const noexcept {
            return lhs == rhs;
        }

        /// True iff `lhs` and `rhs` hold the same characters.
        bool operator()(const std::string& lhs, const std::string& rhs) const noexcept {
            return lhs == rhs;
        }
    };

}  // namespace menagerie::beavers
