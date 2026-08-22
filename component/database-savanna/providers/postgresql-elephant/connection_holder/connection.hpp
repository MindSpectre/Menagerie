#pragma once

#include <cstdint>
#include <utility>

#include <libpq-fe.h>

namespace menagerie::savanna::elephant {

    // -------- ConnectionState --------

    /// Lifecycle state of a Connection.
    enum class ConnectionState : std::uint8_t {
        DISCONNECTED,  ///< No live handle: never opened, open() failed, or close() ran.
        READY,         ///< Live connection, usable for queries.
        BROKEN,        ///< Live handle whose connection failed; close() is the only exit.
    };

    /// Maps a ConnectionState to its debug name, or "UNKNOWN" if the value is out of range.
    [[nodiscard]] constexpr const char* to_string(const ConnectionState state) noexcept {
        switch (state) {
            case ConnectionState::DISCONNECTED:
                return "DISCONNECTED";
            case ConnectionState::READY:
                return "READY";
            case ConnectionState::BROKEN:
                return "BROKEN";
        }
        return "UNKNOWN";
    }

    // -------- Connection --------

    /**
     * @brief Move-only owner of one libpq connection with an explicit lifecycle FSM.
     *
     * @verbatim
     *   DISCONNECTED --open() ok--> READY --verify()/run_cleanup() failure--> BROKEN
     *        ^                        |                                         |
     *        +-------- close() -------+----------------- close() --------------+
     * @endverbatim
     *
     * Every transition is guarded: an operation invoked in a state it is not
     * defined for reports failure instead of misbehaving, and libpq-level
     * connection failures demote READY to BROKEN rather than being re-checked
     * ad hoc at each call site. The pool creates connections via open(),
     * ConnectionHolder carries one through borrow/release cycles, and a BROKEN
     * connection is dropped by the pool instead of re-entering circulation.
     *
     * Not thread-safe; a Connection is only ever operated on by the borrower
     * that holds it (or the pool, between borrows).
     */
    class Connection {
    public:
        /// Constructs a DISCONNECTED connection owning nothing.
        constexpr Connection() noexcept = default;

        /**
         * @brief Opens a new connection: DISCONNECTED -> READY on success.
         * @param conninfo A libpq connection string (see ConnectionConfig::to_connection_string()).
         * @return A READY connection, or a DISCONNECTED one if PQconnectdb failed or
         *         the connection came up unhealthy (the dead handle is closed, not kept).
         */
        [[nodiscard]] static Connection open(const char* conninfo) noexcept {
            PGconn* raw = PQconnectdb(conninfo);
            if (!raw) {
                return {};
            }
            if (PQstatus(raw) != CONNECTION_OK) {
                PQfinish(raw);
                return {};
            }
            return Connection{raw};
        }

        /// Closes the underlying connection, if any.
        ~Connection() {
            close();
        }

        Connection(Connection&& other) noexcept
            : conn_{std::exchange(other.conn_, nullptr)},
              state_{std::exchange(other.state_, ConnectionState::DISCONNECTED)} {
        }

        Connection& operator=(Connection&& other) noexcept {
            if (this != &other) {
                close();
                conn_  = std::exchange(other.conn_, nullptr);
                state_ = std::exchange(other.state_, ConnectionState::DISCONNECTED);
            }
            return *this;
        }

        Connection(const Connection&)            = delete;
        Connection& operator=(const Connection&) = delete;

        /// Current FSM state.
        [[nodiscard]] constexpr ConnectionState state() const noexcept {
            return state_;
        }

        /// True while state() is READY.
        [[nodiscard]] constexpr bool ready() const noexcept {
            return state_ == ConnectionState::READY;
        }

        /// The raw libpq handle; non-null in READY and BROKEN, null in DISCONNECTED.
        [[nodiscard]] constexpr PGconn* native_handle() const noexcept {
            return conn_;
        }

        /**
         * @brief Re-checks the connection with libpq: READY -> READY | BROKEN.
         * @return true iff the connection is still READY afterwards; false from any
         *         other state (guarded, no transition).
         */
        [[nodiscard]] bool verify() noexcept {
            if (state_ != ConnectionState::READY) {
                return false;
            }
            if (PQstatus(conn_) != CONNECTION_OK) {
                state_ = ConnectionState::BROKEN;
                return false;
            }
            return true;
        }

        /**
         * @brief Runs session-reset SQL on a READY connection: READY -> READY | BROKEN.
         * @param sql The statement to run; nullptr or empty is a no-op success.
         * @return true iff the connection is READY and the statement (if any) succeeded;
         *         a failed statement or a dead connection leaves the state BROKEN.
         */
        [[nodiscard]] bool run_cleanup(const char* sql) noexcept {
            if (!verify()) {
                return false;
            }
            if (sql == nullptr || sql[0] == '\0') {
                return true;
            }
            PGresult* res = PQexec(conn_, sql);
            const bool ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
            PQclear(res);
            if (!ok) {
                state_ = ConnectionState::BROKEN;
            }
            return ok;
        }

        /// Releases the handle (PQfinish): any state -> DISCONNECTED. Idempotent.
        void close() noexcept {
            if (conn_) {
                PQfinish(conn_);
                conn_ = nullptr;
            }
            state_ = ConnectionState::DISCONNECTED;
        }

    private:
        /// Adopts an already-open, healthy handle as READY.
        explicit Connection(PGconn* conn) noexcept
            : conn_{conn},
              state_{ConnectionState::READY} {
        }

        PGconn* conn_          = nullptr;
        ConnectionState state_ = ConnectionState::DISCONNECTED;
    };

}  // namespace menagerie::savanna::elephant
