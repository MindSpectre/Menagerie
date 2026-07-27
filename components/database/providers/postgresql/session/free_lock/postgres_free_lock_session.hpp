#pragma once

#include <chrono>
#include <menagerie/beavers>
#include <menagerie/crow>

#include <capability_provider.hpp>
#include <postgres_errors.hpp>
#include <postgres_transaction.hpp>

#include "pool/connection_pool.hpp"
#include "pool/pool_janitor.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief PostgreSQL session owning a connection pool and background janitor
     *
     * The Session is the primary entry point for database operations.
     * Each with_sync() / with_async() call acquires a slot from the pool
     * and returns a slot-aware executor that resets the slot on destruction.
     *
     * Usage:
     *   auto session = Session(conn_config, pool_config);
     *
     *   // Synchronous
     *   auto result = session.with_sync().execute("SELECT 1");
     *
     *   // Asynchronous
     *   auto result = co_await session.with_async(io_exec).execute("SELECT 1");
     */
    class LockFreeSession : beavers::Immutable {
    public:
        /**
         * @brief Construct the session: build the connection pool and start its janitor
         * @throw std::invalid_argument if connection_config fails validation while the
         *        pool pre-warms its initial connections (needed whenever
         *        pool_config.min_connections() > 0, which every PoolConfig preset
         *        defaults to).
         * @throw std::system_error if the OS fails to create the background sweep thread.
         */
        LockFreeSession(ConnectionConfig connection_config, PoolConfig pool_config);

        /// Shuts the session down (see shutdown()) and destroys the pool and janitor.
        ~LockFreeSession();

        /**
         * @brief Get a synchronous executor with an acquired connection
         * @return SyncExecutor on success, ErrorContext on pool exhaustion
         */
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext> with_sync();

        /**
         * @brief Get a synchronous executor, waiting up to `timeout` for a free slot
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         *                Zero returns immediately (same as the no-arg overload).
         * @return SyncExecutor on success, ErrorContext{PoolExhausted} on full timeout
         */
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext>
        with_sync(std::chrono::steady_clock::duration timeout);

        /**
         * @brief Get an asynchronous executor with an acquired connection
         * @param exec Boost.Asio executor for async I/O
         * @return AsyncExecutor on success, ErrorContext on pool exhaustion
         */
        [[nodiscard]] beavers::Outcome<AsyncExecutor, ErrorContext> with_async(boost::asio::any_io_executor exec);

        /**
         * @brief Get an async executor, waiting up to `timeout` for a free slot
         * @param exec Boost.Asio executor for async I/O
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         *                Zero returns immediately (same as the single-arg overload).
         * @return AsyncExecutor on success, ErrorContext{PoolExhausted} on full timeout
         *
         * Note: the timeout wait blocks the calling thread. An awaitable variant
         * that suspends the coroutine may be added later.
         */
        [[nodiscard]] beavers::Outcome<AsyncExecutor, ErrorContext>
        with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout);

        /// Shuts the session down: stops the janitor thread and drains the pool.
        void shutdown();

        // -------- Transaction Factories --------

        /**
         * @brief Begin a transaction, borrowing a slot with a single non-blocking attempt
         * @param opts Transaction isolation / read-only / deferrable options
         * @return An IDLE Transaction the caller must begin() itself, or
         *         ErrorContext{PoolExhausted} if no slot was immediately available
         */
        [[nodiscard]] beavers::Outcome<Transaction, ErrorContext> begin_transaction(TransactionOptions opts = {});

        /**
         * @brief Begin a transaction, waiting up to `timeout` for a free slot
         * @param opts Transaction isolation / read-only / deferrable options
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         * @return Transaction on success, ErrorContext{PoolExhausted} on full timeout
         */
        [[nodiscard]] beavers::Outcome<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout);

        /**
         * @brief Begin an auto-transaction: borrow a slot and immediately issue BEGIN
         * @param opts Transaction isolation / read-only / deferrable options
         * @return An ACTIVE AutoTransaction on success, ErrorContext{PoolExhausted} if
         *         no slot was immediately available, or the error from a failed BEGIN
         */
        [[nodiscard]] beavers::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts = {});

        /**
         * @brief Begin an auto-transaction, waiting up to `timeout` for a free slot
         * @param opts Transaction isolation / read-only / deferrable options
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         * @return AutoTransaction on success, ErrorContext{PoolExhausted} on full timeout
         *         or on failure of the implicit BEGIN
         */
        [[nodiscard]] beavers::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout);

        // -------- Pool Stats --------

        /// Total number of slots in the underlying pool.
        [[nodiscard]] std::size_t pool_capacity() const noexcept;

        /// Number of slots currently borrowed (USED or WAITING).
        [[nodiscard]] std::size_t pool_active_count() const noexcept;

        /// Number of slots currently free and available to acquire.
        [[nodiscard]] std::size_t pool_free_count() const noexcept;

        /// True once shutdown() has been called.
        [[nodiscard]] bool is_shutdown() const noexcept;

    private:
        CROW_COMPONENT_PREFIX("LockFreeSession");

        ConnectionPool pool_;
        PoolJanitor janitor_;
    };

    static_assert(CapabilityProvider<LockFreeSession>);
}  // namespace menagerie::db::postgres
