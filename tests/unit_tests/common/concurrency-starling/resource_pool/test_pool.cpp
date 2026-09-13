#include <array>
#include <barrier>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "pool_test_support.hpp"

using menagerie::starling::AcquireError;
using menagerie::starling::Pool;
using pool_test::ImmovableProbe;
using pool_test::ProbePool;
using pool_test::publish;

namespace {
    constexpr bool verify_token_api() {
        using TestedPool = Pool<int>;
        static_assert(std::is_nothrow_move_constructible_v<typename TestedPool::Borrowed>);
        static_assert(std::is_nothrow_move_constructible_v<typename TestedPool::ReservedSlot>);
        static_assert(!std::is_copy_constructible_v<typename TestedPool::Borrowed>);
        static_assert(!std::is_copy_constructible_v<typename TestedPool::ReservedSlot>);
        static_assert(!std::is_constructible_v<typename TestedPool::Borrowed, TestedPool*, std::size_t>);
        static_assert(!std::is_constructible_v<typename TestedPool::Borrowed, TestedPool*&, std::size_t>);
        static_assert(!std::is_constructible_v<typename TestedPool::ReservedSlot, TestedPool*, std::size_t>);
        static_assert(!std::is_constructible_v<typename TestedPool::ReservedSlot, TestedPool*&, std::size_t>);
        static_assert(std::is_same_v<decltype(std::declval<TestedPool&>().try_acquire()),
                                     std::expected<typename TestedPool::Borrowed, AcquireError>>);
        static_assert(std::is_same_v<decltype(std::declval<TestedPool&>().try_reserve()),
                                     std::optional<typename TestedPool::ReservedSlot>>);
        return true;
    }
    static_assert(verify_token_api());
    static_assert(std::is_same_v<decltype(std::declval<ProbePool::Borrowed&>().quarantine()), void>);
    static_assert(noexcept(std::declval<ProbePool::Borrowed&>().quarantine()));
}  // namespace

TEST(PoolConstruction, RejectsZeroCapacity) {
    EXPECT_THROW((ProbePool{0}), std::invalid_argument);
}

TEST(PoolQuarantine, ExplicitReturnConsumesBorrowAndPreservesObject) {
    ImmovableProbe::reset_counts();
    Pool<ImmovableProbe> pool{1};
    publish(pool, 17);
    auto borrowed = pool.try_acquire();
    ASSERT_TRUE(borrowed);
    auto* address = borrowed->get();
    borrowed->quarantine();
    EXPECT_EQ(borrowed->get(), nullptr);
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.quarantined_count(), 1u);
    EXPECT_EQ(ImmovableProbe::live, 1);
    EXPECT_EQ(ImmovableProbe::destroyed, 0);
    EXPECT_EQ(pool.try_acquire().error(), AcquireError::exhausted);
    auto repair = pool.try_take_quarantined();
    ASSERT_TRUE(repair);
    EXPECT_EQ(repair->get(), address);
    repair->destroy();
    repair->emplace(18);
    EXPECT_EQ(repair->get(), address);
    ASSERT_TRUE(repair->publish());
    auto replacement = pool.try_acquire();
    ASSERT_TRUE(replacement);
    EXPECT_EQ((*replacement)->id, 18u);
}

#ifndef NDEBUG
TEST(PoolLifetime, OutstandingBorrowIsDiagnosed) {
    EXPECT_DEATH(
        {
            auto pool = std::make_unique<Pool<ImmovableProbe>>(1);
            publish(*pool, 17);
            auto borrowed = pool->try_acquire();
            pool.reset();
        },
        "outstanding borrows");
}
#endif

TEST(PoolReservation, ReservesDistinctStableAddresses) {
    ProbePool pool{2};
    auto first  = pool.try_reserve();
    auto second = pool.try_reserve();
    ASSERT_TRUE(first && second);

    auto& a = first->emplace(11);
    auto& b = second->emplace(22);
    EXPECT_NE(std::addressof(a), std::addressof(b));
    EXPECT_FALSE(pool.try_reserve());
    EXPECT_EQ(pool.stats().reserved, 2u);
}

TEST(PoolReservation, EmptyAbandonReturnsSlot) {
    ProbePool pool{1};
    { ASSERT_TRUE(pool.try_reserve()); }
    EXPECT_EQ(pool.stats().vacant, 1u);
    EXPECT_TRUE(pool.try_reserve());
}

TEST(PoolReservation, ConstructedAbandonDestroysExactlyOnce) {
    ImmovableProbe::reset_counts();
    ProbePool pool{1};
    {
        auto slot = pool.try_reserve();
        ASSERT_TRUE(slot);
        slot->emplace(7);
        EXPECT_EQ(ImmovableProbe::live, 1);
    }
    EXPECT_EQ(ImmovableProbe::live, 0);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
    EXPECT_EQ(pool.stats().vacant, 1u);
}

TEST(PoolReservation, PlacementFactoryReceivesFinalAddress) {
    ProbePool pool{1};
    auto slot = pool.try_reserve();
    ASSERT_TRUE(slot);
    ImmovableProbe* seen = nullptr;
    auto& value          = slot->construct([&](ImmovableProbe* address) -> ImmovableProbe* {
        seen = address;
        return std::construct_at(address, 41);
    });
    EXPECT_EQ(seen, std::addressof(value));
    EXPECT_EQ(value.id, 41u);
}

TEST(PoolReservation, ThrowBeforeConstructionRollsBack) {
    ProbePool pool{1};
    EXPECT_THROW(
        {
            auto slot = pool.try_reserve();
            ASSERT_TRUE(slot);
            slot->construct([](ImmovableProbe*) -> ImmovableProbe* { throw std::runtime_error{"factory failed"}; });
        },
        std::runtime_error);
    EXPECT_EQ(pool.stats().vacant, 1u);
}

TEST(PoolConstruction, StartsVacantWithoutConstructingResources) {
    ImmovableProbe::reset_counts();
    menagerie::starling::Pool<ImmovableProbe> pool{65};
    EXPECT_EQ(pool.capacity(), 65u);
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.stats().vacant, 65u);
    EXPECT_EQ(ImmovableProbe::constructed, 0);
}

TEST(PoolReservation, PublicationTransfersOwnershipAndCountsAcrossIdleWords) {
    ImmovableProbe::reset_counts();
    {
        ProbePool pool{65};
        for (std::size_t i = 0; i < 65; ++i) {
            auto slot = pool.try_reserve();
            ASSERT_TRUE(slot);
            slot->emplace(i);
            ASSERT_TRUE(slot->publish());
            EXPECT_EQ(slot->get(), nullptr);
        }
        const auto stats = pool.stats();
        EXPECT_EQ(pool.size(), 65u);
        EXPECT_EQ(stats.capacity, 65u);
        EXPECT_EQ(stats.idle, 65u);
        EXPECT_EQ(stats.vacant, 0u);
        EXPECT_EQ(stats.reserved, 0u);
        EXPECT_EQ(stats.borrowed, 0u);
        EXPECT_EQ(stats.quarantined, 0u);
        EXPECT_EQ(stats.waiters, 0u);
        EXPECT_EQ(ImmovableProbe::destroyed, 0);
    }
    EXPECT_EQ(ImmovableProbe::destroyed, 65);
    EXPECT_EQ(ImmovableProbe::live, 0);
}

TEST(PoolReservation, ShutdownRetainsIdleAndRejectsPublicationWithoutDisarming) {
    ImmovableProbe::reset_counts();
    {
        ProbePool pool{3};
        auto idle = pool.try_reserve();
        idle->emplace(1);
        ASSERT_TRUE(idle->publish());
        auto reserved = pool.try_reserve();
        auto* value   = std::addressof(reserved->emplace(2));
        pool.shutdown();
        pool.shutdown();
        EXPECT_TRUE(pool.is_shutdown());
        EXPECT_FALSE(pool.try_reserve());
        EXPECT_FALSE(reserved->publish());
        EXPECT_EQ(reserved->get(), value);
        EXPECT_EQ(pool.size(), 0u);
        EXPECT_EQ(pool.stats().quarantined, 1u);
        EXPECT_EQ(pool.stats().reserved, 1u);
        EXPECT_EQ(pool.stats().vacant, 1u);
        EXPECT_EQ(pool.stats().idle, 0u);
        EXPECT_EQ(ImmovableProbe::destroyed, 0);
        reserved.reset();
        EXPECT_EQ(ImmovableProbe::destroyed, 1);
        EXPECT_EQ(pool.stats().vacant, 2u);
    }
    EXPECT_EQ(ImmovableProbe::destroyed, 2);
}

TEST(PoolReservation, ReservationPublishesAndRollsBackBeforeFacadeDestruction) {
    ImmovableProbe::reset_counts();
    {
        ProbePool pool{2};
        auto idle = pool.try_reserve();
        idle->emplace(1);
        ASSERT_TRUE(idle->publish());
        {
            auto reserved = pool.try_reserve();
            reserved->emplace(2);
        }
        EXPECT_EQ(pool.stats().vacant, 1u);
        EXPECT_EQ(pool.size(), 1u);
        EXPECT_EQ(ImmovableProbe::destroyed, 1);
        pool.shutdown();
    }
    EXPECT_EQ(ImmovableProbe::destroyed, 2);
}

TEST(PoolReservation, MoveAssignmentRollsBackDestinationAndTransfersSource) {
    ImmovableProbe::reset_counts();
    ProbePool pool{2};
    auto first     = pool.try_reserve();
    auto second    = pool.try_reserve();
    auto* original = std::addressof(first->emplace(1));
    second->emplace(2);
    *second = std::move(*first);
    EXPECT_EQ(first->get(), nullptr);
    EXPECT_EQ(second->get(), original);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
    EXPECT_EQ(pool.stats().reserved, 1u);
    EXPECT_EQ(pool.stats().vacant, 1u);
    auto moved = std::move(*second);
    EXPECT_EQ(second->get(), nullptr);
    EXPECT_EQ(std::as_const(moved).get(), original);
}

TEST(PoolReservation, DestroyAllowsReconstructionAtSameAddress) {
    ImmovableProbe::reset_counts();
    ProbePool pool{1};
    auto slot = pool.try_reserve();
    EXPECT_EQ(slot->get(), nullptr);
    auto* address = std::addressof(slot->emplace(1));
    slot->destroy();
    slot->destroy();
    EXPECT_EQ(slot->get(), nullptr);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
    EXPECT_EQ(pool.stats().reserved, 1u);
    EXPECT_EQ(std::addressof(slot->emplace(2)), address);
}

TEST(PoolReservation, WrongFactoryAddressLeavesReservationEmpty) {
    ProbePool pool{1};
    auto slot = pool.try_reserve();
    EXPECT_THROW(slot->construct([](ImmovableProbe*) -> ImmovableProbe* { return nullptr; }), std::invalid_argument);
    EXPECT_EQ(slot->get(), nullptr);
    EXPECT_EQ(pool.stats().reserved, 1u);
    slot->emplace(1);
    EXPECT_TRUE(slot->publish());
}

TEST(PoolBorrowed, TryAcquireReturnsTypedExhaustion) {
    ProbePool pool{1};
    auto empty = pool.try_acquire();
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error(), AcquireError::exhausted);

    publish(pool, 9);
    auto borrowed = pool.try_acquire();
    ASSERT_TRUE(borrowed);
    EXPECT_EQ((*borrowed)->id, 9u);
    EXPECT_EQ(pool.stats().borrowed, 1u);
}

TEST(PoolBorrowed, MoveAssignmentReturnsPreviousSlotOnce) {
    ProbePool pool{2};
    publish(pool, 1);
    publish(pool, 2);
    auto first  = pool.try_acquire();
    auto second = pool.try_acquire();
    ASSERT_TRUE(first && second);
    *first = std::move(*second);
    EXPECT_EQ(pool.stats().idle, 1u);
}

TEST(PoolBorrowed, NormalReturnIgnoresApplicationStatus) {
    ProbePool pool{1};
    publish(pool, 3);
    ImmovableProbe* address = nullptr;
    {
        auto borrowed = pool.try_acquire();
        ASSERT_TRUE(borrowed);
        address             = borrowed->get();
        (*borrowed)->status = ImmovableProbe::Status::unusable;
    }
    EXPECT_EQ(pool.quarantined_count(), 0u);
    auto reacquired = pool.try_acquire();
    ASSERT_TRUE(reacquired);
    EXPECT_EQ(reacquired->get(), address);
    EXPECT_EQ((*reacquired)->status, ImmovableProbe::Status::unusable);
}

TEST(PoolQuarantine, ExplicitQuarantineIsNotDestroyedOrReacquired) {
    ImmovableProbe::reset_counts();
    ProbePool pool{1};
    publish(pool, 4);
    {
        auto borrowed = pool.try_acquire();
        ASSERT_TRUE(borrowed);
        (*borrowed)->status = ImmovableProbe::Status::unusable;
        borrowed->quarantine();
    }
    EXPECT_EQ(ImmovableProbe::live, 1);
    EXPECT_EQ(pool.quarantined_count(), 1u);
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.try_acquire().error(), AcquireError::exhausted);
}

TEST(PoolQuarantine, RepeatedQuarantineAndMovingEmptyTokensReturnEachSlotOnce) {
    ProbePool pool{2};
    publish(pool, 1);
    publish(pool, 2);
    {
        auto first  = pool.try_acquire();
        auto second = pool.try_acquire();
        ASSERT_TRUE(first && second);
        auto* address = first->get();
        auto moved    = std::move(*first);
        first->quarantine();
        EXPECT_EQ(pool.size(), 2u);
        moved.quarantine();
        moved.quarantine();
        auto empty = std::move(moved);
        EXPECT_EQ(empty.get(), nullptr);
        // NOLINTNEXTLINE(bugprone-use-after-move): empty-token access is part of the contract.
        EXPECT_EQ(moved.get(), nullptr);
        *second = std::move(empty);
        second->quarantine();
        EXPECT_EQ(second->get(), nullptr);
        EXPECT_EQ(pool.size(), 1u);
        EXPECT_EQ(pool.stats().idle, 1u);
        EXPECT_EQ(pool.quarantined_count(), 1u);
        auto repair = pool.try_take_quarantined();
        ASSERT_TRUE(repair);
        EXPECT_EQ(repair->get(), address);
        EXPECT_TRUE(repair->publish());
        moved.quarantine();
        first->quarantine();
        second->quarantine();
        EXPECT_EQ(pool.quarantined_count(), 0u);
    }
    EXPECT_EQ(pool.size(), 2u);
    EXPECT_EQ(pool.stats().idle, 2u);
    EXPECT_EQ(pool.quarantined_count(), 0u);
    pool_test::expect_invariant(pool);
}

TEST(PoolQuarantine, TakeRepairAndRepublishRestoresCapacity) {
    ProbePool pool{1};
    publish(pool, 5);
    {
        auto borrowed       = pool.try_acquire();
        (*borrowed)->status = ImmovableProbe::Status::unusable;
        borrowed->quarantine();
    }
    auto repair = pool.try_take_quarantined();
    ASSERT_TRUE(repair);
    ASSERT_NE(repair->get(), nullptr);
    repair->get()->status = ImmovableProbe::Status::usable;
    ASSERT_TRUE(repair->publish());
    EXPECT_EQ(pool.size(), 1u);
}

TEST(PoolQuarantine, DestroyAndReconstructUsesSameAddress) {
    ProbePool pool{1};
    publish(pool, 6);
    {
        auto borrowed       = pool.try_acquire();
        (*borrowed)->status = ImmovableProbe::Status::unusable;
        borrowed->quarantine();
    }
    auto repair = pool.try_take_quarantined();
    ASSERT_TRUE(repair);
    const auto* original = repair->get();
    repair->destroy();
    auto& replacement = repair->emplace(7);
    EXPECT_EQ(original, std::addressof(replacement));
    ASSERT_TRUE(repair->publish());
}

TEST(PoolShutdown, RejectsNewAcquireAndPublication) {
    ProbePool pool{2};
    publish(pool, 1);
    auto reservation = pool.try_reserve();
    ASSERT_TRUE(reservation);
    reservation->emplace(2);
    pool.shutdown();
    EXPECT_EQ(pool.try_acquire().error(), AcquireError::shutdown);
    EXPECT_FALSE(reservation->publish());
}

TEST(PoolBorrowed, MovesTransferOneReturnAndPreservePointerAccess) {
    static_assert(!std::is_copy_constructible_v<ProbePool::Borrowed>);
    static_assert(!std::is_copy_assignable_v<ProbePool::Borrowed>);
    ProbePool pool{2};
    publish(pool, 1);
    publish(pool, 2);
    {
        auto first  = pool.try_acquire();
        auto second = pool.try_acquire();
        ASSERT_TRUE(first && second);
        const auto* address = second->get();
        *first              = std::move(*second);
        EXPECT_EQ(second->get(), nullptr);
        EXPECT_EQ(first->get(), address);
        EXPECT_EQ(pool.stats().idle, 1u);
        auto moved = std::move(*first);
        EXPECT_EQ(first->get(), nullptr);
        EXPECT_EQ(std::addressof(*std::as_const(moved)), address);
        auto& same = moved;
        moved      = std::move(same);
        EXPECT_EQ(moved.get(), address);
        EXPECT_EQ(pool.stats().idle, 1u);
    }
    EXPECT_EQ(pool.stats().idle, 2u);
}

TEST(PoolQuarantine, IdleExtractionOwnsConstructedSlotAndShrinksSize) {
    ImmovableProbe::reset_counts();
    ProbePool pool{2};
    EXPECT_FALSE(pool.try_take_idle());
    EXPECT_FALSE(pool.try_take_quarantined());
    publish(pool, 12);
    {
        auto taken = pool.try_take_idle();
        ASSERT_TRUE(taken);
        ASSERT_NE(taken->get(), nullptr);
        EXPECT_EQ(taken->get()->id, 12u);
        const auto stats = pool.stats();
        EXPECT_EQ(stats.vacant, 1u);
        EXPECT_EQ(stats.reserved, 1u);
        EXPECT_EQ(stats.idle, 0u);
        EXPECT_EQ(stats.borrowed, 0u);
        EXPECT_EQ(stats.quarantined, 0u);
        EXPECT_EQ(pool.size(), 0u);
        EXPECT_EQ(ImmovableProbe::destroyed, 0);
    }
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
    EXPECT_EQ(pool.stats().vacant, 2u);
}

TEST(PoolQuarantine, AbandonedRepairDestroysOnceAndRestoresVacancy) {
    ImmovableProbe::reset_counts();
    ProbePool pool{1};
    publish(pool, 1);
    {
        auto borrowed       = pool.try_acquire();
        (*borrowed)->status = ImmovableProbe::Status::unusable;
        borrowed->quarantine();
    }
    {
        auto taken = pool.try_take_quarantined();
        ASSERT_TRUE(taken);
        EXPECT_EQ(pool.quarantined_count(), 0u);
        EXPECT_EQ(pool.stats().reserved, 1u);
        EXPECT_EQ(ImmovableProbe::destroyed, 0);
    }
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
    EXPECT_EQ(pool.stats().vacant, 1u);
}

TEST(PoolShutdown, ClosedReturnRetainsResource) {
    Pool<ImmovableProbe> pool{1};
    publish(pool, 1);
    {
        auto borrowed = pool.try_acquire();
        ASSERT_TRUE(borrowed);
        pool.shutdown();
        EXPECT_EQ((*borrowed)->id, 1u);
    }
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.quarantined_count(), 1u);
    EXPECT_FALSE(pool.try_take_idle());
    EXPECT_FALSE(pool.try_take_quarantined());
}

TEST(PoolLifetime, BorrowReturnsBeforeFacadeDestruction) {
    ImmovableProbe::reset_counts();
    {
        ProbePool pool{1};
        publish(pool, 1);
        {
            auto borrowed = pool.try_acquire();
            ASSERT_TRUE(borrowed);
            EXPECT_EQ((*borrowed)->id, 1u);
        }
        EXPECT_EQ(pool.stats().idle, 1u);
        auto taken = pool.try_take_idle();
        ASSERT_TRUE(taken);
        EXPECT_TRUE(taken->publish());
    }
    EXPECT_EQ(ImmovableProbe::live, 0);
    EXPECT_EQ(ImmovableProbe::destroyed, 1);
}

TEST(PoolSyncWait, AcquireBlocksUntilPublish) {
    ProbePool pool{1};
    std::optional<ProbePool::Borrowed> received;
    std::thread waiter{[&] {
        auto result = pool.acquire();
        ASSERT_TRUE(result);
        received.emplace(std::move(*result));
    }};
    pool_test::await_waiters(pool, 1);
    EXPECT_EQ(pool.stats().waiters, 1u);
    publish(pool, 17);
    waiter.join();
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)->id, 17u);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.stats().borrowed, 1u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolSyncWait, AcquireForReportsTimeout) {
    using namespace std::chrono_literals;
    ProbePool pool{1};
    auto result = pool.acquire_for(20ms);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), AcquireError::timeout);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolSyncWait, ShutdownReportsShutdown) {
    ProbePool pool{1};
    std::optional<AcquireError> error;
    std::thread waiter{[&] {
        auto result = pool.acquire();
        ASSERT_FALSE(result);
        error = result.error();
    }};
    pool_test::await_waiters(pool, 1);
    pool.shutdown();
    waiter.join();
    EXPECT_EQ(error, AcquireError::shutdown);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolSyncWait, HealthyReturnHandsOffInFifoOrder) {
    ProbePool pool{1};
    publish(pool, 1);
    auto initial = pool.try_acquire();
    ASSERT_TRUE(initial);
    std::optional<ProbePool::Borrowed> held;
    held.emplace(std::move(*initial));

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
    auto second = wait_once(2);
    pool_test::await_waiters(pool, 2);
    auto third = wait_once(3);
    pool_test::await_waiters(pool, 3);

    held.reset();
    first.join();
    second.join();
    third.join();
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.stats().idle, 1u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolSyncWait, ResourceReleasedAfterDeadlineIsNotAwardedLate) {
    using namespace std::chrono_literals;
    ProbePool pool{1};
    publish(pool, 1);
    auto acquired = pool.try_acquire();
    ASSERT_TRUE(acquired);
    std::optional<ProbePool::Borrowed> held;
    held.emplace(std::move(*acquired));

    std::optional<AcquireError> error;
    std::thread waiter{[&] {
        auto result = pool.acquire_for(10ms);
        ASSERT_FALSE(result);
        error = result.error();
    }};
    waiter.join();
    held.reset();
    EXPECT_EQ(error, AcquireError::timeout);
    EXPECT_EQ(pool.stats().idle, 1u);
}

TEST(PoolMaintenance, TryTakeIdleCannotStealFromWaiter) {
    ProbePool pool{1};
    std::atomic<bool> received{false};
    std::atomic<bool> release_waiter{false};
    std::thread waiter{[&] {
        auto result = pool.acquire_for(std::chrono::seconds{1});
        received.store(result.has_value(), std::memory_order_release);
        EXPECT_TRUE(pool_test::eventually([&] { return release_waiter.load(std::memory_order_acquire); }));
    }};
    pool_test::await_waiters(pool, 1);
    publish(pool, 2);
    EXPECT_TRUE(pool_test::eventually([&] { return received.load(std::memory_order_acquire); }));
    EXPECT_FALSE(pool.try_take_idle());
    release_waiter.store(true, std::memory_order_release);
    waiter.join();
}

TEST(PoolSyncWait, ImmediateAcquisitionUsesPublishedInventory) {
    using namespace std::chrono_literals;
    ProbePool pool{2};
    publish(pool, 11);
    publish(pool, 22);
    auto first  = pool.acquire();
    auto second = pool.acquire_for(1s);
    ASSERT_TRUE(first && second);
    EXPECT_EQ((*first)->id, 11u);
    EXPECT_EQ((*second)->id, 22u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolSyncWait, ExpiredEntryDeadlineDoesNotConsumeIdleInventory) {
    using namespace std::chrono_literals;
    ProbePool pool{1};
    publish(pool, 9);
    EXPECT_EQ(pool.acquire_for(0ms).error(), AcquireError::timeout);
    EXPECT_EQ(pool.acquire_for(-1ms).error(), AcquireError::timeout);
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.stats().idle, 1u);
}

TEST(PoolSyncWait, MaximumDurationAcquiresPublishedInventory) {
    ProbePool pool{1};
    publish(pool, 47);
    auto result = pool.acquire_for(std::chrono::steady_clock::duration::max());
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)->id, 47u);
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.stats().borrowed, 1u);
}

TEST(PoolSyncWait, AbsoluteDeadlineSaturatesAtClockBounds) {
    using Clock     = std::chrono::steady_clock;
    using Duration  = Clock::duration;
    using TimePoint = Clock::time_point;
    struct Case {
        TimePoint now{};
        Duration timeout{};
        TimePoint expected{};
    };
    const std::array cases{
        Case{TimePoint{Duration{1}},                   Duration::max(),  TimePoint::max()      },
        Case{TimePoint{Duration{-1}},                  Duration::min(),  TimePoint::min()      },
        Case{TimePoint{Duration::max() - Duration{1}}, Duration{1},      TimePoint::max()      },
        Case{TimePoint{Duration::min() + Duration{1}}, Duration{-1},     TimePoint::min()      },
        Case{TimePoint{Duration{4}},                   Duration{3},      TimePoint{Duration{7}}},
        Case{TimePoint{Duration{4}},                   Duration{-3},     TimePoint{Duration{1}}},
        Case{TimePoint{Duration{4}},                   Duration::zero(), TimePoint{Duration{4}}},
    };
    for (const auto& entry : cases) {
        EXPECT_EQ(menagerie::starling::detail::pool_deadline_after(entry.now, entry.timeout), entry.expected);
    }
}

TEST(PoolSyncWait, TimeoutUnlinksMiddleWaiterWithoutChangingFifo) {
    using namespace std::chrono_literals;
    ProbePool pool{1};
    std::vector<int> order;
    std::mutex order_mutex;
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
    std::optional<AcquireError> error;
    std::thread middle{[&] {
        auto result = pool.acquire_for(500ms);
        ASSERT_FALSE(result);
        error = result.error();
    }};
    pool_test::await_waiters(pool, 2);
    auto third = wait_once(3);
    pool_test::await_waiters(pool, 3);
    middle.join();
    EXPECT_EQ(error, AcquireError::timeout);
    EXPECT_EQ(pool.stats().waiters, 2u);
    publish(pool, 4);
    first.join();
    third.join();
    EXPECT_EQ(order, (std::vector<int>{1, 3}));
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolSyncWait, ShutdownDrainsAllWaitersAndRejectsNewBlockingCalls) {
    using namespace std::chrono_literals;
    ProbePool pool{1};
    std::vector<std::thread> waiters;
    waiters.reserve(8);
    std::atomic<int> shutdowns{0};
    for (int i = 0; i < 8; ++i) {
        waiters.emplace_back([&] {
            auto result = pool.acquire_for(2s);
            if (!result && result.error() == AcquireError::shutdown)
                ++shutdowns;
        });
    }
    pool_test::await_waiters(pool, 8);
    pool.shutdown();
    for (auto& waiter : waiters)
        waiter.join();
    EXPECT_EQ(shutdowns, 8);
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.acquire().error(), AcquireError::shutdown);
    EXPECT_EQ(pool.acquire_for(1s).error(), AcquireError::shutdown);
}

TEST(PoolSyncWait, WaiterReceivesPublicationAndDrainsBeforeDestruction) {
    ProbePool pool{1};
    std::optional<ProbePool::Borrowed> received;
    std::thread waiter{[&] {
        auto result = pool.acquire();
        ASSERT_TRUE(result);
        received.emplace(std::move(*result));
    }};
    pool_test::await_waiters(pool, 1);
    publish(pool, 31);
    waiter.join();
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)->id, 31u);
    received.reset();
    EXPECT_EQ(pool.stats().idle, 1u);
    pool.shutdown();
}

TEST(PoolSyncWait, ConcurrentReturnAndRegistrationDoNotLoseWakeup) {
    using namespace std::chrono_literals;
    ProbePool pool{1};
    publish(pool, 1);
    std::barrier start{2};
    std::atomic<int> acquired{0};
    std::thread waiter{[&] {
        for (int i = 0; i < 300; ++i) {
            start.arrive_and_wait();
            {
                auto result = pool.acquire_for(1s);
                if (result)
                    ++acquired;
            }
            start.arrive_and_wait();
        }
    }};
    for (int i = 0; i < 300; ++i) {
        auto borrowed = pool.try_acquire();
        EXPECT_TRUE(borrowed);
        start.arrive_and_wait();
        borrowed = std::unexpected{AcquireError::exhausted};
        start.arrive_and_wait();
    }
    waiter.join();
    EXPECT_EQ(acquired, 300);
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.stats().idle, 1u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
