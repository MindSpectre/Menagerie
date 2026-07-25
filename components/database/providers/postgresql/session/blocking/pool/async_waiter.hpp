#pragma once

#include <atomic>
#include <memory>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace menagerie::db::postgres {

    class QueuedHolder;

    /**
     * @brief Heap-allocated waiter node for BlockingPool::async_acquire.
     *
     * Unlike the sync Waiter (stack + CV), AsyncWaiter holds a type-erased
     * asio completion handler bound to the caller's executor and a
     * steady_timer for bounded waits. The atomic claimed flag resolves
     * the race between "pool released a slot to me" and "my timer fired";
     * whichever side flips the flag first drives the completion.
     *
     * Lifetime: owned by BlockingPool via std::shared_ptr while queued.
     * Timer callbacks hold only a std::weak_ptr, so shutdown clears the
     * deque and any lagging callback no-ops safely.
     */
    struct AsyncWaiter : beavers::NonCopyable {
        /// Completion signature: (error_code, acquired holder or null on failure).
        using Handler =
            boost::asio::any_completion_handler<void(boost::system::error_code, std::shared_ptr<QueuedHolder>)>;

        /// Binds the timer to the caller's executor so its callback runs there too.
        explicit AsyncWaiter(boost::asio::any_io_executor exec) noexcept
            : caller_exec{std::move(exec)},
              timer{caller_exec} {
        }

        boost::asio::any_io_executor caller_exec;  ///< Executor the completion handler is posted to.
        boost::asio::steady_timer timer;            ///< Armed only for bounded (timed) acquires.
        Handler handler{};                          ///< Completion callback; moved out and invoked exactly once.
        std::atomic_bool claimed{false};             ///< CAS flag: first of {release, timeout, shutdown} wins the waiter.
    };

}  // namespace menagerie::db::postgres
