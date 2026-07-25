#pragma once

#include <string>


/// Constant-time helpers to avoid timing side channels in security-sensitive
/// comparisons.
namespace menagerie::utilities::security {
    /// Byte-by-byte comparison that always walks the full length of a and b
    /// (no early exit on mismatch), avoiding timing leaks when comparing a
    /// computed hash against a stored one. Returns false immediately if the
    /// sizes differ, since a size mismatch is not secret-dependent.
    [[nodiscard]] inline bool constant_time_compare(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) {
            return false;
        }
        bool result = true;
        for (std::size_t i = 0; i < a.size(); ++i) {
            result &= a[i] == b[i];
        }
        return result;
    }
}  // namespace menagerie::utilities::security
