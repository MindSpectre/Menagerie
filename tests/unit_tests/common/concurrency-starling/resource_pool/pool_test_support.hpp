#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <menagerie/starling>
#include <thread>

#include <gtest/gtest.h>

namespace pool_test {

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::steady_clock::duration timeout = std::chrono::seconds{5}) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!predicate()) {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            std::this_thread::yield();
        }
        return true;
    }

    // Nonfatal failure plus shutdown lets each test reach its existing joins.
    template <typename PoolT>
    void await_waiters(PoolT& pool, std::size_t count) {
        if (!eventually([&] { return pool.waiter_count() == count; })) {
            ADD_FAILURE() << "Waiter count did not reach " << count;
            pool.shutdown();
        }
    }

    template <typename PoolT>
    void expect_invariant(const PoolT& pool) {
        const auto s = pool.stats();
        EXPECT_EQ(s.capacity, s.vacant + s.reserved + s.quarantined + s.idle + s.borrowed);
        EXPECT_EQ(s.idle + s.borrowed, pool.size());
        EXPECT_EQ(s.waiters, pool.waiter_count());
    }

    struct ImmovableProbe {
        enum class Status : std::uint8_t { usable, unusable };

        static inline std::atomic<int> live{0};
        static inline std::atomic<int> constructed{0};
        static inline std::atomic<int> destroyed{0};

        std::size_t id;
        Status status = Status::usable;

        explicit ImmovableProbe(std::size_t value) noexcept
            : id{value} {
            ++live;
            ++constructed;
        }

        ImmovableProbe(const ImmovableProbe&)            = delete;
        ImmovableProbe& operator=(const ImmovableProbe&) = delete;
        ImmovableProbe(ImmovableProbe&&)                 = delete;
        ImmovableProbe& operator=(ImmovableProbe&&)      = delete;

        ~ImmovableProbe() noexcept {
            --live;
            ++destroyed;
        }

        static void reset_counts() noexcept {
            live        = 0;
            constructed = 0;
            destroyed   = 0;
        }
    };

    template <typename PoolT>
    void publish(PoolT& pool, std::size_t id) {
        auto slot = pool.try_reserve();
        ASSERT_TRUE(slot);
        slot->emplace(id);
        ASSERT_TRUE(slot->publish());
    }

    using ProbePool = menagerie::starling::Pool<ImmovableProbe>;

}  // namespace pool_test
