#pragma once

#include <memory>

#include <connection.hpp>
#include <connection_holder.hpp>

namespace menagerie::savanna::elephant {

    class ConnectionPool;

    /**
     * @brief The pool's concrete ConnectionHolder.
     *
     * Owned by ConnectionPool as std::shared_ptr<QueuedHolder>; capabilities
     * receive std::weak_ptr<ConnectionHolder> derived from this strong ref.
     * reset() runs cleanup SQL (the borrower's choice, or the pool's configured
     * default) and hands back to the pool via owner_.
     *
     * owner_ is a raw back-pointer. Pool lifetime strictly dominates
     * holder lifetime (the pool drops its holders_ vector in its dtor), so
     * the raw pointer is never dangling during reset.
     */
    class QueuedHolder final : public ConnectionHolder, public std::enable_shared_from_this<QueuedHolder> {
    public:
        /// Takes ownership of an already-open connection; owner is the pool that will reclaim it.
        QueuedHolder(Connection conn, ConnectionPool* owner) noexcept;

        /// The owned Connection closes itself.
        ~QueuedHolder() override = default;

        /// Runs cleanup SQL, then hands the connection to a waiter or drops it if BROKEN.
        void reset() noexcept override;

    private:
        friend class ConnectionPool;
        ConnectionPool* owner_;
    };

}  // namespace menagerie::savanna::elephant
