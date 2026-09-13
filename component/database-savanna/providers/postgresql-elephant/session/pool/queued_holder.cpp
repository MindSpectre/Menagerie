#include "queued_holder.hpp"

#include "connection_pool.hpp"

namespace menagerie::savanna::elephant {

    QueuedHolder::QueuedHolder(Connection conn, ConnectionPool* owner) noexcept
        : ConnectionHolder{std::move(conn)},
          owner_{owner} {
    }

    void QueuedHolder::reset() noexcept {
        if (!run_cleanup_sql(owner_->pool_config().cleanup_sql())) {
            owner_->drop_dead(this);
            return;
        }
        owner_->return_holder(this);
    }

}  // namespace menagerie::savanna::elephant
