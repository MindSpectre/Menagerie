#pragma once
#include <cstdint>
#include <string_view>
#include <utility>

namespace menagerie::db::postgres {

    /// libpq sslmode values, in increasing order of strictness.
    enum class SslMode : std::uint8_t {
        DISABLE     = 0,  ///< No SSL.
        ALLOW       = 1,  ///< Try non-SSL first, fall back to SSL.
        PREFER      = 2,  ///< Try SSL first, fall back to non-SSL.
        REQUIRE     = 3,  ///< SSL required; certificate not verified.
        VERIFY_CA   = 4,  ///< SSL required; certificate verified against a root CA.
        VERIFY_FULL = 5   ///< SSL required; certificate verified and hostname checked.
    };

    /// Converts an SslMode to its libpq sslmode string (e.g. "verify-full").
    template <typename StringT = std::string_view>
    [[nodiscard]] constexpr StringT ssl_mode_to_string_t(const SslMode mode) noexcept {
        switch (mode) {
            case SslMode::DISABLE:
                return "disable";
            case SslMode::ALLOW:
                return "allow";
            case SslMode::PREFER:
                return "prefer";
            case SslMode::REQUIRE:
                return "require";
            case SslMode::VERIFY_CA:
                return "verify-ca";
            case SslMode::VERIFY_FULL:
                return "verify-full";
        }
        std::unreachable();
    }

    /// Converts an integral value to an SslMode (0-5 as declared above); out-of-range values
    /// fall back to PREFER.
    template <typename T>
        requires std::is_integral_v<T>
    [[nodiscard]] constexpr SslMode ssl_mode_from_int(const T value) noexcept {
        switch (value) {
            case 0:
                return SslMode::DISABLE;
            case 1:
                return SslMode::ALLOW;
            case 2:
                return SslMode::PREFER;
            case 3:
                return SslMode::REQUIRE;
            case 4:
                return SslMode::VERIFY_CA;
            case 5:
                return SslMode::VERIFY_FULL;
            default:
                return SslMode::PREFER;
        }
        std::unreachable();
    }
}  // namespace menagerie::db::postgres
