#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <menagerie/starling>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/bind_immediate_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/system/error_code.hpp>
#include <gtest/gtest.h>

using menagerie::starling::Pool;
using menagerie::starling::Wait;

namespace {
    // Counts constructions/destructions so eager creation, lazy creation, dead-drop
    // and shutdown teardown are all observable.
    struct Probe {
        static inline std::atomic<int> live{0};
        static inline std::atomic<int> made{0};
        std::size_t id;

        explicit Probe(const std::size_t i)
            : id{i} {
            live.fetch_add(1);
        }
        Probe(Probe&& o) noexcept
            : id{o.id} {
            live.fetch_add(1);
        }
        ~Probe() {
            live.fetch_sub(1);
        }
        static void reset() {
            live.store(0);
            made.store(0);
        }
        static std::optional<Probe> make() {
            return Probe{static_cast<std::size_t>(made.fetch_add(1))};
        }
    };

    using SyncPool  = Pool<Wait::sync, Probe>;
    using AsyncPool = Pool<Wait::async, Probe>;
}  // namespace

TEST(PoolConstruction, ValidatesArguments) {
    EXPECT_THROW((SyncPool{0, 0, &Probe::make}), std::invalid_argument);
    EXPECT_THROW((SyncPool{4, 5, &Probe::make}), std::invalid_argument);
}

TEST(PoolConstruction, EagerlyCreatesMinKeepsCapacityLazy) {
    Probe::reset();
    {
        SyncPool pool{8, 3, &Probe::make};
        EXPECT_EQ(pool.capacity(), 8u);
        EXPECT_EQ(pool.free_count(), 3u);
        EXPECT_EQ(pool.active_count(), 0u);
        EXPECT_EQ(Probe::made.load(), 3);
    }
    EXPECT_EQ(Probe::live.load(), 0);  // dtor (via shutdown) destroyed the idle nodes
}

TEST(PoolTryAcquire, UsesFreeThenCreatesToCapacityThenFails) {
    Probe::reset();
    SyncPool pool{2, 1, &Probe::make};
    auto a = pool.try_acquire();  // eager node
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(Probe::made.load(), 1);
    auto b = pool.try_acquire();  // lazy create
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(Probe::made.load(), 2);
    EXPECT_NE((*a)->id, (*b)->id);
    EXPECT_FALSE(pool.try_acquire().has_value());  // capacity reached
    EXPECT_EQ(pool.active_count(), 2u);
}

TEST(PoolTryAcquire, FactoryFailureDoesNotLeakCapacity) {
    std::atomic<int> calls{0};
    Pool<Wait::sync, int> pool{2, 0, [&calls]() -> std::optional<int> {
                                   calls.fetch_add(1);
                                   return std::nullopt;  // creation always fails
                               }};
    EXPECT_FALSE(pool.try_acquire().has_value());
    EXPECT_FALSE(pool.try_acquire().has_value());
    EXPECT_EQ(calls.load(), 2);  // capacity was rolled back, so it retried
    EXPECT_EQ(pool.active_count(), 0u);
}

TEST(PoolHandle, ReleaseReturnsTheSameSlot) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    std::size_t first_id = 0;
    {
        auto h   = pool.try_acquire();
        first_id = (*h)->id;
        EXPECT_EQ(pool.free_count(), 0u);
    }
    EXPECT_EQ(pool.free_count(), 1u);
    auto h2 = pool.try_acquire();
    EXPECT_EQ((*h2)->id, first_id);  // recycled, not re-made
    EXPECT_EQ(Probe::made.load(), 1);
}

TEST(PoolHandle, MarkDeadDropsAndAllowsRecreation) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    {
        auto h   = pool.try_acquire();
        (*h)->id = 999;  // mutate to prove the next one is fresh
        (*h).mark_dead();
    }
    EXPECT_EQ(pool.free_count(), 0u);
    EXPECT_EQ(pool.active_count(), 0u);
    auto h2 = pool.try_acquire();  // capacity was freed -> factory runs again
    ASSERT_TRUE(h2.has_value());
    EXPECT_EQ(Probe::made.load(), 2);
    EXPECT_NE((*h2)->id, 999u);
}

TEST(PoolHandle, MoveTransfersOwnership) {
    Probe::reset();
    SyncPool pool{2, 2, &Probe::make};
    auto a = pool.try_acquire();
    typename SyncPool::Handle b{std::move(*a)};
    EXPECT_TRUE(static_cast<bool>(b));
    auto c = pool.try_acquire();
    *c     = std::move(b);  // move-assign over a live handle releases c's old slot
    EXPECT_EQ(pool.free_count(), 1u);
    c.reset();  // optional reset -> Handle dtor -> back to 2
    EXPECT_EQ(pool.free_count(), 2u);
}

TEST(PoolShutdown, DestroysIdleAndRefusesNewAcquires) {
    Probe::reset();
    SyncPool pool{4, 4, &Probe::make};
    pool.shutdown();
    EXPECT_TRUE(pool.is_shutdown());
    EXPECT_EQ(Probe::live.load(), 0);
    EXPECT_FALSE(pool.try_acquire().has_value());
    pool.shutdown();  // idempotent
}

TEST(PoolShutdown, CheckedOutHandleDiesInlineAfterShutdown) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    {
        auto h = pool.try_acquire();
        pool.shutdown();
        EXPECT_EQ(Probe::live.load(), 1);  // checked-out node survives shutdown
    }  // release after shutdown destroys inline
    EXPECT_EQ(Probe::live.load(), 0);
    EXPECT_EQ(pool.free_count(), 0u);
}

using namespace std::chrono_literals;

TEST(PoolSyncWait, AcquireBlocksUntilRelease) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    auto held = pool.try_acquire();
    std::atomic<bool> got{false};
    std::thread waiter{[&] {
        auto h = pool.acquire();  // unbounded
        got.store(h.has_value(), std::memory_order_release);
    }};
    while (pool.waiter_count() < 1) {
        std::this_thread::yield();
    }
    held.reset();  // release -> direct handoff
    waiter.join();
    EXPECT_TRUE(got.load(std::memory_order_acquire));
}

TEST(PoolSyncWait, AcquireForTimesOutAndUnregisters) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    auto held     = pool.try_acquire();
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(pool.acquire_for(50ms).has_value());
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 40ms);
    EXPECT_EQ(pool.waiter_count(), 0u);  // timed-out waiter removed itself
}

TEST(PoolSyncWait, HandoffIsFifoAndSkipsFreeList) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    auto held = pool.try_acquire();
    std::vector<int> order;
    std::mutex order_mtx;
    std::atomic<int> parked{0};
    auto wait_thread = [&](int idx) {
        return std::thread{[&, idx] {
            parked.fetch_add(1);
            auto h = pool.acquire();
            ASSERT_TRUE(h.has_value());
            {
                const std::lock_guard g{order_mtx};
                order.push_back(idx);
            }
            std::this_thread::sleep_for(20ms);  // hold so the next handoff is observable
        }};
    };
    std::thread t1 = wait_thread(1);
    while (pool.waiter_count() < 1) {
        std::this_thread::yield();
    }
    std::thread t2 = wait_thread(2);
    while (pool.waiter_count() < 2) {
        std::this_thread::yield();
    }
    held.reset();  // -> t1 (FIFO head), then t1's release -> t2
    t1.join();
    t2.join();
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_EQ(pool.free_count(), 1u);  // finally free after t2's release
}

TEST(PoolSyncWait, ShutdownWakesAllWaitersEmpty) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    auto held = pool.try_acquire();
    std::atomic<int> empties{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < 3; ++i) {
        ts.emplace_back([&] {
            if (!pool.acquire().has_value()) {
                empties.fetch_add(1);
            }
        });
    }
    while (pool.waiter_count() < 3) {
        std::this_thread::yield();
    }
    pool.shutdown();
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(empties.load(), 3);
    held.reset();  // post-shutdown release destroys inline; dtor assert holds
}

TEST(PoolAsyncWait, FastPathCompletesViaImmediateExecutorBeforeRun) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    bool done = false;
    pool.acquire(ioc.get_executor(),
                 boost::asio::bind_immediate_executor(boost::asio::system_executor{},
                                                      [&](const boost::system::error_code ec, AsyncPool::Handle h) {
                                                          EXPECT_FALSE(ec);
                                                          EXPECT_TRUE(static_cast<bool>(h));
                                                          done = true;
                                                      }));
    EXPECT_TRUE(done);  // completed inline — ioc.run() never called
}

TEST(PoolAsyncWait, FastPathWithoutImmediateExecutorPosts) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    bool done = false;
    pool.acquire(ioc.get_executor(), [&](const boost::system::error_code ec, AsyncPool::Handle h) {
        EXPECT_FALSE(ec);
        EXPECT_TRUE(static_cast<bool>(h));
        done = true;
    });
    EXPECT_FALSE(done);  // not yet: default immediate executor == post to exec
    ioc.run();
    EXPECT_TRUE(done);
}

TEST(PoolAsyncWait, ParkedWaiterWokenByRelease) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    auto held = pool.try_acquire();
    bool done = false;
    pool.acquire(ioc.get_executor(), [&](const boost::system::error_code ec, AsyncPool::Handle h) {
        EXPECT_FALSE(ec);
        EXPECT_TRUE(static_cast<bool>(h));
        done = true;
    });
    EXPECT_EQ(pool.waiter_count(), 1u);
    held.reset();  // direct handoff -> posts completion
    ioc.run();
    EXPECT_TRUE(done);
    EXPECT_EQ(pool.free_count(), 1u);  // handle inside the lambda released; resource returned
}

TEST(PoolAsyncWait, BoundedWaitTimesOut) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    auto held = pool.try_acquire();
    boost::system::error_code seen{};
    pool.acquire_for(ioc.get_executor(), 50ms, [&](const boost::system::error_code ec, AsyncPool::Handle h) {
        seen = ec;
        EXPECT_FALSE(static_cast<bool>(h));
    });
    ioc.run();
    EXPECT_EQ(seen, boost::asio::error::timed_out);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, ShutdownDrainsParkedWaiter) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    auto held = pool.try_acquire();
    boost::system::error_code seen{};
    pool.acquire(ioc.get_executor(), [&](const boost::system::error_code ec, AsyncPool::Handle) { seen = ec; });
    pool.shutdown();
    ioc.run();
    EXPECT_EQ(seen, boost::asio::error::operation_aborted);
    held.reset();
}

TEST(PoolAsyncWait, ShutdownDuringLazyCreateDoesNotStrandWaiter) {
    Probe::reset();
    std::atomic<bool> factory_started{false};
    std::condition_variable factory_cv;
    std::mutex factory_mtx;

    AsyncPool pool{1, 0, [&]() -> std::optional<Probe> {
                       factory_started.store(true);
                       std::unique_lock lk{factory_mtx};
                       factory_cv.wait(lk, [&] { return pool.is_shutdown(); });
                       return std::nullopt;
                   }};

    boost::asio::io_context ioc;
    boost::system::error_code seen{};

    // Acquire in a thread: will block inside the factory during obtain_locked
    std::thread acquire_thread{[&] {
        pool.acquire(ioc.get_executor(), [&](const boost::system::error_code ec, AsyncPool::Handle) { seen = ec; });
    }};

    // Wait for factory to start
    while (!factory_started.load()) {
        std::this_thread::yield();
    }

    // Now shutdown (which drains the empty waiter list) while factory is still blocked
    pool.shutdown();

    // Unblock the factory. Hold factory_mtx while notifying: the factory evaluates
    // its predicate under this mutex, and the shutdown flag it reads is not itself
    // guarded by it — a bare notify issued between the predicate evaluation and the
    // park is lost and the factory sleeps forever (seen under TSan timing).
    {
        const std::lock_guard g{factory_mtx};
        factory_cv.notify_all();
    }

    // Join acquire thread
    acquire_thread.join();

    // Run io_context to execute any pending handlers
    ioc.run();

    // Assert: the handler completed exactly once with operation_aborted
    EXPECT_EQ(seen, boost::asio::error::operation_aborted);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, CancellationSlotAbortsParkedWaiterExactlyOnce) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    auto held = pool.try_acquire();
    boost::asio::cancellation_signal sig;
    std::atomic<int> completions{0};
    boost::system::error_code seen{};
    pool.acquire(
        ioc.get_executor(),
        boost::asio::bind_cancellation_slot(sig.slot(), [&](const boost::system::error_code ec, AsyncPool::Handle h) {
            completions.fetch_add(1);
            seen = ec;
            EXPECT_FALSE(static_cast<bool>(h));
        }));
    EXPECT_EQ(pool.waiter_count(), 1u);
    sig.emit(boost::asio::cancellation_type::terminal);
    ioc.run();
    EXPECT_EQ(completions.load(), 1);
    EXPECT_EQ(seen, boost::asio::error::operation_aborted);
    EXPECT_EQ(pool.waiter_count(), 0u);
    held.reset();  // release after cancel must take the publish path, not strand
    EXPECT_TRUE(pool.try_acquire().has_value());
}

TEST(PoolSyncWait, DeadReleaseCreatesReplacementForParkedWaiter) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    auto held = pool.try_acquire();
    ASSERT_TRUE(held.has_value());
    std::optional<SyncPool::Handle> got;
    std::thread waiter{[&] { got = pool.acquire_for(std::chrono::seconds{2}); }};
    while (pool.waiter_count() == 0) {
        std::this_thread::yield();
    }
    held->mark_dead();
    held.reset();  // destroys the only resource; must kick the parked waiter to recreate
    waiter.join();
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(Probe::made.load(), 2);  // the eager resource + the kicked recreation
    got.reset();
    EXPECT_EQ(pool.active_count(), 0u);
}

TEST(PoolAsyncWait, DeadReleaseCreatesReplacementForParkedWaiter) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    auto held = pool.try_acquire();
    ASSERT_TRUE(held.has_value());
    std::atomic<int> completions{0};
    boost::system::error_code seen{boost::asio::error::would_block};
    pool.acquire_for(
        ioc.get_executor(), std::chrono::seconds{2}, [&](const boost::system::error_code ec, AsyncPool::Handle h) {
            completions.fetch_add(1);
            seen = ec;
            EXPECT_EQ(static_cast<bool>(h), !ec);
            h = {};
        });
    EXPECT_EQ(pool.waiter_count(), 1u);
    held->mark_dead();
    held.reset();  // dead release must post a one-shot creator job for the parked waiter
    ioc.run();     // runs the creator job (factory) and then the handed-off completion
    EXPECT_EQ(completions.load(), 1);
    EXPECT_EQ(seen, boost::system::error_code{});
    EXPECT_EQ(Probe::made.load(), 2);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolAsyncWait, ParkedCompletionHonorsBoundExecutor) {
    Probe::reset();
    AsyncPool pool{1, 1, &Probe::make};
    boost::asio::io_context ioc;
    auto strand = boost::asio::make_strand(ioc);
    auto held   = pool.try_acquire();
    ASSERT_TRUE(held.has_value());
    std::atomic<int> completions{0};
    bool in_strand = false;
    pool.acquire(ioc.get_executor(),
                 boost::asio::bind_executor(strand, [&](const boost::system::error_code ec, AsyncPool::Handle h) {
                     completions.fetch_add(1);
                     in_strand = strand.running_in_this_thread();
                     EXPECT_FALSE(ec);
                     h = {};
                 }));
    EXPECT_EQ(pool.waiter_count(), 1u);
    held.reset();  // direct handoff: the completion must run on the bound strand
    ioc.run();
    EXPECT_EQ(completions.load(), 1);
    EXPECT_TRUE(in_strand);
}

TEST(PoolAsyncWait, CancellationEmitAfterCompletionAndPoolDeathIsInert) {
    Probe::reset();
    boost::asio::io_context ioc;
    boost::asio::cancellation_signal sig;
    std::atomic<int> completions{0};
    {
        AsyncPool pool{1, 1, &Probe::make};
        auto held = pool.try_acquire();
        ASSERT_TRUE(held.has_value());
        pool.acquire_for(ioc.get_executor(),
                         std::chrono::seconds{2},
                         boost::asio::bind_cancellation_slot(
                             sig.slot(), [&](const boost::system::error_code ec, AsyncPool::Handle h) {
                                 completions.fetch_add(1);
                                 EXPECT_FALSE(ec);
                                 h = {};
                             }));
        EXPECT_EQ(pool.waiter_count(), 1u);
        held.reset();  // handoff completes the waiter
        ioc.run();
        EXPECT_EQ(completions.load(), 1);
    }  // pool destroyed; the caller-owned slot still holds the pool's stale callback
    sig.emit(boost::asio::cancellation_type::terminal);  // must be inert: no UAF, no second completion
    EXPECT_EQ(completions.load(), 1);
}

TEST(PoolAsyncWait, ReleaseVsTimeoutCompletesExactlyOnce) {
    // Cross-thread race: release and timeout fire concurrently; claimed CAS ensures
    // exactly-once completion. Test is repeated 50 times with varied delays to shake
    // the race condition.
    Probe::reset();
    for (int round = 0; round < 50; ++round) {
        AsyncPool pool{1, 1, &Probe::make};
        boost::asio::io_context ioc;
        auto guard = boost::asio::make_work_guard(ioc);
        std::thread runner{[&] { ioc.run(); }};
        auto held = pool.try_acquire();
        ASSERT_TRUE(held.has_value());
        std::atomic<int> completions{0};
        std::atomic<bool> done{false};
        pool.acquire_for(ioc.get_executor(),
                         std::chrono::microseconds{500},
                         [&](const boost::system::error_code, AsyncPool::Handle h) {
                             completions.fetch_add(1);
                             h = {};
                             done.store(true, std::memory_order_release);
                         });
        std::this_thread::sleep_for(std::chrono::microseconds{300 + 137 * (round % 5)});
        held.reset();  // races the 500us timer, which the runner thread is actively driving
        while (!done.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        EXPECT_EQ(completions.load(), 1) << "round " << round;
        guard.reset();
        runner.join();
        // Drain: whichever side lost may still hold the slot briefly; wait for it to return.
        while (pool.free_count() + pool.active_count() != pool.capacity() || pool.active_count() != 0) {
            std::this_thread::yield();
        }
    }
    EXPECT_EQ(Probe::live.load(), 0);
}

TEST(PoolStress, SyncChurnKeepsInvariants) {
    constexpr std::size_t kThreads = 8;
    constexpr int kIters           = 2000;
    Probe::reset();
    SyncPool pool{4, 2, &Probe::make};
    std::atomic<int> acquired{0};
    std::vector<std::thread> ts;
    for (std::size_t t = 0; t < kThreads; ++t) {
        ts.emplace_back([&, t] {
            for (int i = 0; i < kIters; ++i) {
                if (auto h = pool.acquire_for(5ms)) {
                    acquired.fetch_add(1, std::memory_order_relaxed);
                    if ((t + static_cast<std::size_t>(i)) % 97 == 0) {
                        (*h)->id += 1;  // touch the resource
                        h->mark_dead();
                    }
                }
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }
    EXPECT_GT(acquired.load(), 0);
    EXPECT_LE(pool.free_count() + pool.active_count(), pool.capacity());
    EXPECT_EQ(pool.active_count(), 0u);
    EXPECT_EQ(pool.waiter_count(), 0u);
}

TEST(PoolStress, AsyncTimeoutChurnCompletesEveryInitiation) {
    // Every initiation must complete exactly once (success or timed_out), under
    // saturation with releases racing timers — the claimed-CAS contract.
    constexpr int kCoros = 64;
    constexpr int kLoops = 200;
    Probe::reset();
    AsyncPool pool{4, 4, &Probe::make};
    boost::asio::io_context ioc;
    std::atomic<int> completions{0};
    std::function<void(int)> spawn = [&](int remaining) {
        if (remaining == 0) {
            return;
        }
        pool.acquire_for(ioc.get_executor(), 1ms, [&, remaining](const boost::system::error_code, AsyncPool::Handle h) {
            completions.fetch_add(1, std::memory_order_relaxed);
            h = {};  // release (no-op on timeout)
            spawn(remaining - 1);
        });
    };
    for (int c = 0; c < kCoros; ++c) {
        spawn(kLoops);
    }
    ioc.run();
    EXPECT_EQ(completions.load(), kCoros * kLoops);
    EXPECT_EQ(pool.waiter_count(), 0u);
    EXPECT_EQ(pool.active_count(), 0u);
}

TEST(PoolSlots, DeadSlotIsRecycledUpToCapacity) {
    Probe::reset();
    SyncPool pool{2, 2, &Probe::make};
    for (int round = 0; round < 5; ++round) {
        auto a = pool.try_acquire();
        auto b = pool.try_acquire();
        ASSERT_TRUE(a.has_value() && b.has_value()) << "round " << round;
        EXPECT_FALSE(pool.try_acquire().has_value());  // capacity honored every round
        (*a).mark_dead();
        a.reset();  // destroyed; its slot id must be reusable next round
        b.reset();
    }
    EXPECT_EQ(pool.active_count(), 0u);
    EXPECT_EQ(pool.free_count(), 1u);             // the dropped node is re-created lazily on demand
    EXPECT_TRUE(pool.try_acquire().has_value());  // and creation still works (slot ids not exhausted)
}

TEST(PoolSlots, GateCounterReturnsToZeroAfterChurn) {
    Probe::reset();
    SyncPool pool{1, 1, &Probe::make};
    auto held = pool.try_acquire();
    std::thread t1{[&] { (void)pool.acquire_for(30ms); }};  // will time out
    std::thread t2{[&] {
        auto h = pool.acquire();
        (void)h;
    }};  // will be handed off
    while (pool.waiter_count() < 2) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(60ms);  // let t1 time out and unregister
    held.reset();                       // handoff to t2
    t1.join();
    t2.join();
    EXPECT_EQ(pool.waiter_count(), 0u);
    // The gate must be exact after all waiters resolved: a leaked count would send
    // every future release through the mutex slow path forever.
    auto again = pool.try_acquire();
    ASSERT_TRUE(again.has_value());
    again.reset();  // this release must take the lock-free publish path (not observable
                    // directly; the assertion is waiter_count()==0 + no deadlock/hang)
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
