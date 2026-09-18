#pragma once

#include <atomic>
#include <chrono>
#include <expected>
#include <mutex>

#include "borrowed.hpp"
#include "waiter.hpp"

namespace menagerie::starling::detail {

    template <typename T>
    class PoolSyncAcquire {
    public:
        static std::expected<PoolBorrowed<T>, AcquireError>
        acquire(Pool<T>* pool, const std::chrono::steady_clock::time_point deadline) {
            const auto expired = [&] {
                return deadline != std::chrono::steady_clock::time_point::max() &&
                       std::chrono::steady_clock::now() >= deadline;
            };
            if (pool->closed_.load(std::memory_order_acquire)) {
                return std::unexpected{AcquireError::shutdown};
            }
            if (expired()) {
                return std::unexpected{AcquireError::timeout};
            }
            if (const auto index = pool->claim_idle()) {
                if (pool->closed_.load(std::memory_order_acquire)) {
                    const std::lock_guard lock{pool->mutex_};
                    pool->quarantine_borrowed(*index);
                    return std::unexpected{AcquireError::shutdown};
                }
                if (expired()) {
                    pool->handoff_or_publish(*index);
                    return std::unexpected{AcquireError::timeout};
                }
                return PoolBorrowed<T>{pool, *index};
            }

            pool_waiters::SyncWaiter waiter;
            waiter.deadline = deadline;
            std::unique_lock lock{pool->mutex_};
            if (pool->closed_.load(std::memory_order_relaxed)) {
                return std::unexpected{AcquireError::shutdown};
            }
            if (expired()) {
                return std::unexpected{AcquireError::timeout};
            }
            pool->waiter_gate_.fetch_add(1, std::memory_order_seq_cst);
            if (const auto index = pool->claim_idle(true)) {
                pool->waiter_gate_.fetch_sub(1, std::memory_order_seq_cst);
                if (expired()) {
                    pool->handoff_or_publish_locked(*index, lock);
                    return std::unexpected{AcquireError::timeout};
                }
                return PoolBorrowed<T>{pool, *index};
            }
            pool->waiters_.push_back(waiter);
            pool->assert_invariants_locked();
            const auto completed = [&] { return waiter.state != pool_waiters::WaiterState::queued; };
            if (waiter.bounded()) {
                waiter.cv.wait_until(lock, deadline, completed);
            } else {
                waiter.cv.wait(lock, completed);
            }
            if (waiter.state == pool_waiters::WaiterState::queued) {
                pool->unlink_waiter(waiter, pool_waiters::WaiterState::timeout);
                return std::unexpected{AcquireError::timeout};
            }
            lock.unlock();
            {
                // The selector pins this stack frame until notify_one finishes.
                // Never acquire the pool mutex while holding this pin.
                const std::lock_guard notification{waiter.notification_mutex};
            }
            switch (waiter.state) {
                case pool_waiters::WaiterState::assigned:
                    return PoolBorrowed<T>{pool, waiter.assigned_slot};
                case pool_waiters::WaiterState::timeout:
                    return std::unexpected{AcquireError::timeout};
                case pool_waiters::WaiterState::shutdown:
                    return std::unexpected{AcquireError::shutdown};
                case pool_waiters::WaiterState::queued:
                case pool_waiters::WaiterState::cancelled:
                    std::unreachable();
            }
            std::unreachable();
        }
    };

}  // namespace menagerie::starling::detail
