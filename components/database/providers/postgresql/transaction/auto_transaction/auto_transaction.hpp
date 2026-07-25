#pragma once

#include <capability_provider.hpp>

#include "base/transaction.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief Transaction that is already ACTIVE because BEGIN was sent by whoever
     *        created it (see Session::begin_auto_transaction()).
     *
     * Thin move-only wrapper delegating to an inner Transaction; satisfies
     * CapabilityProvider on its own. Destroying it without commit() runs the same
     * connection-holder cleanup path an abandoned Transaction would.
     */
    class AutoTransaction : public beavers::NonCopyable {
    public:
        /**
         * @brief Set cleanup SQL to run when this transaction releases the slot
         * @param query Predefined cleanup query
         */
        template <typename Self>
        auto&& do_cleanup(this Self&& self, CleanupQuery query) noexcept {
            self.tx_.do_cleanup(query);
            return std::forward<Self>(self);
        }

        /// Sends COMMIT, moving the underlying transaction ACTIVE -> COMMITTED.
        [[nodiscard]] beavers::Outcome<void, ErrorContext> commit();

        /// Borrows a synchronous executor bound to this transaction's connection.
        [[nodiscard]] beavers::Outcome<SyncExecutor, ErrorContext> with_sync() const;
        /**
         * @brief Borrows an asynchronous executor bound to this transaction's connection.
         * @param exec Boost.Asio executor the async operations complete on.
         */
        [[nodiscard]] beavers::Outcome<AsyncExecutor, ErrorContext> with_async(boost::asio::any_io_executor exec) const;

        /// Creates a nested savepoint named `name` within this transaction.
        [[nodiscard]] beavers::Outcome<Savepoint, ErrorContext> savepoint(std::string name) const;

        /// Current lifecycle state of the underlying transaction.
        [[nodiscard]] TransactionStatus status() const noexcept;
        /// True while the underlying transaction is ACTIVE.
        [[nodiscard]] bool is_active() const noexcept;
        /// True once the underlying transaction has COMMITTED or ROLLED_BACK.
        [[nodiscard]] bool is_finished() const noexcept;

    private:
        friend class LockFreeSession;
        friend class BlockingSession;
        explicit AutoTransaction(Transaction tx);

        Transaction tx_;
    };

    static_assert(CapabilityProvider<AutoTransaction>);
}  // namespace menagerie::db::postgres
