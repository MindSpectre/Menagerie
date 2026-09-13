#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>

#include "pool_test_support.hpp"

using menagerie::starling::AcquireError;
using pool_test::ImmovableProbe;
using pool_test::ProbePool;
using pool_test::publish;
using namespace std::chrono_literals;

namespace {
    struct AllocationFailingExecutor {
        template <typename Function>
        void execute(Function&&) const {
            throw std::bad_alloc{};
        }

        static constexpr auto query(boost::asio::execution::blocking_t) noexcept {
            return boost::asio::execution::blocking_t::never_t{};
        }

        [[maybe_unused]] friend bool operator==(AllocationFailingExecutor, AllocationFailingExecutor) = default;
    };

    boost::asio::awaitable<void>
    hold_sentinel_until_acquired(ProbePool& pool, std::shared_ptr<int> sentinel, bool bounded, bool& resumed) {
        auto result = co_await (bounded ? pool.async_acquire_for(1h) : pool.async_acquire());
        (void)sentinel;
        resumed     = true;
    }

    void expect_context_releases_coroutine_frame(bool bounded) {
        ProbePool pool{1};
        auto sentinel = std::make_shared<int>(0);
        const std::weak_ptr<int> lifetime = sentinel;
        bool resumed                                = false;
        {
            boost::asio::io_context ioc;
            boost::asio::co_spawn(ioc, hold_sentinel_until_acquired(pool, sentinel, bounded, resumed), boost::asio::detached);
            ioc.poll();
            ASSERT_EQ(pool.waiter_count(), 1u);
            sentinel.reset();
            ASSERT_FALSE(lifetime.expired());
        }
        EXPECT_FALSE(resumed);
        EXPECT_TRUE(lifetime.expired());
        EXPECT_EQ(pool.waiter_count(), 0u);
    }
}  // namespace

TEST(PoolAsyncWait, ContextTeardownReleasesBoundedCoroutineFrame) {
    expect_context_releases_coroutine_frame(true);
}

TEST(PoolAsyncWait, ContextTeardownReleasesUnboundedCoroutineFrame) {
    expect_context_releases_coroutine_frame(false);
}

TEST(PoolAsyncWait, ContextTeardownReleasesPostedCompletionBeforeDelivery) {
    ImmovableProbe::reset_counts();
    auto pool    = std::make_unique<ProbePool>(1);
    bool resumed = false;
    {
        boost::asio::io_context ioc;
        boost::asio::cancellation_signal signal;
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                auto result = co_await pool->async_acquire_for(1h);
                resumed     = true;
            },
            boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));
        ioc.poll();
        ASSERT_EQ(pool->waiter_count(), 1u);
        publish(*pool, 41);
        ASSERT_EQ(pool->waiter_count(), 0u);
        EXPECT_EQ(ImmovableProbe::live, 1);
    }
    EXPECT_FALSE(resumed);
    EXPECT_EQ(pool->stats().idle, 1u);
    pool.reset();
    EXPECT_EQ(ImmovableProbe::live, 0);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
}

TEST(PoolAsyncWait, ContextTeardownUnlinksWaitersBeforeSurvivingFacadeShutdown) {
    ProbePool pool{1};
    boost::asio::cancellation_signal signal;
    bool resumed = false;
    {
        boost::asio::io_context ioc;
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                auto result = co_await pool.async_acquire_for(1h);
                resumed     = true;
            },
            boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                auto result = co_await pool.async_acquire();
                resumed     = true;
            },
            boost::asio::detached);
        ioc.poll();
        ASSERT_EQ(pool.waiter_count(), 2u);
    }
    EXPECT_FALSE(resumed);
    EXPECT_EQ(pool.waiter_count(), 0u);
    signal.emit(boost::asio::cancellation_type::terminal);
    pool.shutdown();
    EXPECT_TRUE(pool.is_shutdown());
}

TEST(PoolAsyncWait, ContextTeardownReturnsSelectedSlotToSurvivingPool) {
    ProbePool pool{1};
    boost::asio::cancellation_signal signal;
    bool resumed = false;
    {
        boost::asio::io_context ioc;
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                auto result = co_await pool.async_acquire_for(1h);
                resumed     = true;
            },
            boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));
        ioc.poll();
        ASSERT_EQ(pool.waiter_count(), 1u);
        publish(pool, 42);
        EXPECT_EQ(pool.stats().borrowed, 1u);
    }
    EXPECT_FALSE(resumed);
    EXPECT_EQ(pool.stats().idle, 1u);
    EXPECT_EQ(pool.stats().borrowed, 0u);
    signal.emit(boost::asio::cancellation_type::terminal);
    auto result = pool.try_acquire();
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)->id, 42u);
}

TEST(PoolAsyncWait, SelectedCompletionSchedulerAllocationFailureTerminates) {
    EXPECT_EXIT(
        {
            std::set_terminate([] { std::_Exit(86); });
            menagerie::starling::detail::pool_waiters::AsyncWaiterContext context;
            context.submit([] { boost::asio::post(AllocationFailingExecutor{}, [] { std::_Exit(87); }); });
            std::_Exit(88);
        },
        ::testing::ExitedWithCode(86),
        "");
}

TEST(PoolAsyncWait, InitiationSchedulerFailurePropagatesAndReleasesRegistryLock) {
    menagerie::starling::detail::pool_waiters::AsyncWaiterContext context;
    EXPECT_THROW(context.submit_initiation([] { boost::asio::post(AllocationFailingExecutor{}, [] {}); }),
                 std::bad_alloc);
    bool submitted = false;
    context.submit_initiation([&] { submitted = true; });
    EXPECT_TRUE(submitted);
}

TEST(PoolAsyncWait, PublishCompletesParkedCoroutine) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    std::optional<std::expected<ProbePool::Borrowed, AcquireError>> result;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> { result.emplace(co_await pool.async_acquire()); },
        boost::asio::detached);
    ioc.poll();
    EXPECT_EQ(pool.waiter_count(), 1u);
    publish(pool, 12);
    EXPECT_FALSE(result);
    ioc.restart();
    ioc.run();
    ASSERT_TRUE(result && *result);
    EXPECT_EQ((**result)->id, 12u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, AbsoluteDeadlineReportsTimeout) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    std::optional<AcquireError> error;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire_for(10ms);
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::detached);
    ioc.run();
    EXPECT_EQ(error, AcquireError::timeout);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, CancellationCompletesExactlyOnce) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    boost::asio::cancellation_signal signal;
    std::atomic<int> completions{0};
    std::optional<AcquireError> error;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            co_await boost::asio::this_coro::throw_if_cancelled(false);
            auto result = co_await pool.async_acquire_for(1s);
            ++completions;
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));

    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 1u);
    signal.emit(boost::asio::cancellation_type::terminal);
    signal.emit(boost::asio::cancellation_type::terminal);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(completions, 1);
    EXPECT_EQ(error, AcquireError::cancelled);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, CompletionReturnsToAwaitingStrand) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    auto strand    = boost::asio::make_strand(ioc);
    bool on_strand = false;
    boost::asio::co_spawn(
        strand,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire();
            EXPECT_TRUE(result);
            on_strand = strand.running_in_this_thread();
            // Reentry would deadlock if completion retained the pool mutex.
            EXPECT_EQ(pool.stats().borrowed, 1u);
        },
        boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 1u);
    std::thread publisher{[&] { publish(pool, 13); }};
    publisher.join();
    EXPECT_FALSE(on_strand);
    ioc.restart();
    ioc.run();
    EXPECT_TRUE(on_strand);
}

TEST(PoolAsyncWait, PreviouslyRequestedCancellationDoesNotClaimIdleInventory) {
    ProbePool pool{1};
    publish(pool, 14);
    boost::asio::io_context ioc;
    boost::asio::cancellation_signal signal;
    std::optional<AcquireError> error;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            co_await boost::asio::this_coro::throw_if_cancelled(false);
            signal.emit(boost::asio::cancellation_type::terminal);
            auto result = co_await pool.async_acquire();
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
            EXPECT_EQ(pool.stats().idle, 1u);
        },
        boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));
    ioc.run();
    EXPECT_EQ(error, AcquireError::cancelled);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, CancellationAfterHandoffCannotReplaceSelectedSuccess) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    boost::asio::cancellation_signal signal;
    int completions = 0;
    std::optional<ProbePool::Borrowed> received;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            co_await boost::asio::this_coro::throw_if_cancelled(false);
            auto result = co_await pool.async_acquire_for(1s);
            ++completions;
            EXPECT_TRUE(result);
            if (result)
                received.emplace(std::move(*result));
        },
        boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));
    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 1u);
    publish(pool, 15);
    signal.emit(boost::asio::cancellation_type::terminal);
    pool.shutdown();
    ioc.restart();
    ioc.run();
    EXPECT_EQ(completions, 1);
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)->id, 15u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, ImmediateSuccessAndExpiredEntryUsePublishedInventory) {
    ProbePool pool{1};
    publish(pool, 21);
    boost::asio::io_context ioc;
    auto strand     = boost::asio::make_strand(ioc);
    int completions = 0;
    boost::asio::co_spawn(
        strand,
        [&]() -> boost::asio::awaitable<void> {
            for (const auto timeout : {0ms, -1ms}) {
                auto result = co_await pool.async_acquire_for(timeout);
                EXPECT_FALSE(result);
                if (!result)
                    EXPECT_EQ(result.error(), AcquireError::timeout);
                EXPECT_EQ(pool.stats().idle, 1u);
                ++completions;
            }
            {
                auto result = co_await pool.async_acquire_for(std::chrono::steady_clock::duration::max());
                EXPECT_TRUE(result);
                if (result)
                    EXPECT_EQ((*result)->id, 21u);
                EXPECT_TRUE(strand.running_in_this_thread());
                ++completions;
            }
            auto result = co_await pool.async_acquire();
            EXPECT_TRUE(result);
            ++completions;
        },
        boost::asio::detached);
    ioc.run();
    EXPECT_EQ(completions, 4);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, DeadlineStartsWhenApiReturnsAwaitable) {
    ProbePool pool{1};
    publish(pool, 22);
    auto pending = pool.async_acquire_for(10ms);
    std::this_thread::sleep_for(25ms);
    boost::asio::io_context ioc;
    std::optional<AcquireError> error;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await std::move(pending);
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::detached);
    ioc.run();
    EXPECT_EQ(error, AcquireError::timeout);
    EXPECT_EQ(pool.stats().idle, 1u);
}

TEST(PoolAsyncWait, DelayedTimerDeliveryCannotAwardExpiredHead) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    std::optional<AcquireError> error;
    std::optional<ProbePool::Borrowed> received;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire_for(10ms);
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::detached);
    ioc.poll();
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire();
            EXPECT_TRUE(result);
            if (result)
                received.emplace(std::move(*result));
        },
        boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 2u);
    std::this_thread::sleep_for(25ms);
    // No timer callback has run. Handoff must make the deadline decision.
    publish(pool, 23);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(error, AcquireError::timeout);
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)->id, 23u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, ShutdownDrainsWaitersAndRejectsNewAsyncCalls) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    int completions = 0;
    auto wait_once  = [&]() -> boost::asio::awaitable<void> {
        auto result = co_await pool.async_acquire_for(1s);
        EXPECT_FALSE(result);
        if (!result)
            EXPECT_EQ(result.error(), AcquireError::shutdown);
        ++completions;
    };
    boost::asio::co_spawn(ioc, wait_once, boost::asio::detached);
    boost::asio::co_spawn(ioc, wait_once, boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 2u);
    pool.shutdown();
    EXPECT_EQ(completions, 0);
    boost::asio::co_spawn(ioc, wait_once, boost::asio::detached);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(completions, 3);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, SelectedCompletionDrainsAfterShutdownBeforePoolDestruction) {
    ImmovableProbe::reset_counts();
    auto pool = std::make_unique<ProbePool>(1);
    boost::asio::io_context ioc;
    std::optional<ProbePool::Borrowed> received;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool->async_acquire();
            EXPECT_TRUE(result);
            if (result)
                received.emplace(std::move(*result));
        },
        boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool->waiter_count(), 1u);
    publish(*pool, 24);
    pool->shutdown();
    EXPECT_EQ(ImmovableProbe::live, 1);
    ioc.restart();
    ioc.run();
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)->id, 24u);
    received.reset();
    EXPECT_EQ(pool->quarantined_count(), 1u);
    pool.reset();
    EXPECT_EQ(ImmovableProbe::live, 0);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
}

TEST(PoolAsyncWait, AsyncWaiterDrainsBeforeDestruction) {
    ProbePool pool{1};
    boost::asio::io_context ioc;
    std::optional<ProbePool::Borrowed> received;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire_for(1s);
            EXPECT_TRUE(result);
            if (result)
                received.emplace(std::move(*result));
        },
        boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 1u);
    publish(pool, 25);
    ioc.restart();
    ioc.run();
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)->id, 25u);
    received.reset();
    pool.shutdown();
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, ShutdownCallbacksDrainBeforePoolDestruction) {
    ImmovableProbe::reset_counts();
    auto pool = std::make_unique<ProbePool>(1);
    publish(*pool, 26);
    auto held = pool->try_acquire();
    ASSERT_TRUE(held);
    boost::asio::io_context ioc;
    std::optional<AcquireError> error;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool->async_acquire_for(1s);
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool->waiter_count(), 1u);
    pool->shutdown();
    held = std::unexpected{AcquireError::exhausted};
    EXPECT_EQ(ImmovableProbe::live, 1);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(error, AcquireError::shutdown);
    pool.reset();
    EXPECT_EQ(ImmovableProbe::live, 0);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
}

TEST(PoolMixedWait, SyncAndAsyncShareFifoOrder) {
    ProbePool pool{1};
    publish(pool, 30);
    auto held = pool.try_acquire();
    ASSERT_TRUE(held);
    std::mutex order_mutex;
    std::vector<int> order;
    auto wait_once = [&](int id) {
        return std::thread{[&, id] {
            auto result = pool.acquire();
            ASSERT_TRUE(result);
            const std::lock_guard lock{order_mutex};
            order.push_back(id);
        }};
    };
    auto first = wait_once(1);
    pool_test::await_waiters(pool, 1);
    boost::asio::io_context ioc;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto result = co_await pool.async_acquire();
            EXPECT_TRUE(result);
            const std::lock_guard lock{order_mutex};
            order.push_back(2);
        },
        boost::asio::detached);
    ioc.poll();
    EXPECT_EQ(pool.waiter_count(), 2u);
    auto third = wait_once(3);
    pool_test::await_waiters(pool, 3);
    held = std::unexpected{AcquireError::exhausted};
    ioc.restart();
    ioc.run();
    first.join();
    third.join();
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.stats().idle, 1u);
}

TEST(PoolMixedWait, QuarantineKeepsSyncAndAsyncDemandParkedUntilRepublish) {
    ProbePool pool{1};
    publish(pool, 40);
    auto borrowed = pool.try_acquire();
    ASSERT_TRUE(borrowed);
    auto* address = borrowed->get();
    boost::asio::io_context ioc;
    bool async_resumed = false;
    boost::asio::co_spawn(ioc, [&]() -> boost::asio::awaitable<void> {
        auto result = co_await pool.async_acquire();
        async_resumed = true;
        EXPECT_TRUE(result);
        if (result) {
            EXPECT_EQ(result->get(), address);
            EXPECT_EQ((*result)->id, 41u);
        }
    }, boost::asio::detached);
    ioc.poll();
    ASSERT_EQ(pool.waiter_count(), 1u);
    std::atomic<bool> sync_resumed{false};
    std::thread waiter{[&] {
        auto result = pool.acquire();
        sync_resumed = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(result->get(), address);
        EXPECT_EQ((*result)->id, 41u);
    }};
    pool_test::await_waiters(pool, 2);
    borrowed->quarantine();
    ioc.poll();
    EXPECT_FALSE(async_resumed);
    EXPECT_FALSE(sync_resumed.load());
    EXPECT_EQ(pool.waiter_count(), 2u);
    auto repair = pool.try_take_quarantined();
    if (repair) {
        EXPECT_EQ(repair->get(), address);
        repair->get()->id = 41;
        EXPECT_TRUE(repair->publish());
    } else {
        ADD_FAILURE() << "Explicit quarantine must leave its slot available for repair";
        pool.shutdown();
    }
    ioc.restart();
    ioc.run();
    waiter.join();
    EXPECT_TRUE(async_resumed);
    EXPECT_TRUE(sync_resumed.load());
    EXPECT_EQ(pool.stats().idle, 1u);
    pool_test::expect_invariant(pool);
}

TEST(PoolMixedWait, CancellationUnlinksMiddleAsyncWaiter) {
    ProbePool pool{1};
    std::mutex order_mutex;
    std::vector<int> order;
    auto wait_once = [&](int id) {
        return std::thread{[&, id] {
            auto result = pool.acquire();
            ASSERT_TRUE(result);
            const std::lock_guard lock{order_mutex};
            order.push_back(id);
        }};
    };
    auto first = wait_once(1);
    pool_test::await_waiters(pool, 1);
    boost::asio::io_context ioc;
    boost::asio::cancellation_signal signal;
    std::optional<AcquireError> error;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            co_await boost::asio::this_coro::throw_if_cancelled(false);
            auto result = co_await pool.async_acquire();
            EXPECT_FALSE(result);
            if (!result)
                error = result.error();
        },
        boost::asio::bind_cancellation_slot(signal.slot(), boost::asio::detached));
    ioc.poll();
    EXPECT_EQ(pool.waiter_count(), 2u);
    auto third = wait_once(3);
    pool_test::await_waiters(pool, 3);
    signal.emit(boost::asio::cancellation_type::terminal);
    ioc.restart();
    ioc.run();
    EXPECT_EQ(error, AcquireError::cancelled);
    EXPECT_EQ(pool.waiter_count(), 2u);
    publish(pool, 31);
    first.join();
    third.join();
    EXPECT_EQ(order, (std::vector<int>{1, 3}));
    EXPECT_EQ(pool.waiter_count(), 0u);
}
