#pragma once

#include <chrono>
#include <expected>
#include <menagerie/beaver>
#include <menagerie/crow>

#include <boost/asio/awaitable.hpp>
#include <capability_provider.hpp>
#include <postgres_errors.hpp>
#include <postgres_transaction.hpp>

#include "pool/connection_pool.hpp"

namespace menagerie::savanna::elephant {

    /**
     * @brief The PostgreSQL session: owns the provider's connection pool and lends
     *        out executors and transactions.
     *
     * One session, three acquisition policies, chosen per call site rather than by
     * selecting a pool implementation:
     *   - try_* :         non-blocking; fails with PoolExhausted when no connection is free
     *   - *(timeout) :    bounded wait; fails with WaitTimeout when the deadline passes
     *   - * (no timeout): unbounded wait, until a connection frees or shutdown()
     *
     * Sync verbs park the calling thread on a condition variable; with_async verbs
     * suspend the calling coroutine and never block an io_context thread. All
     * waiters - threads and coroutines - share one FIFO queue, so fairness holds
     * across both worlds and a freed connection is handed directly to the head
     * waiter instead of being re-raced.
     */
    class Session : beaver::Immutable {
    public:
        /// Constructs the underlying ConnectionPool with the given configs.
        Session(ConnectionConfig connection_config, PoolConfig pool_config);
        /// Shuts down the pool if not already shut down.
        ~Session();

        // -------- Sync Executor --------

        /// Non-blocking sync acquire; fails immediately if the pool is exhausted or shut down.
        [[nodiscard]] std::expected<SyncExecutor, ErrorContext> try_with_sync() noexcept;
        /// Bounded sync acquire; blocks the calling thread up to timeout.
        [[nodiscard]] std::expected<SyncExecutor, ErrorContext>
        with_sync(std::chrono::steady_clock::duration timeout) noexcept;
        /// Unbounded sync acquire; blocks the calling thread until a slot frees or shutdown().
        [[nodiscard]] std::expected<SyncExecutor, ErrorContext> with_sync() noexcept;

        // -------- Async Executor --------

        /// Non-blocking async acquire; fails immediately if the pool is exhausted or shut down.
        [[nodiscard]] std::expected<AsyncExecutor, ErrorContext>
        try_with_async(boost::asio::any_io_executor exec) noexcept;

        /**
         * @brief Coroutine-aware bounded acquire.
         *
         * Never blocks the calling thread; suspends the caller until a
         * slot is available or the timeout expires.
         */
        [[nodiscard]] boost::asio::awaitable<std::expected<AsyncExecutor, ErrorContext>>
        with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout);

        /**
         * @brief Coroutine-aware unbounded acquire.
         *
         * Suspends until a slot is available or the pool shuts down.
         */
        [[nodiscard]] boost::asio::awaitable<std::expected<AsyncExecutor, ErrorContext>>
        with_async(boost::asio::any_io_executor exec);

        // -------- Transactions --------

        /// Non-blocking: acquires a Transaction without sending BEGIN (caller must call begin()).
        [[nodiscard]] std::expected<Transaction, ErrorContext>
        try_begin_transaction(TransactionOptions opts = {}) noexcept;
        /// Bounded: acquires a Transaction (up to timeout) without sending BEGIN.
        [[nodiscard]] std::expected<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept;
        /// Unbounded: acquires a Transaction without sending BEGIN.
        [[nodiscard]] std::expected<Transaction, ErrorContext> begin_transaction(TransactionOptions opts = {}) noexcept;

        /// Non-blocking: acquires a Transaction and immediately sends BEGIN.
        [[nodiscard]] std::expected<AutoTransaction, ErrorContext>
        try_begin_auto_transaction(TransactionOptions opts = {}) noexcept;
        /// Bounded: acquires a Transaction (up to timeout) and immediately sends BEGIN.
        [[nodiscard]] std::expected<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept;
        /// Unbounded: acquires a Transaction and immediately sends BEGIN.
        [[nodiscard]] std::expected<AutoTransaction, ErrorContext>
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
        CROW_COMPONENT_PREFIX("Session");

        ConnectionPool pool_;
    };

    // Session models the sync half of CapabilityProvider. Its with_async(exec)
    // overloads return an awaitable that suspends the caller until a connection
    // frees - deliberately not the synchronous std::expected the full concept
    // requires; the immediate async borrow is spelled try_with_async(exec).
    // Transaction and AutoTransaction, whose connection is already borrowed,
    // model the full concept.
    static_assert(SyncCapabilityProvider<Session>);

}  // namespace menagerie::savanna::elephant
