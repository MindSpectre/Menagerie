#pragma once

#include <condition_variable>

namespace menagerie::db::postgres {

    class QueuedHolder;

    /**
     * @brief FIFO waiter node for BlockingPool::acquire.
     *
     * Stack-allocated inside acquire() and pushed as a raw pointer onto
     * BlockingPool::waiters_ under the pool mutex. A releaser pops the
     * head waiter, sets assigned + ready, then notifies its cv. The
     * waiter re-checks the predicate under the pool mutex to handle the
     * race between cv.wait_for returning and releaser handoff.
     */
    struct Waiter {
        QueuedHolder* assigned = nullptr;  ///< Holder handed off by the releaser; null until ready.
        std::condition_variable cv;        ///< Signaled by the releaser, or by shutdown().
        bool ready = false;                ///< True once assigned is valid (or the wait was broken by shutdown).
    };

}  // namespace menagerie::db::postgres
