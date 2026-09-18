#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <utility>

#include <boost/asio/execution_context.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive_ptr.hpp>

namespace menagerie::starling::detail::pool_waiters {

    enum class WaiterKind : std::uint8_t { sync, async };
    enum class WaiterState : std::uint8_t { queued, assigned, timeout, cancelled, shutdown };

    using WaiterHook = boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>>;

    struct WaiterNode {
        WaiterHook hook;
        WaiterKind kind{};
        WaiterState state                              = WaiterState::queued;
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
        std::size_t assigned_slot                      = static_cast<std::size_t>(-1);

        [[nodiscard]] bool bounded() const noexcept {
            return deadline != std::chrono::steady_clock::time_point::max();
        }
    };

    using WaiterList = boost::intrusive::list<WaiterNode,
                                              boost::intrusive::member_hook<WaiterNode, WaiterHook, &WaiterNode::hook>,
                                              boost::intrusive::constant_time_size<true>>;

    struct SyncWaiter : WaiterNode {
        SyncWaiter() noexcept {
            kind = WaiterKind::sync;
        }
        std::condition_variable cv;
        // Selection takes this while holding the pool mutex, then retains it
        // through notify_one() after unlocking the pool. A timed/spurious wake
        // must pass this mutex before destroying its stack-owned cv.
        std::mutex notification_mutex;
    };

    class IntrusiveRefCounted {
    public:
        friend void intrusive_ptr_add_ref(IntrusiveRefCounted* pointer) noexcept {
            pointer->refs_.fetch_add(1, std::memory_order_relaxed);
        }

        friend void intrusive_ptr_release(IntrusiveRefCounted* pointer) noexcept {
            if (pointer->refs_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete pointer;
            }
        }

    protected:
        virtual ~IntrusiveRefCounted() = default;

    private:
        std::atomic_uint refs_{0};
    };

    // The intrusive queue is independent of the concrete coroutine handler.
    // Only the parked async arm uses this thunk; the concrete payload stores its
    // handler in place and constructs the Borrowed after the pool is unlocked.
    struct AsyncWaiterNode : WaiterNode, IntrusiveRefCounted {
        explicit AsyncWaiterNode(void (*completion)(boost::intrusive_ptr<AsyncWaiterNode>) noexcept) noexcept
            : complete{completion} {
            kind = WaiterKind::async;
        }

        void (*complete)(boost::intrusive_ptr<AsyncWaiterNode>) noexcept;
        WaiterHook context_hook;

        virtual void prepare_abandon() noexcept = 0;
        virtual void abandon() noexcept         = 0;
    };

    // Context ownership also covers unbounded waits, for which Asio owns no
    // timer operation. This registry is lifecycle bookkeeping, not an inventory
    // queue: all acquisition order remains in the single mixed WaiterList.
    class AsyncWaiterContext final : public IntrusiveRefCounted {
        using List = boost::intrusive::list<
            AsyncWaiterNode,
            boost::intrusive::member_hook<AsyncWaiterNode, WaiterHook, &AsyncWaiterNode::context_hook>,
            boost::intrusive::constant_time_size<false>>;

    public:
        void add(AsyncWaiterNode& waiter) noexcept {
            const std::lock_guard lock{mutex_};
            assert(!stopping_);
            intrusive_ptr_add_ref(&waiter);
            waiters_.push_back(waiter);
        }

        void remove(AsyncWaiterNode& waiter) noexcept {
            {
                const std::lock_guard lock{mutex_};
                waiters_.erase(waiters_.iterator_to(waiter));
            }
            intrusive_ptr_release(&waiter);
        }

        template <typename Schedule>
        void submit_initiation(Schedule&& schedule) {
            const std::lock_guard lock{mutex_};
            if (stopping_) {
                return;
            }
            std::forward<Schedule>(schedule)();
        }

        template <typename Schedule>
        void submit(Schedule&& schedule) noexcept {
            // A selected operation cannot be retried or completed inline from
            // a noexcept token return. Scheduler failure is unrecoverable;
            // terminate explicitly rather than silently losing completion.
            try {
                submit_initiation(std::forward<Schedule>(schedule));
            } catch (...) {
                std::terminate();
            }
        }

        void shutdown() noexcept {
            List abandoned;
            {
                const std::lock_guard lock{mutex_};
                stopping_ = true;
                abandoned.splice(abandoned.end(), waiters_);
                // Unlink EVERY waiter before destroying ANY coroutine frame:
                // a frame may own a Pool whose destructor drains other waiters.
                // Core selection always unlocks before entering this service.
                for (auto& waiter : abandoned) {
                    waiter.prepare_abandon();
                }
            }
            while (!abandoned.empty()) {
                boost::intrusive_ptr<AsyncWaiterNode> waiter{&abandoned.front(), false};
                abandoned.pop_front();
                waiter->abandon();
            }
        }

    private:
        std::mutex mutex_;
        List waiters_;
        bool stopping_ = false;
    };

    class AsyncWaiterService final : public boost::asio::execution_context::service {
    public:
        static inline boost::asio::execution_context::id id;

        explicit AsyncWaiterService(boost::asio::execution_context& context)
            : service{context},
              waiters{new AsyncWaiterContext} {
        }

        // One allocation per execution_context. A returning borrower that has
        // already selected a waiter can outlive service destruction: it keeps
        // this stopped gate alive and never posts through the dead executor.
        const boost::intrusive_ptr<AsyncWaiterContext> waiters;

    private:
        void shutdown() noexcept override {
            waiters->shutdown();
        }
    };

}  // namespace menagerie::starling::detail::pool_waiters
