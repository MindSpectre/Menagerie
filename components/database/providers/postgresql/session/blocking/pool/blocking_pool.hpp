#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <menagerie/beavers>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <connection_holder.hpp>
#include <pool_config.hpp>
#include <postgres_connection_config.hpp>

#include "async_waiter.hpp"
#include "queued_holder.hpp"
#include "waiter.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief Classic mutex + FIFO-waiter PostgreSQL connection pool.
     *
     * Owns its QueuedHolders as std::shared_ptr; capabilities receive
     * std::weak_ptr<ConnectionHolder>. On release, either hands the
     * connection directly to the head waiter (under the pool mutex) or
     * pushes it onto the free deque.
     *
     * Acquisition modes:
     *   - try_acquire(): non-blocking; fails fast on exhaustion
     *   - acquire(timeout): bounded CV wait; blocks the calling thread
     *   - acquire(): unbounded CV wait; blocks the calling thread
     *   - async_acquire(exec, timeout, token): asio-compatible async
     *     operation; suspends the caller's coroutine without ever
     *     blocking an io_context worker thread
     *   - async_acquire(exec, token): unbounded async variant
     *
     * Sync (Waiter) and async (AsyncWaiter) waiters share a single FIFO
     * deque so fairness is preserved regardless of which acquisition
     * mode each caller uses.
     */
    class BlockingPool : beavers::Immutable {
    public:
        /// Completion signature for async_acquire: (error_code, acquired holder or null on failure).
        using AsyncSignature = void(boost::system::error_code, std::shared_ptr<QueuedHolder>);

        /// Builds the pool and eagerly creates pool_config.min_connections() connections.
        BlockingPool(ConnectionConfig connection_config, PoolConfig pool_config);
        /// Shuts down the pool if not already shut down.
        ~BlockingPool();

        /**
         * @brief Non-blocking acquire: takes a free holder or creates one up to capacity.
         * @return An empty weak_ptr if the pool is shut down or exhausted (never queues a waiter).
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder> try_acquire() noexcept;

        /**
         * @brief Bounded blocking acquire: waits up to timeout for a slot to free.
         *
         * Blocks the calling thread on a condition variable if no slot is
         * immediately available. The waiter shares FIFO order with async
         * waiters registered via async_acquire.
         *
         * @return An empty weak_ptr if the timeout elapses or the pool shuts down while waiting.
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder> acquire(std::chrono::steady_clock::duration timeout) noexcept;

        /**
         * @brief Unbounded blocking acquire: waits until a slot frees or shutdown() drains the waiter.
         * @return An empty weak_ptr only if shutdown() ran while waiting.
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder> acquire() noexcept;

        /**
         * @brief Coroutine-friendly bounded acquire; never blocks the calling thread.
         *
         * Initiated via boost::asio::async_initiate, so the completion token
         * determines the return type (e.g. an awaitable for use_awaitable).
         * If timeout elapses first, the completion carries
         * boost::asio::error::timed_out.
         */
        template <typename CompletionToken>
        auto async_acquire(const boost::asio::any_io_executor& exec,
                           std::chrono::steady_clock::duration timeout,
                           CompletionToken&& token) {
            return boost::asio::async_initiate<CompletionToken, AsyncSignature>(
                [this, exec, timeout]<typename T>(T&& raw_handler) mutable {
                    this->initiate_async_acquire(exec, timeout, AsyncWaiter::Handler{std::forward<T>(raw_handler)});
                },
                token);
        }

        /**
         * @brief Coroutine-friendly unbounded acquire; never blocks the calling thread.
         *
         * Waits until a slot is available or shutdown() drains the waiter,
         * in which case the completion carries
         * boost::asio::error::operation_aborted.
         */
        template <typename CompletionToken>
        auto async_acquire(const boost::asio::any_io_executor& exec, CompletionToken&& token) {
            return boost::asio::async_initiate<CompletionToken, AsyncSignature>(
                [this, exec]<typename T>(T&& raw_handler) mutable {
                    this->initiate_async_acquire(exec,
                                                 std::chrono::steady_clock::duration::zero(),
                                                 AsyncWaiter::Handler{std::forward<T>(raw_handler)});
                },
                token);
        }

        /**
         * @brief Drains the pool: wakes every blocking waiter and posts an
         * operation_aborted completion to every async waiter, then clears
         * all holders. Idempotent - later calls are no-ops.
         */
        void shutdown();

        /// Configured maximum number of connections (PoolConfig::capacity()).
        [[nodiscard]] std::size_t capacity() const noexcept;
        /// Number of holders currently sitting in the free list.
        [[nodiscard]] std::size_t free_count() const noexcept;
        /// Number of holders currently checked out (created minus free).
        [[nodiscard]] std::size_t active_count() const noexcept;
        /// Number of callers (sync and async) currently queued for a slot.
        [[nodiscard]] std::size_t waiter_count() const noexcept;
        /// True once shutdown() has run.
        [[nodiscard]] bool is_shutdown() const noexcept;

        /// Connection parameters this pool was constructed with.
        [[nodiscard]] const ConnectionConfig& connection_config() const noexcept;
        /// Pool sizing/timeout parameters this pool was constructed with.
        [[nodiscard]] const PoolConfig& pool_config() const noexcept;

        /**
         * @brief Opens a new PostgreSQL connection using the pool's connection config.
         * @return A live PGconn* the caller now owns, or nullptr if
         *         PQconnectdb failed or the connection came up unhealthy.
         */
        [[nodiscard]] PGconn* create_connection() const;

    private:
        friend class QueuedHolder;
        void return_holder(QueuedHolder* raw) noexcept;
        void drop_dead(QueuedHolder* raw) noexcept;

        [[nodiscard]] std::shared_ptr<QueuedHolder> try_create_holder();

        /// Shared deque of sync and async waiters, in FIFO arrival order.
        using WaiterEntry = std::variant<Waiter*, std::shared_ptr<AsyncWaiter>>;

        /// Non-template initiation body. Zero timeout means "unbounded".
        void initiate_async_acquire(const boost::asio::any_io_executor& exec,
                                    std::chrono::steady_clock::duration timeout,
                                    AsyncWaiter::Handler handler);

        /// Called from an AsyncWaiter's timer callback.
        void on_async_waiter_timeout(const std::shared_ptr<AsyncWaiter>& waiter) noexcept;

        ConnectionConfig conn_cfg_;
        PoolConfig pool_cfg_;

        mutable std::mutex mtx_;
        std::vector<std::shared_ptr<QueuedHolder>> holders_;
        std::deque<QueuedHolder*> free_;
        std::deque<WaiterEntry> waiters_;
        std::size_t created_ = 0;

        std::atomic_bool shutdown_{false};
    };

}  // namespace menagerie::db::postgres
