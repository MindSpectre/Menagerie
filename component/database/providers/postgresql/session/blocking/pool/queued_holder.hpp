#pragma once

#include <memory>

#include <connection_holder.hpp>
#include <libpq-fe.h>

namespace menagerie::db::postgres {

    class BlockingPool;

    /**
     * @brief Blocking-pool concrete ConnectionHolder.
     *
     * Owned by BlockingPool as std::shared_ptr<QueuedHolder>; capabilities
     * receive std::weak_ptr<ConnectionHolder> derived from this strong ref.
     * reset() runs cleanup SQL and hands back to the pool via owner_.
     *
     * owner_ is a raw back-pointer. Pool lifetime strictly dominates
     * holder lifetime (the pool drops its holders_ vector in its dtor), so
     * the raw pointer is never dangling during reset.
     */
    class QueuedHolder final : public ConnectionHolder, public std::enable_shared_from_this<QueuedHolder> {
    public:
        /// Wraps an already-connected conn; owner is the pool that will reclaim it.
        QueuedHolder(PGconn* conn, BlockingPool* owner) noexcept;

        /// Closes the underlying connection if it is still open.
        ~QueuedHolder() override;

        /// Runs cleanup SQL, then hands the connection to a waiter or drops it if dead.
        void reset() noexcept override;

    private:
        friend class BlockingPool;
        BlockingPool* owner_;
    };

}  // namespace menagerie::db::postgres
