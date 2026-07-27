#pragma once

#include <memory>
#include <menagerie/beavers>
#include <menagerie/crow>

#include <capability_provider.hpp>
#include <connection_holder.hpp>

#include "options/transaction_options.hpp"
#include "status/transaction_status.hpp"

namespace menagerie::db::postgres {

    class LockFreeSession;
    class BlockingSession;
    class Savepoint;

    /**
     * @brief Explicit-lifecycle SQL transaction on a borrowed PostgreSQL connection.
     *
     * Move-only; holds a std::weak_ptr<ConnectionHolder> so the borrowed slot is
     * released automatically once the last Transaction referencing it is destroyed.
     * begin() / commit() / rollback() are explicit and state-checked: calling them,
     * or with_sync() / with_async(), while in the wrong TransactionStatus returns
     * ErrorContext{ClientErrorCode::InvalidState} instead of misbehaving. Destroying
     * an ACTIVE Transaction without commit() runs a safety-net ROLLBACK before the
     * connection holder is released.
     */
    class Transaction : beavers::NonCopyable {
    public:
        /// Runs a safety-net ROLLBACK if still ACTIVE, then releases the connection holder.
        ~Transaction();

        /// Transfers ownership of the borrowed connection; other is left with no holder and status FAILED.
        Transaction(Transaction&& other) noexcept;
        /// Releases this transaction's holder (if any), then takes over other's; other is left with no holder and
        /// status FAILED.
        Transaction& operator=(Transaction&& other) noexcept;

        // -------- Cleanup --------

        /**
         * @brief Set cleanup SQL to run when this transaction releases the slot
         * @param query Predefined cleanup query
         */
        template <typename Self>
        constexpr auto&& do_cleanup(this Self&& self, const CleanupQuery query) noexcept {
            if (auto live = self.holder_.lock()) {
                live->set_cleanup(query);
            }
            return std::forward<Self>(self);
        }

        // -------- Lifecycle Control (sync) --------

        /**
         * @brief Sends BEGIN using the configured TransactionOptions, moving IDLE -> ACTIVE.
         * @return Success, or ErrorContext{InvalidState} if this transaction is not IDLE.
         * @throw std::invalid_argument under the same condition as TransactionOptions::to_begin_sql().
         */
        [[nodiscard]] beavers::Outcome<void, ErrorContext> begin();

        /// Sends COMMIT, moving ACTIVE -> COMMITTED. Returns ErrorContext{InvalidState} unless ACTIVE.
        [[nodiscard]] beavers::Outcome<void, ErrorContext> commit();

        /// Sends ROLLBACK, moving ACTIVE -> ROLLED_BACK. Returns ErrorContext{InvalidState} unless ACTIVE.
        [[nodiscard]] beavers::Outcome<void, ErrorContext> rollback();

        // -------- Capability Provision --------

        /// Borrows a synchronous executor bound to this transaction's connection. Returns ErrorContext{InvalidState}
        /// unless ACTIVE.
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext> with_sync() const;
        /**
         * @brief Borrows an asynchronous executor bound to this transaction's connection.
         * @param exec Boost.Asio executor the async operations complete on.
         * @return The executor, or ErrorContext{InvalidState} unless this transaction is ACTIVE.
         */
        [[nodiscard]] beavers::Outcome<AsyncExecutor, ErrorContext> with_async(boost::asio::any_io_executor exec) const;

        // -------- Savepoints --------

        /**
         * @brief Creates a nested savepoint named `name` within this transaction.
         * @return The Savepoint, or ErrorContext{InvalidState} unless ACTIVE, or
         *         ErrorContext{InvalidArgument} if name is not a valid identifier.
         */
        [[nodiscard]] beavers::Outcome<Savepoint, ErrorContext> savepoint(std::string name) const;

        // -------- Introspection --------

        /// Current lifecycle state of this transaction.
        [[nodiscard]] constexpr TransactionStatus status() const noexcept {
            return status_;
        }

        /// True while status() is ACTIVE.
        [[nodiscard]] constexpr bool is_active() const noexcept {
            return status_ == TransactionStatus::ACTIVE;
        }

        /// True once status() is COMMITTED or ROLLED_BACK.
        [[nodiscard]] constexpr bool is_finished() const noexcept {
            return status_ == TransactionStatus::COMMITTED || status_ == TransactionStatus::ROLLED_BACK;
        }

        /// Underlying libpq connection handle used by this transaction.
        [[nodiscard]] constexpr PGconn* native_handle() const noexcept {
            return conn_;
        }

    private:
        friend class LockFreeSession;
        friend class BlockingSession;

        CROW_COMPONENT_PREFIX("Transaction");

        std::weak_ptr<ConnectionHolder> holder_;
        PGconn* conn_ = nullptr;
        TransactionOptions options_;
        TransactionStatus status_ = TransactionStatus::IDLE;

        Transaction(std::weak_ptr<ConnectionHolder> holder, TransactionOptions opts);

        [[nodiscard]] beavers::Outcome<void, ErrorContext> execute_control(const std::string& sql) const;
    };

    static_assert(CapabilityProvider<Transaction>);
}  // namespace menagerie::db::postgres
