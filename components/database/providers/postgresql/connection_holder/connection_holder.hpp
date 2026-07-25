#pragma once

#include <cstdint>

#include <libpq-fe.h>

namespace menagerie::db::postgres {

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

    // -------- SlotStatus --------

    /// Lifecycle state of a pool-managed connection slot.
    enum class SlotStatus : std::uint8_t {
        FREE,      ///< Idle and available to be acquired.
        USED,      ///< Currently borrowed by a capability.
        WAITING,   ///< Reserved for a slot mid-handoff to a waiter; treated like USED where checked.
        DEAD,      ///< Connection failed and is pending replacement by the pool janitor.
        INACTIVE,  ///< Never initialized; CAS'd directly to USED (handed out immediately) on
                   ///< first acquire that needs it, not to FREE.
    };

    /// Maps a SlotStatus to its debug name, or "UNKNOWN" if the value is out of range.
    [[nodiscard]] constexpr const char* to_string(const SlotStatus status) noexcept {
        switch (status) {
            case SlotStatus::FREE:
                return "FREE";
            case SlotStatus::USED:
                return "USED";
            case SlotStatus::WAITING:
                return "WAITING";
            case SlotStatus::DEAD:
                return "DEAD";
            case SlotStatus::INACTIVE:
                return "INACTIVE";
        }
        return "UNKNOWN";
    }

    // -------- ConnectionHolder base class --------

    /**
     * @brief Polymorphic base class for pool-managed PostgreSQL connection handles.
     *
     * Capabilities receive std::weak_ptr<ConnectionHolder>. On capability
     * destruction the weak_ptr is locked to a temporary shared_ptr and reset()
     * is invoked, which runs cleanup SQL and returns the connection via a
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
            return conn_;
        }

        /// Selects the SQL that reset() runs before the connection goes back into
        /// circulation; overwrites any previously set cleanup.
        void set_cleanup(const CleanupQuery q) noexcept {
            cleanup_sql_ = to_sql(q);
        }

    protected:
        ConnectionHolder() noexcept = default;
        /// Wraps an already-open connection handle `conn`.
        explicit ConnectionHolder(PGconn* conn) noexcept
            : conn_{conn} {
        }

        /// Runs the pending cleanup SQL (if any); returns false if the connection
        /// is not in a usable state or the cleanup query itself failed.
        [[nodiscard]] bool run_cleanup_sql() noexcept {
            if (!conn_ || PQstatus(conn_) != CONNECTION_OK)
                return false;
            if (cleanup_sql_ == nullptr || cleanup_sql_[0] == '\0')
                return true;
            PGresult* res = PQexec(conn_, cleanup_sql_);
            const bool ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
            PQclear(res);
            cleanup_sql_ = nullptr;
            return ok;
        }

        PGconn* conn_            = nullptr;  ///< The wrapped libpq connection handle.
        const char* cleanup_sql_ = nullptr;  ///< SQL to run on reset(), or null for none.
    };

}  // namespace menagerie::db::postgres
