#include "postgres_session.hpp"

#include <menagerie/crow>
#include <utility>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <db_error_codes.hpp>

namespace menagerie::savanna::elephant {

    namespace {
        ClientErrorCode map_empty_weak_timed(const ConnectionPool& pool) noexcept {
            return pool.is_shutdown() ? ClientErrorCode::PoolShutdown : ClientErrorCode::WaitTimeout;
        }
    }  // namespace

    Session::Session(ConnectionConfig connection_config, PoolConfig pool_config)
        : pool_{std::move(connection_config), std::move(pool_config)} {
        COMPONENT_LOG_INF() << "Session created";
    }

    Session::~Session() {
        shutdown();
        COMPONENT_LOG_INF() << "Session destroyed";
    }

    // -------- Sync --------

    std::expected<SyncExecutor, ErrorContext> Session::try_with_sync() noexcept {
        auto holder = pool_.try_acquire();
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return SyncExecutor{std::move(holder)};
    }

    std::expected<SyncExecutor, ErrorContext> Session::with_sync(std::chrono::steady_clock::duration timeout) noexcept {
        auto holder = pool_.acquire(timeout);
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{map_empty_weak_timed(pool_)}});
        }
        return SyncExecutor{std::move(holder)};
    }

    std::expected<SyncExecutor, ErrorContext> Session::with_sync() noexcept {
        auto holder = pool_.acquire();
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }
        return SyncExecutor{std::move(holder)};
    }

    // -------- Async --------

    std::expected<AsyncExecutor, ErrorContext> Session::try_with_async(boost::asio::any_io_executor exec) noexcept {
        auto holder = pool_.try_acquire();
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return AsyncExecutor{std::move(holder), std::move(exec)};
    }

    boost::asio::awaitable<std::expected<AsyncExecutor, ErrorContext>>
    Session::with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout) {
        auto [ec, holder_sp] =
            co_await pool_.async_acquire(exec, timeout, boost::asio::as_tuple(boost::asio::use_awaitable));

        if (ec == boost::asio::error::operation_aborted) {
            co_return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }
        if (ec) {
            co_return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::WaitTimeout}});
        }
        if (!holder_sp) {
            co_return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }

        co_return AsyncExecutor{std::weak_ptr<ConnectionHolder>{holder_sp}, std::move(exec)};
    }

    boost::asio::awaitable<std::expected<AsyncExecutor, ErrorContext>>
    Session::with_async(boost::asio::any_io_executor exec) {
        auto [ec, holder_sp] = co_await pool_.async_acquire(exec, boost::asio::as_tuple(boost::asio::use_awaitable));

        if (ec || !holder_sp) {
            co_return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }

        co_return AsyncExecutor{std::weak_ptr<ConnectionHolder>{holder_sp}, std::move(exec)};
    }

    // -------- Transactions --------

    std::expected<Transaction, ErrorContext> Session::try_begin_transaction(TransactionOptions opts) noexcept {
        auto holder = pool_.try_acquire();
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{std::move(holder), opts};
    }

    std::expected<Transaction, ErrorContext>
    Session::begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept {
        auto holder = pool_.acquire(timeout);
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{map_empty_weak_timed(pool_)}});
        }
        return Transaction{std::move(holder), opts};
    }

    std::expected<Transaction, ErrorContext> Session::begin_transaction(TransactionOptions opts) noexcept {
        auto holder = pool_.acquire();
        if (holder.expired()) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }
        return Transaction{std::move(holder), opts};
    }

    std::expected<AutoTransaction, ErrorContext> Session::try_begin_auto_transaction(TransactionOptions opts) noexcept {
        auto tx_outcome = try_begin_transaction(opts);
        if (!tx_outcome.has_value()) {
            return std::unexpected(std::move(tx_outcome).error());
        }
        Transaction tx = std::move(tx_outcome).value();
        if (auto b = tx.begin(); !b.has_value()) {
            return std::unexpected(std::move(b).error());
        }
        return AutoTransaction{std::move(tx)};
    }

    std::expected<AutoTransaction, ErrorContext>
    Session::begin_auto_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept {
        auto tx_outcome = begin_transaction(opts, timeout);
        if (!tx_outcome.has_value()) {
            return std::unexpected(std::move(tx_outcome).error());
        }
        Transaction tx = std::move(tx_outcome).value();
        if (auto b = tx.begin(); !b.has_value()) {
            return std::unexpected(std::move(b).error());
        }
        return AutoTransaction{std::move(tx)};
    }

    std::expected<AutoTransaction, ErrorContext> Session::begin_auto_transaction(TransactionOptions opts) noexcept {
        auto tx_outcome = begin_transaction(opts);
        if (!tx_outcome.has_value()) {
            return std::unexpected(std::move(tx_outcome).error());
        }
        Transaction tx = std::move(tx_outcome).value();
        if (auto b = tx.begin(); !b.has_value()) {
            return std::unexpected(std::move(b).error());
        }
        return AutoTransaction{std::move(tx)};
    }

    // -------- Lifecycle + Stats --------

    void Session::shutdown() {
        pool_.shutdown();
    }

    std::size_t Session::pool_capacity() const noexcept {
        return pool_.capacity();
    }
    std::size_t Session::pool_active_count() const noexcept {
        return pool_.active_count();
    }
    std::size_t Session::pool_free_count() const noexcept {
        return pool_.free_count();
    }
    std::size_t Session::pool_waiter_count() const noexcept {
        return pool_.waiter_count();
    }
    bool Session::is_shutdown() const noexcept {
        return pool_.is_shutdown();
    }

}  // namespace menagerie::savanna::elephant
