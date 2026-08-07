#include "postgres_free_lock_session.hpp"

#include <menagerie/crow>

#include <db_error_codes.hpp>

namespace menagerie::db::postgres {

    LockFreeSession::LockFreeSession(ConnectionConfig connection_config, PoolConfig pool_config)
        : pool_{std::move(connection_config), std::move(pool_config)},
          janitor_{pool_} {
        COMPONENT_LOG_INF() << "Session created";
    }

    LockFreeSession::~LockFreeSession() {
        shutdown();
        COMPONENT_LOG_INF() << "Session destroyed";
    }

    beavers::Outcome<SyncExecutor, ErrorContext> LockFreeSession::with_sync() {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot();
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return SyncExecutor{std::move(holder)};
    }

    beavers::Outcome<SyncExecutor, ErrorContext>
    LockFreeSession::with_sync(std::chrono::steady_clock::duration timeout) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot_wait(timeout);
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return SyncExecutor{std::move(holder)};
    }

    beavers::Outcome<AsyncExecutor, ErrorContext> LockFreeSession::with_async(boost::asio::any_io_executor exec) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot();
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return AsyncExecutor{std::move(holder), std::move(exec)};
    }

    beavers::Outcome<AsyncExecutor, ErrorContext>
    LockFreeSession::with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot_wait(timeout);
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return AsyncExecutor{std::move(holder), std::move(exec)};
    }

    void LockFreeSession::shutdown() {
        COMPONENT_LOG_ENTER_FUNCTION();
        janitor_.stop();
        pool_.shutdown();
        COMPONENT_LOG_LEAVE_FUNCTION();
    }

    std::size_t LockFreeSession::pool_capacity() const noexcept {
        return pool_.capacity();
    }

    std::size_t LockFreeSession::pool_active_count() const noexcept {
        return pool_.active_count();
    }

    std::size_t LockFreeSession::pool_free_count() const noexcept {
        return pool_.free_count();
    }

    bool LockFreeSession::is_shutdown() const noexcept {
        return pool_.is_shutdown();
    }

    beavers::Outcome<Transaction, ErrorContext> LockFreeSession::begin_transaction(const TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot();
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{std::move(holder), opts};
    }

    beavers::Outcome<Transaction, ErrorContext>
    LockFreeSession::begin_transaction(const TransactionOptions opts, std::chrono::steady_clock::duration timeout) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot_wait(timeout);
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{std::move(holder), opts};
    }

    beavers::Outcome<AutoTransaction, ErrorContext>
    LockFreeSession::begin_auto_transaction(const TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot();
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }

        Transaction tx{std::move(holder), opts};
        if (auto begin_result = tx.begin(); !begin_result.is_success()) {
            return beavers::err(begin_result.error<ErrorContext>());
        }

        return AutoTransaction{std::move(tx)};
    }

    beavers::Outcome<AutoTransaction, ErrorContext>
    LockFreeSession::begin_auto_transaction(const TransactionOptions opts,
                                            std::chrono::steady_clock::duration timeout) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto holder = pool_.acquire_slot_wait(timeout);
        if (holder.expired()) {
            return beavers::err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }

        Transaction tx{std::move(holder), opts};
        if (auto begin_result = tx.begin(); !begin_result.is_success()) {
            return beavers::err(begin_result.error<ErrorContext>());
        }

        return AutoTransaction{std::move(tx)};
    }

}  // namespace menagerie::db::postgres
