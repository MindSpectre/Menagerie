#pragma once

#include <menagerie/beavers>
#include <string>

#include "detail/hash_interface.hpp"

namespace menagerie::crypto {

    /// @brief HMAC-SHA256 hasher.
    ///
    /// Privately inherits HashInterface (no public/private keyword before the
    /// base name defaults to private for a class), so hash_with_generated_salt
    /// is not reachable through this type from outside the class - only
    /// hash_function is re-exposed as public.
    class SHA256Hash final : HashInterface {
    public:
        SHA256Hash()           = default;
        ~SHA256Hash() override = default;

        /// @brief Creates a 256-bit HMAC hash using the given data and salt
        /// @param data The message to hash
        /// @param salt The salt to use in the hash
        /// @return A hexadecimal string representing the HMAC-SHA256 hash
        [[nodiscard]] std::string hash_function(std::string_view data, std::string_view salt) override;

        /// Returns the current HMAC key.
        [[nodiscard]] const std::string& key() const noexcept {
            return key_;
        }

        /// Sets the HMAC key used by subsequent hash_function calls.
        template <beavers::IsStringLike StringTp>
        constexpr void set_key(StringTp&& key) & noexcept {
            key_ = std::forward<StringTp>(key);
        }

    private:
        std::string key_;
    };
}  // namespace menagerie::crypto
