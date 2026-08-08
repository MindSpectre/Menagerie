#pragma once

#include <string>
#include <vector>

#include "salt_generator.hpp"

/// OpenSSL-backed password/data hashing: an abstract hasher interface with
/// HMAC-SHA256 and PBKDF2-HMAC-SHA256 implementations, plus a cryptographically
/// secure salt generator.
namespace menagerie::crypto {
    /// @brief Abstract hasher interface: derived classes implement
    /// hash_function; hash_with_generated_salt is provided once here.
    struct HashInterface {
        /// A computed hash paired with the salt used to produce it.
        struct HashWithSalt {
            std::string hash;  ///< The computed hash.
            std::string salt;  ///< The salt used to produce hash.
        };

        virtual ~HashInterface() = default;

        HashInterface() = default;

        /// Hashes data with salt; the concrete algorithm is up to the override.
        virtual std::string hash_function(std::string_view data, std::string_view salt) = 0;

        /// @brief Draws a 16-byte Base64 salt from SaltGenerator and hashes
        /// password against it.
        ///
        /// Concrete hashers (SHA256Hash, PBKDF2Hash) privately inherit
        /// HashInterface, so this method is not reachable through them from
        /// outside the class; callers pair a hasher's own hash_function with
        /// SaltGenerator directly instead.
        [[nodiscard]] HashWithSalt hash_with_generated_salt(const std::string_view password) {
            std::string salt = SaltGenerator::generate_base64(16);  // 16-byte salt
            std::string hash = hash_function(password, salt);
            return {.hash = std::move(hash), .salt = std::move(salt)};
        }
    };

}  // namespace menagerie::crypto
