#pragma once

#include <cstdint>
#include <utility>

#include <libpq-fe.h>

#include "connection.hpp"

namespace menagerie::savanna::elephant {

    // -------- CleanupQuery --------

    /// The SQL a pool runs when a borrowed connection is returned, to reset session state
    /// left behind by the borrower before the connection goes back into circulation.
    enum class CleanupQuery : std::uint8_t {
        None,           ///< Run no cleanup SQL.
        ResetAll,       ///< RESET ALL: reverts all session-local configuration to defaults.
        DeallocateAll,  ///< DEALLOCATE ALL: drops all prepared statements.
        DiscardTemp,    ///< DISCARD TEMP: drops temporary tables.
        DiscardAll,     ///< DISCARD ALL: combines RESET ALL, DEALLOCATE ALL, DISCARD TEMP, and more.
    };

    /// Maps a CleanupQuery to its SQL text, or nullptr for CleanupQuery::None.
    [[nodiscard]] constexpr const char* to_sql(const CleanupQuery query) noexcept {
        switch (query) {
            case CleanupQuery::None:
                return nullptr;
            case CleanupQuery::ResetAll:
                return "RESET ALL";
            case CleanupQuery::DeallocateAll:
                return "DEALLOCATE ALL";
            case CleanupQuery::DiscardTemp:
                return "DISCARD TEMP";
            case CleanupQuery::DiscardAll:
                return "DISCARD ALL";
        }
        return nullptr;
    }

    // -------- ConnectionHolder base class --------

    /**
     * @brief Polymorphic base class for pool-managed PostgreSQL connection handles.
     *
     * Owns its connection as a Connection, so lifecycle state (READY / BROKEN /
     * DISCONNECTED) travels with the handle instead of being re-derived from libpq
     * at each call site. Capabilities receive std::weak_ptr<ConnectionHolder>. On
     * capability destruction the weak_ptr is locked to a temporary shared_ptr and
     * reset() is invoked, which runs cleanup SQL and returns the connection via a
     * pool-specific release path.
     */
    class ConnectionHolder {
    public:
        virtual ~ConnectionHolder() = default;

        ConnectionHolder(const ConnectionHolder&)            = delete;
        ConnectionHolder& operator=(const ConnectionHolder&) = delete;
        ConnectionHolder(ConnectionHolder&&)                 = delete;
        ConnectionHolder& operator=(ConnectionHolder&&)      = delete;

        /// Runs any pending cleanup SQL and returns the connection to its owning pool
        /// through that pool's own release path.
        virtual void reset() noexcept = 0;

        /// The raw libpq connection handle; valid only while this holder is alive.
        [[nodiscard]] PGconn* conn() const noexcept {
            return connection_.native_handle();
        }

        /// Selects the SQL that reset() runs before the connection goes back into
        /// circulation; overwrites any previously set cleanup.
        void set_cleanup(const CleanupQuery q) noexcept {
            cleanup_sql_ = to_sql(q);
        }

    protected:
        ConnectionHolder() noexcept = default;
        /// Takes ownership of an already-open connection.
        explicit ConnectionHolder(Connection conn) noexcept
            : connection_{std::move(conn)} {
        }

        /**
         * @brief Runs the pending cleanup SQL, falling back to @p fallback_sql when the
         *        borrower set none (nullptr or empty means no cleanup).
         * @return false if the connection is not READY or the cleanup statement failed;
         *         the connection is left BROKEN, and the pool should drop it.
         */
        [[nodiscard]] bool run_cleanup_sql(const char* fallback_sql = nullptr) noexcept {
            const char* sql = cleanup_sql_ != nullptr ? cleanup_sql_ : fallback_sql;
            cleanup_sql_    = nullptr;
            return connection_.run_cleanup(sql);
        }

        Connection connection_;              ///< The owned connection and its lifecycle FSM.
        const char* cleanup_sql_ = nullptr;  ///< Explicit per-borrow cleanup, or null for the pool default.
    };

}  // namespace menagerie::savanna::elephant
