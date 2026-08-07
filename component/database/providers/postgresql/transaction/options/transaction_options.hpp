#pragma once

#include <cstdint>
#include <string>

namespace menagerie::db::postgres {

    /// PostgreSQL transaction isolation level, passed through to `BEGIN ISOLATION LEVEL ...`.
    enum class IsolationLevel : std::uint8_t {
        READ_COMMITTED,
        REPEATABLE_READ,
        SERIALIZABLE,
    };

    /// PostgreSQL transaction access mode, passed through to `BEGIN ... READ WRITE|READ ONLY`.
    enum class AccessMode : std::uint8_t {
        READ_WRITE,
        READ_ONLY,
    };

    /// Returns the SQL keyword for level, or "UNKNOWN" for an out-of-range value.
    [[nodiscard]] constexpr const char* to_string(const IsolationLevel level) noexcept {
        switch (level) {
            case IsolationLevel::READ_COMMITTED:
                return "READ COMMITTED";
            case IsolationLevel::REPEATABLE_READ:
                return "REPEATABLE READ";
            case IsolationLevel::SERIALIZABLE:
                return "SERIALIZABLE";
        }
        return "UNKNOWN";
    }

    /// Returns the SQL keyword for mode, or "UNKNOWN" for an out-of-range value.
    [[nodiscard]] constexpr const char* to_string(const AccessMode mode) noexcept {
        switch (mode) {
            case AccessMode::READ_WRITE:
                return "READ WRITE";
            case AccessMode::READ_ONLY:
                return "READ ONLY";
        }
        return "UNKNOWN";
    }

    /// Isolation level, access mode, and deferrability requested for a `BEGIN` statement.
    struct TransactionOptions {
        /// Isolation level to request; defaults to READ COMMITTED.
        IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
        /// Read/write access mode to request; defaults to READ WRITE.
        AccessMode access        = AccessMode::READ_WRITE;
        /// Requests DEFERRABLE; only valid combined with SERIALIZABLE + READ ONLY.
        bool deferrable          = false;

        /**
         * @brief Builds the `BEGIN ...` statement text for these options.
         * @throw std::invalid_argument if deferrable is set without SERIALIZABLE + READ ONLY.
         */
        [[nodiscard]] constexpr std::string to_begin_sql() const {
            if (deferrable && (isolation != IsolationLevel::SERIALIZABLE || access != AccessMode::READ_ONLY)) {
                throw std::invalid_argument("DEFERRABLE is only valid with SERIALIZABLE READ ONLY");
            }

            std::string sql  = "BEGIN";
            sql             += " ISOLATION LEVEL ";
            sql             += to_string(isolation);
            sql             += ' ';
            sql             += to_string(access);

            if (deferrable) {
                sql += " DEFERRABLE";
            }

            return sql;
        }
    };

}  // namespace menagerie::db::postgres
