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

    /// Minimal `T` with a member to call, so `operator->` has something to reach.
    struct Counter {
        long long value = 0;

        void bump() {
            ++value;
        }
    };
}  // namespace

//
// Construction semantics -- the distinction introduced in #39.
//

// The size literal is unsigned on purpose: the build runs -Wconversion -Werror,
// so a plain `5` here is a sign-conversion error against vector's size_type.
TEST(SynchronizedResourceTest, ParenthesizedFormForwardsToElementConstructor) {
    SynchronizedResource<std::vector<int>> resource(5u);
    EXPECT_EQ(resource.lock()->size(), 5u);
}

TEST(SynchronizedResourceTest, BracedFormUsesInitializerList) {
    SynchronizedResource<std::vector<int>> resource{5};
    ASSERT_EQ(resource.lock()->size(), 1u);
    EXPECT_EQ((*resource.lock())[0], 5);
}

TEST(SynchronizedResourceTest, BracedFormAcceptsMultipleElements) {
    SynchronizedResource<std::vector<int>> resource{1, 2, 3};
    ASSERT_EQ(resource.lock()->size(), 3u);
    EXPECT_EQ((*resource.lock())[2], 3);
}

// Widening into a scalar T is fine; narrowing stays ill-formed, so
// SynchronizedResource<int>{2.5} does not compile.
static_assert(std::is_constructible_v<SynchronizedResource<int>, int>);
static_assert(!std::is_constructible_v<SynchronizedResource<int>, double>);

//
// Locking behaviour under real contention.
//

TEST(SynchronizedResourceTest, InLockSerializesConcurrentWriters) {
    SynchronizedResource<long long> counter(0LL);

    std::vector<std::jthread> writers;
    writers.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i) {
        writers.emplace_back([&counter] {
            for (std::size_t n = 0; n < kIncrements; ++n) {
                counter.in_lock([](long long& value) { ++value; });
            }
        });
    }
    writers.clear();  // jthread dtors join

    EXPECT_EQ(*counter.lock(), static_cast<long long>(kThreads * kIncrements));
}

TEST(SynchronizedResourceTest, ArrowSerializesConcurrentWriters) {
    SynchronizedResource<Counter> counter{};

    std::vector<std::jthread> writers;
    writers.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i) {
        writers.emplace_back([&counter] {
            for (std::size_t n = 0; n < kIncrements; ++n) {
                counter->bump();  // the proxy holds the lock for this statement only
            }
        });
    }
    writers.clear();  // jthread dtors join

    EXPECT_EQ(counter.lock()->value, static_cast<long long>(kThreads * kIncrements));
}

//
// Proxy and callable accessors.
//

TEST(SynchronizedResourceTest, ProxyMutatesThroughArrow) {
    SynchronizedResource<std::vector<int>> resource(0u);  // empty vector

    resource->push_back(11);
    resource->push_back(22);

    ASSERT_EQ(resource.lock()->size(), 2u);
    EXPECT_EQ(resource.lock()->back(), 22);
}

TEST(SynchronizedResourceTest, InLockReturnsCallableResult) {
    SynchronizedResource<std::vector<int>> resource{4, 5, 6};

    const int sum = resource.in_lock(
        [](std::vector<int>& values) { return values[0] + values[1] + values[2]; });

    EXPECT_EQ(sum, 15);
}

// A const instance locks the same mutable mutex and hands out a const view.
TEST(SynchronizedResourceTest, ConstInstanceReadsThroughInLockAndArrow) {
    const SynchronizedResource<std::vector<int>> resource{4, 5, 6};

    const int sum = resource.in_lock(
        [](const std::vector<int>& values) { return values[0] + values[1] + values[2]; });

    EXPECT_EQ(sum, 15);
    EXPECT_EQ(resource->size(), 3u);
    EXPECT_EQ((*resource.lock())[2], 6);
}

static_assert(std::is_same_v<decltype(std::declval<SynchronizedResource<int>&>().lock().operator->()),
                             int*>);
static_assert(
    std::is_same_v<decltype(std::declval<const SynchronizedResource<int>&>().lock().operator->()),
                   const int*>);

// The const view rides on the handle's type, not on its cv-qualification: a
// `const`-qualified handle over a plain `T&` would decay under `auto p =
// res.lock()`, which drops top-level const from the prvalue and would hand back
// a mutable view of a const resource.
static_assert(std::is_same_v<decltype(std::declval<const SynchronizedResource<int>&>().lock()),
                             SynchronizedResource<int>::ConstProxy>);
static_assert(std::is_same_v<decltype(std::declval<SynchronizedResource<int>&>().lock()),
                             SynchronizedResource<int>::Proxy>);
