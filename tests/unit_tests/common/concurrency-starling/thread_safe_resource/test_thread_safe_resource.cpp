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
}  // namespace

//
// Construction semantics -- the distinction introduced in #39.
//

// The size literal is unsigned on purpose: the build runs -Wconversion -Werror,
// so a plain `5` here is a sign-conversion error against vector's size_type.
TEST(ThreadSafeResourceTest, ParenthesizedFormForwardsToElementConstructor) {
    ThreadSafeResource<std::vector<int>> resource(5u);
    EXPECT_EQ(resource.read()->size(), 5u);
}

TEST(ThreadSafeResourceTest, BracedFormUsesInitializerList) {
    ThreadSafeResource<std::vector<int>> resource{5};
    ASSERT_EQ(resource.read()->size(), 1u);
    EXPECT_EQ((*resource.read())[0], 5);
}

TEST(ThreadSafeResourceTest, BracedFormAcceptsMultipleElements) {
    ThreadSafeResource<std::vector<int>> resource{1, 2, 3};
    ASSERT_EQ(resource.read()->size(), 3u);
    EXPECT_EQ((*resource.read())[2], 3);
}

// Widening into a scalar T is fine; narrowing stays ill-formed, so
// ThreadSafeResource<int>{2.5} does not compile.
static_assert(std::is_constructible_v<ThreadSafeResource<int>, int>);
static_assert(!std::is_constructible_v<ThreadSafeResource<int>, double>);

//
// Locking behaviour under real contention.
//

TEST(ThreadSafeResourceTest, WithLockSerializesConcurrentWriters) {
    ThreadSafeResource<long long> counter(0LL);

    std::vector<std::jthread> writers;
    writers.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i) {
        writers.emplace_back([&counter] {
            for (std::size_t n = 0; n < kIncrements; ++n) {
                counter.with_lock([](long long& value) { ++value; });
            }
        });
    }
    writers.clear();  // jthread dtors join

    EXPECT_EQ(*counter.read(), static_cast<long long>(kThreads * kIncrements));
}

TEST(ThreadSafeResourceTest, ReadProxiesAreHeldConcurrently) {
    ThreadSafeResource<long long> resource(7LL);

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

    // Bounded wait: if read() handed out an exclusive lock, only one reader could
    // ever be inside the region, so this expires rather than hanging the suite.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (holding.load(std::memory_order_acquire) < kThreads &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    const std::size_t observed = holding.load(std::memory_order_acquire);

    release.store(true, std::memory_order_release);  // must precede the join
    readers.clear();

    EXPECT_EQ(observed, kThreads) << "read() did not grant shared access";
}

//
// Proxy and callable accessors.
//

TEST(ThreadSafeResourceTest, WriteProxyMutatesThroughArrow) {
    ThreadSafeResource<std::vector<int>> resource(0u);  // empty vector

    resource->push_back(11);
    resource->push_back(22);

    ASSERT_EQ(resource.read()->size(), 2u);
    EXPECT_EQ(resource.read()->back(), 22);
}

TEST(ThreadSafeResourceTest, WithReadLockReturnsCallableResult) {
    const ThreadSafeResource<std::vector<int>> resource{4, 5, 6};

    const int sum = resource.with_read_lock(
        [](const std::vector<int>& values) { return values[0] + values[1] + values[2]; });

    EXPECT_EQ(sum, 15);
}
