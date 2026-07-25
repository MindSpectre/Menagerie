#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace menagerie::crypto {
    /// @brief Static helpers backed by OpenSSL's RAND_bytes for generating
    /// cryptographically secure random salts.
    class SaltGenerator {
    public:
        /// @brief Generate a cryptographically secure random salt of the specified size.
        /// @return The raw bytes in a std::vector<uint8_t>.
        /// @throw std::runtime_error if size is 0 or the underlying RAND_bytes call fails.
        [[nodiscard]] static std::vector<std::uint8_t> generate_bytes(std::size_t size);

        /// @brief Generate salt and return it as a hex string (e.g., "3afc18...").
        /// @throw std::runtime_error if size is 0 or the underlying RAND_bytes call fails.
        [[nodiscard]] static std::string generate_hex(std::size_t size);

        /// @brief Generate salt and return it as a Base64-encoded string.
        /// @throw std::runtime_error if size is 0 or the underlying RAND_bytes/BIO calls fail.
        [[nodiscard]] static std::string generate_base64(std::size_t size);

    private:
        /// Helper function: Encode data as Base64 using OpenSSL's BIO and EVP APIs.
        [[nodiscard]] static std::string encode_base64(const std::vector<std::uint8_t>& data);
    };
}  // namespace menagerie::crypto
