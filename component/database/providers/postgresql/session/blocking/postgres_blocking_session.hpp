#pragma once

#include <chrono>
#include <menagerie/beavers>
#include <menagerie/crow>

#include <boost/asio/awaitable.hpp>
#include <capability_provider.hpp>
#include <postgres_errors.hpp>
#include <postgres_transaction.hpp>

#include "pool/blocking_pool.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief PostgreSQL session backed by a classic mutex + CV blocking pool.
     *
     * Unlike Session (lock-free, fail-fast), BlockingSession offers strict
     * FIFO waiter fairness, condition-variable wakeups, and three
     * acquisition modes per method: try (non-blocking, fails on exhaustion),
     * timed (bounded wait), and blocking (unbounded until success/shutdown).
     */
    class BlockingSession : beavers::Immutable {
    public:
        /// Constructs the underlying BlockingPool with the given configs.
        BlockingSession(ConnectionConfig connection_config, PoolConfig pool_config);
        /// Shuts down the pool if not already shut down.
        ~BlockingSession();

        // -------- Sync Executor --------

        /// Non-blocking sync acquire; fails immediately if the pool is exhausted or shut down.
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext> try_with_sync() noexcept;
        /// Bounded sync acquire; blocks the calling thread up to timeout.
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext>
        with_sync(std::chrono::steady_clock::duration timeout) noexcept;
        /// Unbounded sync acquire; blocks the calling thread until a slot frees or shutdown().
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext> with_sync() noexcept;

        // -------- Async Executor --------

        /// Non-blocking async acquire; fails immediately if the pool is exhausted or shut down.
        [[nodiscard]] beavers::Outcome<AsyncExecutor, ErrorContext>
        try_with_async(boost::asio::any_io_executor exec) noexcept;

        /**
         * @brief Coroutine-aware bounded acquire.
         *
         * Never blocks the calling thread; suspends the caller until a
         * slot is available or the timeout expires.
         */
        [[nodiscard]] boost::asio::awaitable<beavers::Outcome<AsyncExecutor, ErrorContext>>
        with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout);

        /**
         * @brief Coroutine-aware unbounded acquire.
         *
         * Suspends until a slot is available or the pool shuts down.
         */
        [[nodiscard]] boost::asio::awaitable<beavers::Outcome<AsyncExecutor, ErrorContext>>
        with_async(boost::asio::any_io_executor exec);

        // -------- Transactions --------

        /// Non-blocking: acquires a Transaction without sending BEGIN (caller must call begin()).
        [[nodiscard]] beavers::Outcome<Transaction, ErrorContext>
        try_begin_transaction(TransactionOptions opts = {}) noexcept;
        /// Bounded: acquires a Transaction (up to timeout) without sending BEGIN.
        [[nodiscard]] beavers::Outcome<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept;
        /// Unbounded: acquires a Transaction without sending BEGIN.
        [[nodiscard]] beavers::Outcome<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts = {}) noexcept;

        /// Non-blocking: acquires a Transaction and immediately sends BEGIN.
        [[nodiscard]] beavers::Outcome<AutoTransaction, ErrorContext>
        try_begin_auto_transaction(TransactionOptions opts = {}) noexcept;
        /// Bounded: acquires a Transaction (up to timeout) and immediately sends BEGIN.
        [[nodiscard]] beavers::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept;
        /// Unbounded: acquires a Transaction and immediately sends BEGIN.
        [[nodiscard]] beavers::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts = {}) noexcept;

        // -------- Lifecycle + Stats --------

        /// Drains the pool: wakes/aborts every waiter and clears all holders. Idempotent.
        void shutdown();

        /// Configured maximum number of connections.
        [[nodiscard]] std::size_t pool_capacity() const noexcept;
        /// Number of holders currently checked out.
        [[nodiscard]] std::size_t pool_active_count() const noexcept;
        /// Number of holders currently sitting in the free list.
        [[nodiscard]] std::size_t pool_free_count() const noexcept;
        /// Number of callers currently queued for a slot.
        [[nodiscard]] std::size_t pool_waiter_count() const noexcept;
        /// True once shutdown() has run.
        [[nodiscard]] bool is_shutdown() const noexcept;

    private:
        CROW_COMPONENT_PREFIX("BlockingSession");

        BlockingPool pool_;
    };

    // NOTE: BlockingSession does not satisfy the CapabilityProvider concept
    // because with_async(exec) now returns an awaitable instead of a
    // synchronous Outcome - it is the one session type whose async acquire
    // path suspends a coroutine rather than blocking the calling thread.
    // LockFreeSession still satisfies CapabilityProvider.

}  // namespace menagerie::db::postgres
