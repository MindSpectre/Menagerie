#pragma once
#include <cstdint>

namespace menagerie::db::postgres {
    /// PostgreSQL wire-format codes, as used by libpq's PQfformat()/PQexecParams()
    /// format arguments.
    struct FormatRegistry {
        constexpr static std::uint32_t text   = 0;  ///< Human-readable text wire format.
        constexpr static std::uint32_t binary = 1;  ///< Raw binary wire format.
    };
}  // namespace menagerie::db::postgres
