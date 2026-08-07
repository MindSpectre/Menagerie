#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <menagerie/beavers>
#include <vector>

#include <connection_holder.hpp>
#include <pool_config.hpp>
#include <postgres_connection_config.hpp>
#include <sequence.hpp>

#include "slot_holder.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief Lock-free connection pool using a disruptor-inspired ring buffer
     *
     * Holds a fixed-size vector of std::shared_ptr<SlotHolder>. Acquire uses
     * a CAS scan from a hint cursor for O(1) amortized borrowing and returns
     * a std::weak_ptr<ConnectionHolder> that capabilities keep as their
     * borrow handle. Reset is the capability's responsibility via the
     * SlotHolder's virtual reset().
     *
     * INACTIVE slots are lazily CAS'd straight to USED (handed out immediately, not staged
     * through FREE) on the first acquire that finds no FREE slot. The pool does not grow
     * dynamically.
     */
    class ConnectionPool : beavers::Immutable {
    public:
        /**
         * @brief Construct the pool: allocate capacity() slots and pre-warm min_connections() of them
         *
         * Slots beyond min_connections() start INACTIVE and connect lazily on the
         * first acquire that needs them. A slot whose pre-warm connection attempt
         * fails is left DEAD for the janitor to retry.
         *
         * @throw std::invalid_argument if min_connections() > 0 and connection_config
         *        fails validation while building the connection string.
         */
        ConnectionPool(ConnectionConfig connection_config, PoolConfig pool_config);

        /// Shuts the pool down; slot destruction closes each connection via ~SlotHolder.
        ~ConnectionPool();

        /**
         * @brief Acquire a connection holder from the pool (lock-free)
         * @return A weak_ptr to the borrowed slot; expired() is true if no FREE or
         *         INACTIVE slot was available, or the pool has been shut down.
         * @warning Promoting an INACTIVE slot calls create_connection() unguarded, which can
         *          throw std::invalid_argument if connection_config() fails validation. Since
         *          this function is noexcept, that throw terminates the process; it is reachable
         *          when min_connections() == 0 (so the constructor never validated the config
         *          during pre-warm) and connection_config() is invalid.
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder> acquire_slot() noexcept;

        /**
         * @brief Acquire a connection holder, retrying against a bounded sleep-retry budget
         * @param timeout Total time budget, split into 10 sleep-retry attempts between
         *                acquire_slot() calls. Non-positive behaves like a single
         *                acquire_slot() call.
         * @return Same as acquire_slot(); expired() is true if the full budget elapses
         *         without finding a slot, or the pool is shut down mid-retry.
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder>
        acquire_slot_wait(std::chrono::steady_clock::duration timeout) noexcept;

        /**
         * @brief Graceful shutdown: mark INACTIVE all non-borrowed slots and prevent further acquires
         *
         * Borrowed slots are released normally by their capability's destructor.
         * Idempotent; a second call is a no-op.
         */
        void shutdown();

        // -------- Stats --------

        /// Total slot count (fixed at construction; the ring buffer capacity).
        [[nodiscard]] std::size_t capacity() const noexcept;
        /// Number of slots currently USED or WAITING (borrowed).
        [[nodiscard]] std::size_t active_count() const noexcept;
        /// Number of slots currently FREE (connected and available to acquire).
        [[nodiscard]] std::size_t free_count() const noexcept;
        /// True once shutdown() has been called.
        [[nodiscard]] bool is_shutdown() const noexcept;

        // -------- Internal Access (for Janitor) --------

        /// Direct access to the slot vector; used by PoolJanitor's sweep.
        [[nodiscard]] std::vector<std::shared_ptr<SlotHolder>>& slots() noexcept;
        /// Connection parameters this pool uses to create new connections.
        [[nodiscard]] const ConnectionConfig& connection_config() const noexcept;
        /// Sizing/timeout configuration this pool was constructed with.
        [[nodiscard]] const PoolConfig& pool_config() const noexcept;

        /**
         * @brief Create a new PGconn using the stored connection config
         * @return A live PGconn on success, nullptr if the connection attempt failed
         *         (e.g. server unreachable or credentials rejected).
         * @throw std::invalid_argument if connection_config() fails validation while
         *        building the connection string.
         */
        [[nodiscard]] PGconn* create_connection() const;

    private:
        ConnectionConfig connection_config_;
        PoolConfig pool_config_;
        std::size_t mask_;

        std::vector<std::shared_ptr<SlotHolder>> slots_;
        multithread::Sequence hint_cursor_{0};

        std::atomic_bool shutdown_{false};
    };

}  // namespace menagerie::db::postgres
