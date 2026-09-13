#include <barrier>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <unordered_set>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include "pool_test_support.hpp"

using namespace pool_test;
using menagerie::starling::AcquireError;
using namespace std::chrono_literals;

TEST(PoolStress, ReservationsNeverAliasAcrossConcurrentConstruction) {
    ImmovableProbe::reset_counts();
    {
        ProbePool pool{64};
        std::mutex mutex;
        std::unordered_set<ImmovableProbe*> reserved_addresses;
        std::atomic<int> completed{0};
        std::vector<std::thread> threads;
        threads.reserve(16);
        for (int worker = 0; worker < 16; ++worker) {
            threads.emplace_back([&, worker] {
                for (int iteration = 0; iteration < 2000; ++iteration) {
                    auto slot = pool.try_reserve();
                    ASSERT_TRUE(slot);
                    auto& resource = slot->construct([&](ImmovableProbe* address) {
                        return std::construct_at(address, static_cast<std::size_t>(worker));
                    });
                    {
                        const std::lock_guard lock{mutex};
                        EXPECT_TRUE(reserved_addresses.insert(&resource).second);
                    }
                    std::this_thread::yield();
                    {
                        const std::lock_guard lock{mutex};
                        EXPECT_EQ(reserved_addresses.erase(&resource), 1u);
                    }
                    if (iteration % 2 == 0) {
                        ASSERT_TRUE(slot->publish());
                        {
                            auto borrow = pool.acquire_for(1s);
                            ASSERT_TRUE(borrow);
                        }
                        std::optional<ProbePool::ReservedSlot> retired;
                        ASSERT_TRUE(eventually([&] {
                            retired = pool.try_take_idle();
                            return retired.has_value();
                        }));
                    }
                    ++completed;
                }
            });
        }
        for (auto& thread : threads)
            thread.join();
        EXPECT_EQ(completed, 32000);
        EXPECT_TRUE(reserved_addresses.empty());
        EXPECT_EQ(pool.stats().vacant, 64u);
        expect_invariant(pool);
    }
    EXPECT_EQ(ImmovableProbe::constructed, 32000);
    EXPECT_EQ(ImmovableProbe::destroyed, 32000);
    EXPECT_EQ(ImmovableProbe::live, 0);
}

TEST(PoolStress, ReturnRacingRegistrationNeverLosesWakeup) {
    ProbePool pool{1};
    publish(pool, 1);
    std::barrier start{2};
    std::atomic<int> acquired{0};
    std::thread waiter{[&] {
        for (int round = 0; round < 2000; ++round) {
            start.arrive_and_wait();
            {
                auto result = pool.acquire_for(1s);
                if (result)
                    ++acquired;
            }
            start.arrive_and_wait();
        }
    }};
    for (int round = 0; round < 2000; ++round) {
        auto held = pool.try_acquire();
        EXPECT_TRUE(held);
        start.arrive_and_wait();
        held = std::unexpected{AcquireError::exhausted};
        start.arrive_and_wait();
    }
    waiter.join();
    EXPECT_EQ(acquired, 2000);
    EXPECT_EQ(pool.waiter_count(), 0u);
    expect_invariant(pool);
}

TEST(PoolStress, AsyncTimeoutStormUnlinksEveryWaiter) {
    ProbePool pool{1};
    publish(pool, 1);
    auto held = pool.try_acquire();
    ASSERT_TRUE(held);
    boost::asio::io_context ioc;
    std::atomic<int> timeouts{0};
    constexpr int waiter_count = 4096;
    for (int i = 0; i < waiter_count; ++i) {
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                auto result = co_await pool.async_acquire_for(5ms);
                if (!result && result.error() == AcquireError::timeout)
                    ++timeouts;
            },
            boost::asio::detached);
    }
    const auto start = std::chrono::steady_clock::now();
    ioc.run();
    RecordProperty(
        "cleanup_ms",
        static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));
    EXPECT_EQ(timeouts, waiter_count);
    EXPECT_EQ(pool.waiter_count(), 0u);
    expect_invariant(pool);
}

TEST(PoolStress, PublicationAfterTimeoutRemainsIdle) {
    ProbePool pool{1};
    auto result = pool.acquire_for(5ms);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), AcquireError::timeout);
    publish(pool, 1);
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.stats().idle, 1u);
    expect_invariant(pool);
}

TEST(PoolRace, TryAcquireAgainstShutdown) {
    for (int round = 0; round < 500; ++round) {
        ProbePool pool{1};
        publish(pool, 1);
        std::barrier start{2};
        std::thread claimant{[&] {
            start.arrive_and_wait();
            auto result = pool.try_acquire();
            if (result)
                EXPECT_EQ((*result)->id, 1u);
            else
                EXPECT_EQ(result.error(), AcquireError::shutdown);
        }};
        start.arrive_and_wait();
        pool.shutdown();
        claimant.join();
        EXPECT_EQ(pool.quarantined_count(), 1u);
        expect_invariant(pool);
    }
}

TEST(PoolRace, ReservationPublicationAgainstShutdown) {
    ImmovableProbe::reset_counts();
    for (int round = 0; round < 500; ++round) {
        ProbePool pool{1};
        auto slot = pool.try_reserve();
        ASSERT_TRUE(slot);
        slot->emplace(1);
        std::barrier start{2};
        bool published = false;
        std::thread publisher{[&, reservation = std::move(*slot)]() mutable {
            start.arrive_and_wait();
            published = reservation.publish();
        }};
        start.arrive_and_wait();
        pool.shutdown();
        publisher.join();
        EXPECT_EQ(pool.stats().quarantined, published ? 1u : 0u);
        EXPECT_EQ(pool.stats().vacant, published ? 0u : 1u);
        expect_invariant(pool);
    }
    EXPECT_EQ(ImmovableProbe::constructed, 500);
    EXPECT_EQ(ImmovableProbe::destroyed, 500);
    EXPECT_EQ(ImmovableProbe::live, 0);
}

TEST(PoolRace, BorrowReturnAgainstShutdownQuarantinesExactlyOnce) {
    for (int round = 0; round < 500; ++round) {
        ProbePool pool{1};
        publish(pool, 1);
        auto held = pool.try_acquire();
        ASSERT_TRUE(held);
        std::barrier start{2};
        std::thread returning{[&, borrowed = std::move(*held)]() mutable {
            start.arrive_and_wait();
            { auto released = std::move(borrowed); }
        }};
        start.arrive_and_wait();
        pool.shutdown();
        returning.join();
        EXPECT_EQ(pool.quarantined_count(), 1u);
        EXPECT_EQ(pool.stats().idle, 0u);
        EXPECT_EQ(pool.stats().borrowed, 0u);
        expect_invariant(pool);
    }
}

TEST(PoolStress, ReservationAndCompletionDrainBeforePoolDestruction) {
    ImmovableProbe::reset_counts();
    boost::asio::io_context ioc;
    auto pool = std::make_unique<ProbePool>(1);
    auto slot = pool->try_reserve();
    ASSERT_TRUE(slot);
    slot->emplace(1);
    std::optional<AcquireError> error;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool->async_acquire();
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool->waiter_count(), 1u);
    pool->shutdown();
    EXPECT_FALSE(slot->publish());
    slot.reset();
    EXPECT_EQ(ImmovableProbe::live, 0);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(error, AcquireError::shutdown);
    pool.reset();
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
}

TEST(PoolRace, ExplicitQuarantineAgainstShutdownConsumesExactlyOnce) {
    for (int iteration = 0; iteration < 300; ++iteration) {
        ProbePool pool{1};
        publish(pool, 1);
        auto borrowed = pool.try_acquire();
        ASSERT_TRUE(borrowed);
        std::barrier start{2};
        std::thread quarantining{[&] {
            start.arrive_and_wait();
            borrowed->quarantine();
            borrowed->quarantine();
        }};
        start.arrive_and_wait();
        pool.shutdown();
        quarantining.join();
        EXPECT_EQ(borrowed->get(), nullptr);
        EXPECT_EQ(pool.quarantined_count(), 1u);
        EXPECT_EQ(pool.size(), 0u);
        EXPECT_EQ(pool.stats().borrowed, 0u);
        expect_invariant(pool);
    }
}

#ifndef NDEBUG
TEST(PoolLifetime, OutstandingReservationIsDiagnosed) {
    EXPECT_DEATH({
        auto pool = std::make_unique<ProbePool>(1);
        auto reserved = pool->try_reserve();
        pool.reset();
    }, "outstanding reservations");
}

TEST(PoolLifetime, UnstartedAwaitableIsDiagnosed) {
    EXPECT_DEATH({
        auto pool = std::make_unique<ProbePool>(1);
        auto pending = pool->async_acquire();
        pool.reset();
    }, "outstanding async");
}

TEST(PoolLifetime, UnstartedBoundedAwaitableIsDiagnosed) {
    EXPECT_DEATH({
        auto pool = std::make_unique<ProbePool>(1);
        auto pending = pool->async_acquire_for(1h);
        pool.reset();
    }, "outstanding async");
}

TEST(PoolLifetime, UnlinkedAsyncCompletionIsDiagnosed) {
    EXPECT_DEATH(
        {
            boost::asio::io_context ioc;
            auto pool = std::make_unique<ProbePool>(1);
            boost::asio::co_spawn(
                ioc,
                [&]() -> boost::asio::awaitable<void> {
                    auto result = co_await pool->async_acquire();
                    (void)result;
                },
                boost::asio::detached);
            ioc.poll();
            pool->shutdown();
            // Queue is empty, but its selected completion still holds a raw Core.
            pool.reset();
            std::_Exit(0);
        },
        "outstanding async");
}
#endif

TEST(PoolFunctional, TransactionsQuarantineRepairAndMaintenance) {
    ProbePool pool{2};
    {
        auto empty = pool.try_reserve();
        ASSERT_TRUE(empty);
    }
    {
        auto abandoned = pool.try_reserve();
        ASSERT_TRUE(abandoned);
        abandoned->construct([](ImmovableProbe* address) { return std::construct_at(address, 10); });
    }
    EXPECT_EQ(pool.stats().vacant, 2u);
    publish(pool, 11);
    {
        auto borrow = pool.try_acquire();
        ASSERT_TRUE(borrow);
        (*borrow)->status = ImmovableProbe::Status::unusable;
        borrow->quarantine();
    }
    EXPECT_EQ(pool.quarantined_count(), 1u);
    EXPECT_EQ(pool.try_acquire().error(), AcquireError::exhausted);
    auto repair = pool.try_take_quarantined();
    ASSERT_TRUE(repair);
    auto* address = repair->get();
    repair->destroy();
    EXPECT_EQ(&repair->emplace(12), address);
    ASSERT_TRUE(repair->publish());
    auto idle = pool.try_take_idle();
    ASSERT_TRUE(idle);
    EXPECT_EQ(idle->get()->id, 12u);
    EXPECT_TRUE(idle->publish());
    pool.shutdown();
    EXPECT_EQ(pool.acquire().error(), AcquireError::shutdown);
    EXPECT_FALSE(pool.try_reserve());
    EXPECT_FALSE(pool.try_take_idle());
    EXPECT_FALSE(pool.try_take_quarantined());
    expect_invariant(pool);
}

TEST(PoolFunctional, MixedWaitersDrainBeforeFacadeDestruction) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    std::vector<int> order;
    std::mutex mutex;
    std::thread first{[&] {
        auto result = pool.acquire_for(2s);
        EXPECT_TRUE(result);
        const std::lock_guard lock{mutex};
        order.push_back(1);
    }};
    await_waiters(pool, 1);
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire_for(2s);
            EXPECT_TRUE(result);
            const std::lock_guard lock{mutex};
            order.push_back(2);
        },
        boost::asio::detached);
    ioc.poll();
    EXPECT_EQ(pool.waiter_count(), 2u);
    publish(pool, 1);
    first.join();
    ioc.restart();
    ioc.run();
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_EQ(pool.stats().idle, 1u);
    expect_invariant(pool);
}

TEST(PoolFunctional, TimeoutCancellationAndShutdownDrainAsyncOperations) {
    ProbePool pool{1};
    EXPECT_EQ(pool.acquire_for(1ms).error(), AcquireError::timeout);
    boost::asio::io_context ioc;
    boost::asio::cancellation_signal cancellation;
    std::optional<AcquireError> cancelled;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            co_await boost::asio::this_coro::throw_if_cancelled(false);
            auto result = co_await pool.async_acquire();
            EXPECT_FALSE(result);
            if (!result)
                cancelled = result.error();
        },
        boost::asio::bind_cancellation_slot(cancellation.slot(), boost::asio::detached));
    ioc.poll();
    EXPECT_EQ(pool.waiter_count(), 1u);
    cancellation.emit(boost::asio::cancellation_type::terminal);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(cancelled, AcquireError::cancelled);
    std::optional<AcquireError> timeout;
    std::optional<AcquireError> shutdown;
    ioc.restart();
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire_for(1ms);
            if (!result)
                timeout = result.error();
        },
        boost::asio::detached);
    ioc.run();
    EXPECT_EQ(timeout, AcquireError::timeout);
    ioc.restart();
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire();
            if (!result)
                shutdown = result.error();
        },
        boost::asio::detached);
    ioc.poll();
    pool.shutdown();
    ioc.restart();
    ioc.run();
    EXPECT_EQ(shutdown, AcquireError::shutdown);
    expect_invariant(pool);
}
