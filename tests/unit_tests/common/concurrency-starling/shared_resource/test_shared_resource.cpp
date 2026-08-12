#include <atomic>
#include <chrono>
#include <cstddef>
#include <menagerie/starling>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

using namespace menagerie::starling;

namespace {
    constexpr std::size_t kThreads    = 8;
    constexpr std::size_t kIncrements = 10'000;

    /// Number of reader threads that held a `ReadProxy` at the same time.
    ///
    /// Every reader parks inside the proxy's scope until `release` flips, so the
    /// count observed before that flip is the concurrency `read()` actually
    /// granted. The wait runs in two bounded phases so the answer does not
    /// depend on scheduling latency: a generous one for the first reader to
    /// arrive at all, then a short settle window for the rest.
    std::size_t concurrent_read_holders(SharedResource<long long>& resource) {
        std::atomic<std::size_t> holding{0};
        std::atomic<bool> release{false};

        std::vector<std::jthread> readers;
        readers.reserve(kThreads);
        for (std::size_t i = 0; i < kThreads; ++i) {
            readers.emplace_back([&] {
                const auto proxy = resource.read();
                holding.fetch_add(1, std::memory_order_release);
                while (!release.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                EXPECT_EQ(*proxy, 7LL);
            });
        }

        const auto arrival_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (holding.load(std::memory_order_acquire) == 0 &&
               std::chrono::steady_clock::now() < arrival_deadline) {
            std::this_thread::yield();
        }

        const auto settle_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{200};
        while (holding.load(std::memory_order_acquire) < kThreads &&
               std::chrono::steady_clock::now() < settle_deadline) {
            std::this_thread::yield();
        }
        const std::size_t observed = holding.load(std::memory_order_acquire);

        release.store(true, std::memory_order_release);  // must precede the join
        readers.clear();                                 // jthread dtors join
        return observed;
    }
}  // namespace

//
// Construction semantics -- the distinction introduced in #39.
//

// The size literal is unsigned on purpose: the build runs -Wconversion -Werror,
// so a plain `5` here is a sign-conversion error against vector's size_type.
TEST(SharedResourceTest, ParenthesizedFormForwardsToElementConstructor) {
    SharedResource<std::vector<int>> resource(5u);
    EXPECT_EQ(resource.read()->size(), 5u);
}

TEST(SharedResourceTest, BracedFormUsesInitializerList) {
    SharedResource<std::vector<int>> resource{5};
    ASSERT_EQ(resource.read()->size(), 1u);
    EXPECT_EQ((*resource.read())[0], 5);
}

TEST(SharedResourceTest, BracedFormAcceptsMultipleElements) {
    SharedResource<std::vector<int>> resource{1, 2, 3};
    ASSERT_EQ(resource.read()->size(), 3u);
    EXPECT_EQ((*resource.read())[2], 3);
}

// Widening into a scalar T is fine; narrowing stays ill-formed, so
// SharedResource<int>{2.5} does not compile.
static_assert(std::is_constructible_v<SharedResource<int>, int>);
static_assert(!std::is_constructible_v<SharedResource<int>, double>);

//
// Locking behaviour under real contention.
//

TEST(SharedResourceTest, WithWriteLockSerializesConcurrentWriters) {
    SharedResource<long long> counter(0LL);

    std::vector<std::jthread> writers;
    writers.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i) {
        writers.emplace_back([&counter] {
            for (std::size_t n = 0; n < kIncrements; ++n) {
                counter.with_write_lock([](long long& value) { ++value; });
            }
        });
    }
    writers.clear();  // jthread dtors join

    EXPECT_EQ(*counter.read(), static_cast<long long>(kThreads * kIncrements));
}

// The reason this class exists at all: readers share the lock rather than
// queueing behind one another.
TEST(SharedResourceTest, ReadProxiesAreHeldConcurrently) {
    SharedResource<long long> resource(7LL);

    EXPECT_EQ(concurrent_read_holders(resource), kThreads) << "read() did not grant shared access";
}

//
// Proxy and callable accessors.
//

TEST(SharedResourceTest, WriteProxyMutatesThroughArrow) {
    SharedResource<std::vector<int>> resource(0u);  // empty vector

    resource->push_back(11);
    resource->push_back(22);

    ASSERT_EQ(resource.read()->size(), 2u);
    EXPECT_EQ(resource.read()->back(), 22);
}

TEST(SharedResourceTest, WithReadLockReturnsCallableResult) {
    const SharedResource<std::vector<int>> resource{4, 5, 6};

    const int sum = resource.with_read_lock(
        [](const std::vector<int>& values) { return values[0] + values[1] + values[2]; });

    EXPECT_EQ(sum, 15);
}
