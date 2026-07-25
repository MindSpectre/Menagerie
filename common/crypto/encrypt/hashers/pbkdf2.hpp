#pragma once

#include <string>

#include "detail/hash_interface.hpp"

namespace menagerie::crypto {

    /// @brief PBKDF2-HMAC-SHA256 hasher (100,000 iterations, 256-bit digest).
    ///
    /// Privately inherits HashInterface (no public/private keyword before the
    /// base name defaults to private for a class), so hash_with_generated_salt
    /// is not reachable through this type from outside the class - only
    /// hash_function is re-exposed as public.
    class PBKDF2Hash final : HashInterface {
    public:
        PBKDF2Hash()           = default;
        ~PBKDF2Hash() override = default;

        /// @brief Creates a PBKDF2 hash using the given password and salt
        /// @param password The password to hash
        /// @param salt The salt to use in the hash
        /// @return A hexadecimal string representing the PBKDF2 hash
        /// @throw std::runtime_error if the underlying PKCS5_PBKDF2_HMAC call fails
        [[nodiscard]] std::string hash_function(std::string_view password, std::string_view salt) override;
    };
}  // namespace menagerie::crypto
