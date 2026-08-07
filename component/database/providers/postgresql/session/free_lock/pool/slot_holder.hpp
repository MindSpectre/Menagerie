#pragma once

#include <atomic>
#include <memory>
#include <new>

#include <connection_holder.hpp>

namespace menagerie::db::postgres {

    /**
     * @brief Ring-buffer concrete ConnectionHolder for ConnectionPool
     *
     * Stores the atomic slot status consumed by the pool's CAS acquire path.
     * Cache-line aligned to avoid false sharing. Pool owns each SlotHolder
     * through a std::shared_ptr; capabilities receive std::weak_ptr<ConnectionHolder>
     * derived from the pool's strong ref.
     */
    class alignas(std::hardware_destructive_interference_size) SlotHolder final
        : public ConnectionHolder,
          public std::enable_shared_from_this<SlotHolder> {
    public:
        /// Slot lifecycle state; read and CAS'd directly by the pool's acquire path.
        std::atomic<SlotStatus> status = SlotStatus::INACTIVE;

        /// Constructs an empty, INACTIVE slot with no connection.
        SlotHolder() noexcept = default;
        /// Constructs an INACTIVE slot wrapping an already-open connection.
        explicit SlotHolder(PGconn* conn) noexcept
            : ConnectionHolder{conn} {
        }

        /// Closes the underlying connection, if any.
        ~SlotHolder() override {
            if (conn_) {
                PQfinish(conn_);
                conn_ = nullptr;
            }
        }

        /// Runs any pending cleanup SQL and returns the slot to FREE, or marks it DEAD if cleanup fails.
        void reset() noexcept override {
            if (!run_cleanup_sql()) {
                status.store(SlotStatus::DEAD, std::memory_order_release);
                return;
            }
            status.store(SlotStatus::FREE, std::memory_order_release);
        }

        /// Replace the underlying PGconn (used by the janitor during replacement).
        void replace_conn(PGconn* new_conn) noexcept {
            if (conn_) {
                PQfinish(conn_);
            }
            conn_ = new_conn;
        }
    };

}  // namespace menagerie::db::postgres
