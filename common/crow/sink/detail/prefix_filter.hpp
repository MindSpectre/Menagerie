#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace menagerie::crow {

    /// Transparent hash so `unordered_set<std::string, PrefixHash, std::equal_to<>>`
    /// supports `contains(string_view)` without constructing a temporary std::string.
    struct PrefixHash {
        using is_transparent = void;  ///< Marker enabling heterogeneous lookup.

        /// Hashes a string_view (avoids constructing a temporary std::string).
        std::size_t operator()(const std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        /// Hashes a std::string via the same algorithm as the string_view overload.
        std::size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    /// Which prefixes PrefixFilter::accepts lets through.
    enum class PrefixFilterMode : std::uint8_t {
        AllowAll,   ///< Every prefix is accepted (the default).
        Allowlist,  ///< Only prefixes in the set are accepted.
        Denylist    ///< Every prefix except those in the set is accepted.
    };

    /**
     * @brief Per-sink prefix allow / deny filter.
     *
     * Mutually exclusive modes. Empty prefix is accepted by default in all modes;
     * call block_empty_prefix() to reject it.
     */
    class PrefixFilter {
    public:
        using Set = std::unordered_set<std::string, PrefixHash, std::equal_to<>>;  ///< Allowlist/denylist prefix set.

        constexpr PrefixFilter() = default;  // AllowAll

        /// Builds an allowlist filter: only prefixes in set are accepted.
        [[nodiscard]] constexpr static PrefixFilter allow(Set set) {
            return PrefixFilter{PrefixFilterMode::Allowlist, std::move(set)};
        }
        /// Builds a denylist filter: every prefix except those in set is accepted.
        [[nodiscard]] constexpr static PrefixFilter deny(Set set) {
            return PrefixFilter{PrefixFilterMode::Denylist, std::move(set)};
        }

        /// Makes the empty prefix rejected instead of always-accepted.
        constexpr PrefixFilter& block_empty_prefix() noexcept {
            block_empty_ = true;
            return *this;
        }

        /// True if prefix passes this filter's mode (and block_empty_prefix() setting).
        [[nodiscard]] constexpr bool accepts(const std::string_view prefix) const noexcept {
            if (prefix.empty()) {
                return !block_empty_;
            }
            switch (mode_) {
                case PrefixFilterMode::AllowAll:
                    return true;
                case PrefixFilterMode::Allowlist:
                    return set_.contains(prefix);
                case PrefixFilterMode::Denylist:
                    return !set_.contains(prefix);
            }
            std::unreachable();
        }

        /// This filter's mode.
        [[nodiscard]] constexpr PrefixFilterMode mode() const noexcept {
            return mode_;
        }
        /// Whether block_empty_prefix() has been called.
        [[nodiscard]] constexpr bool blocks_empty() const noexcept {
            return block_empty_;
        }

    private:
        constexpr PrefixFilter(const PrefixFilterMode m, Set s)
            : mode_{m},
              set_{std::move(s)} {
        }

        PrefixFilterMode mode_ = PrefixFilterMode::AllowAll;
        bool block_empty_      = false;
        Set set_;
    };

}  // namespace menagerie::crow
