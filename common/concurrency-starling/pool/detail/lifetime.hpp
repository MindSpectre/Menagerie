#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <menagerie/beaver>
#include <utility>

namespace menagerie::starling::detail {

    // Cold coroutine lifetime diagnosis, including unlinked/posted completions.
    // Release builds have neither a counter nor atomic bookkeeping.
    class AsyncOperationCount {
    public:
#ifndef NDEBUG
        void started() noexcept {
            count_.fetch_add(1, std::memory_order_relaxed);
        }
        void finished() noexcept {
            count_.fetch_sub(1, std::memory_order_relaxed);
        }
        void assert_drained() const noexcept {
            assert(count_.load(std::memory_order_relaxed) == 0 && "Pool destroyed with outstanding async operations");
        }

    private:
        std::atomic<std::size_t> count_{0};
#else
        void started() const noexcept {
            beaver::force_non_static(this);
        }
        void finished() const noexcept {
            beaver::force_non_static(this);
        }
        void assert_drained() const noexcept {
            beaver::force_non_static(this);
        }
#endif
    };

    template <typename PoolT>
    class AsyncPoolReference {
    public:
        PoolT* pool;
        explicit AsyncPoolReference(PoolT* value)
            : pool{value} {
            pool->async_operations_.started();
        }
        AsyncPoolReference(AsyncPoolReference&& other) noexcept
            : pool{std::exchange(other.pool, {})} {
        }
        ~AsyncPoolReference() {
            if (pool)
                pool->async_operations_.finished();
        }
    };

}  // namespace menagerie::starling::detail
