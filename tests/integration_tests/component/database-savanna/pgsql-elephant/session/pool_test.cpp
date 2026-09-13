// PostgreSQL Pool Integration Tests
// Tests connection pool behavior through Session: multi-executor, exhaustion,
// shutdown, stats, concurrency, and connection cleanup on release.

#include <thread>
#include <tuple>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <postgres_session.hpp>

using namespace menagerie::savanna::elephant;
using namespace menagerie::savanna;
using namespace std::chrono_literals;

// ============== Test Helpers ==============

static ConnectionConfig make_test_config() {
    auto credentials = ConnectionCredentials::Builder{}
                           .host(menagerie::beaver::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
                           .port(menagerie::beaver::value_or(std::getenv("POSTGRES_PORT"), "5433"))
                           .dbname(menagerie::beaver::value_or(std::getenv("POSTGRES_DB"), "test_db"))
                           .user(menagerie::beaver::value_or(std::getenv("POSTGRES_USER"), "test_user"))
                           .password(menagerie::beaver::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"))
                           .finalize();
    return ConnectionConfig::Builder{}.credentials(std::move(credentials)).ssl_mode(SslMode::DISABLE).finalize();
}

static PoolConfig make_pool_config(std::size_t cap, std::size_t min) {
    return PoolConfig::Builder{}.capacity(cap).min_connections(min).finalize();
}

// ============== Test Fixture ==============

class PoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());

        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "PostgreSQL unavailable: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD";
        }
        PQfinish(probe);
    }
};

// ============== MultipleSyncExecutors ==============

TEST_F(PoolTest, MultipleSyncExecutorsRunQueriesAndRelease) {
    Session session{make_test_config(), make_pool_config(4, 2)};

    // Acquire 3 executors simultaneously from a capacity-4 pool
    {
        auto exec1 = session.with_sync().value();
        auto exec2 = session.with_sync().value();
        auto exec3 = session.with_sync().value();

        ASSERT_TRUE(exec1.valid());
        ASSERT_TRUE(exec2.valid());
        ASSERT_TRUE(exec3.valid());

        // Each executor runs an independent query on its own connection
        auto r1 = exec1.execute("SELECT 1 AS n");
        auto r2 = exec2.execute("SELECT 2 AS n");
        auto r3 = exec3.execute("SELECT 3 AS n");

        ASSERT_TRUE(r1.has_value()) << r1.error().format();
        ASSERT_TRUE(r2.has_value()) << r2.error().format();
        ASSERT_TRUE(r3.has_value()) << r3.error().format();

        EXPECT_EQ(r1.value().get<int>(0, 0), 1);
        EXPECT_EQ(r2.value().get<int>(0, 0), 2);
        EXPECT_EQ(r3.value().get<int>(0, 0), 3);
    }
    // All 3 executors destroyed — slots returned

    // Session is still usable after releasing all executors
    auto exec = session.with_sync().value();
    ASSERT_TRUE(exec.valid());
    auto result = exec.execute("SELECT 42 AS answer");
    ASSERT_TRUE(result.has_value()) << result.error().format();
    EXPECT_EQ(result.value().get<int>(0, 0), 42);

    session.shutdown();
}

// ============== PoolExhaustion ==============

TEST_F(PoolTest, PoolExhaustionFailsTryAcquire) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    // Exhaust the pool (capacity=2)
    auto exec1 = session.try_with_sync().value();
    auto exec2 = session.try_with_sync().value();
    ASSERT_TRUE(exec1.valid());
    ASSERT_TRUE(exec2.valid());

    // 3rd try-acquire should fail fast — pool exhausted
    auto result3 = session.try_with_sync();
    EXPECT_FALSE(result3.has_value());

    // Release one executor by moving it out of scope
    { [[maybe_unused]] auto released = std::move(exec1); }

    // Now acquire should succeed again
    auto exec4 = session.try_with_sync().value();
    EXPECT_TRUE(exec4.valid());

    auto result = exec4.execute("SELECT 1");
    ASSERT_TRUE(result.has_value()) << result.error().format();

    session.shutdown();
}

// ============== Shutdown ==============

TEST_F(PoolTest, ShutdownPreventsNewAcquisitions) {
    Session session{make_test_config(), make_pool_config(4, 1)};

    // Verify session works before shutdown
    {
        auto exec = session.with_sync().value();
        ASSERT_TRUE(exec.valid());
        auto result = exec.execute("SELECT 1");
        ASSERT_TRUE(result.has_value()) << result.error().format();
    }

    // Shutdown the session
    session.shutdown();
    EXPECT_TRUE(session.is_shutdown());

    // New acquisitions should fail after shutdown
    auto result = session.with_sync();
    EXPECT_FALSE(result.has_value());
}

TEST_F(PoolTest, DoubleShutdownIdempotent) {
    Session session{make_test_config(), make_pool_config(4, 1)};

    // First shutdown
    EXPECT_NO_THROW(session.shutdown());
    EXPECT_TRUE(session.is_shutdown());

    // Second shutdown — should not crash or throw
    EXPECT_NO_THROW(session.shutdown());
    EXPECT_TRUE(session.is_shutdown());
}

// ============== ExecutorLifecycleScope ==============

TEST_F(PoolTest, ExecutorLifecycleScopeReleasesSlot) {
    Session session{make_test_config(), make_pool_config(4, 1)};

    const auto free_before = session.pool_free_count();

    // Inner scope: acquire and use an executor
    {
        auto exec = session.with_sync().value();
        ASSERT_TRUE(exec.valid());

        auto result = exec.execute("SELECT pg_backend_pid()");
        ASSERT_TRUE(result.has_value()) << result.error().format();
    }
    // Executor destroyed — slot returned to pool

    // Session recovers: can acquire again and execute queries
    auto exec = session.with_sync().value();
    ASSERT_TRUE(exec.valid());
    auto result = exec.execute("SELECT 1");
    ASSERT_TRUE(result.has_value()) << result.error().format();

    // Free count should be restored (minus the one we just acquired)
    EXPECT_GE(session.pool_free_count() + 1, free_before);

    session.shutdown();
}

// ============== StatsAccuracy ==============

TEST_F(PoolTest, StatsAccuracy) {
    Session session{make_test_config(), make_pool_config(4, 2)};

    // Capacity is always 4
    EXPECT_EQ(session.pool_capacity(), 4u);

    // Before any acquisitions: active should be 0
    const auto active_before = session.pool_active_count();
    const auto free_before   = session.pool_free_count();
    EXPECT_EQ(active_before, 0u);
    EXPECT_GT(free_before, 0u);

    // Acquire two executors
    auto exec1 = session.with_sync().value();
    auto exec2 = session.with_sync().value();
    ASSERT_TRUE(exec1.valid());
    ASSERT_TRUE(exec2.valid());

    // During: active count should have increased by 2
    EXPECT_EQ(session.pool_active_count(), active_before + 2);

    // Free count should have decreased
    EXPECT_LT(session.pool_free_count(), free_before);

    // Release executors
    { [[maybe_unused]] auto released1 = std::move(exec1); }
    { [[maybe_unused]] auto released2 = std::move(exec2); }

    // After release: active count should be back to initial
    EXPECT_EQ(session.pool_active_count(), active_before);

    session.shutdown();
}

// ============== Query Surface ==============

TEST_F(PoolTest, WithSyncVariadicParameters) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    auto exec   = session.with_sync().value();
    auto create = exec.execute("CREATE TABLE IF NOT EXISTS pool_variadic_test (name VARCHAR(100), value INTEGER)");
    ASSERT_TRUE(create.has_value()) << create.error().format();
    ASSERT_TRUE(exec.execute("TRUNCATE pool_variadic_test").has_value());

    auto insert = exec.execute("INSERT INTO pool_variadic_test (name, value) VALUES ($1, $2)", std::string{"Bob"}, 99);
    ASSERT_TRUE(insert.has_value()) << insert.error().format();

    auto select = exec.execute("SELECT value FROM pool_variadic_test WHERE name = $1", std::string{"Bob"});
    ASSERT_TRUE(select.has_value());
    EXPECT_EQ(select.value().get<int>(0, 0), 99);

    std::ignore = exec.execute("DROP TABLE IF EXISTS pool_variadic_test");
}

TEST_F(PoolTest, WithSyncReturnsErrorOnBadQuery) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    auto exec   = session.with_sync().value();
    auto result = exec.execute("SELCT 1");  // typo

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().sqlstate.substr(0, 2), "42");
}

// ============== ConcurrentAsyncQueries ==============

TEST_F(PoolTest, ConcurrentAsyncQueriesComplete) {
    Session session{make_test_config(), make_pool_config(4, 2)};

    boost::asio::io_context ioc;

    constexpr int kQueries = 4;
    std::atomic<int> success_count{0};
    std::atomic<int> completion_count{0};

    for (int i = 0; i < kQueries; ++i) {
        boost::asio::co_spawn(
            ioc,
            [&session, &ioc, &success_count, &completion_count, i]() -> boost::asio::awaitable<void> {
                auto acquired = co_await session.with_async(ioc.get_executor(), 5s);
                if (acquired.has_value()) {
                    auto exec = std::move(acquired).value();
                    if (auto result = co_await exec.execute("SELECT $1::integer AS n", i);
                        result.has_value() && result.value().get<int>(0, 0) == i) {
                        ++success_count;
                    }
                }
                ++completion_count;
                co_return;
            },
            boost::asio::detached);
    }

    ioc.run();

    EXPECT_EQ(completion_count.load(), kQueries);
    EXPECT_EQ(success_count.load(), kQueries);

    session.shutdown();
}

TEST_F(PoolTest, AsyncExecutorReleasesConnectionAfterScope) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    boost::asio::io_context ioc;
    const auto free_before = session.pool_free_count();

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto exec   = session.try_with_async(ioc.get_executor()).value();
            std::ignore = co_await exec.execute("SELECT 1");
            co_return;
        },
        boost::asio::detached);
    ioc.run();

    // Connection must be returned after scope (slot reset by AsyncExecutor destructor)
    EXPECT_GE(session.pool_free_count(), free_before);

    session.shutdown();
}

// ============== ConcurrentSyncFromThreads ==============

TEST_F(PoolTest, ConcurrentSyncExecutorsFromMultipleThreads) {
    Session session{make_test_config(), make_pool_config(4, 2)};

    constexpr int kThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&session, &success_count, i] {
            auto outcome = session.with_sync();
            if (!outcome.has_value()) {
                return;
            }
            auto exec = std::move(outcome).value();
            if (auto result = exec.execute("SELECT $1::integer AS n", i);
                result.has_value() && result.value().get<int>(0, 0) == i) {
                ++success_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), kThreads);

    session.shutdown();
}

// ============== Cleanup on Release ==============

TEST_F(PoolTest, NoCleanupKeepsSessionState) {
    // Control for the two cleanup tests below: with neither do_cleanup() nor a
    // pool default, session-local state survives the release/reacquire cycle.
    Session session{make_test_config(), make_pool_config(1, 1)};

    {
        auto exec   = session.with_sync().value();
        auto result = exec.execute("CREATE TEMP TABLE _pool_marker (x INT)");
        ASSERT_TRUE(result.has_value()) << result.error().format();
    }
    // Capacity is 1, so the next borrow reuses the same connection — uncleaned.

    auto exec   = session.with_sync().value();
    auto result = exec.execute("SELECT * FROM _pool_marker");
    EXPECT_TRUE(result.has_value()) << "temp table must survive when no cleanup is configured";
}

TEST_F(PoolTest, ExplicitDoCleanupDiscardsSessionState) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    // Set session-local state on the only connection, with an explicit DISCARD ALL on release.
    {
        auto exec   = session.with_sync().value().do_cleanup(CleanupQuery::DiscardAll);
        auto result = exec.execute("CREATE TEMP TABLE _pool_marker (x INT)");
        ASSERT_TRUE(result.has_value()) << result.error().format();
    }
    // Capacity is 1, so the next borrow reuses the same connection — cleaned.

    auto exec   = session.with_sync().value();
    auto result = exec.execute("SELECT * FROM _pool_marker");
    EXPECT_FALSE(result.has_value()) << "temp table must be gone after explicit DISCARD ALL";
}

TEST_F(PoolTest, PoolDefaultCleanupRunsWhenBorrowerSetNone) {
    // The pool-wide default cleanup (PoolConfig::cleanup_sql) applies when the
    // borrower never called do_cleanup().
    auto pool_cfg = PoolConfig::Builder{}.capacity(1).min_connections(1).cleanup_sql("DISCARD ALL").finalize();
    Session session{make_test_config(), std::move(pool_cfg)};

    {
        auto exec   = session.with_sync().value();  // no do_cleanup()
        auto result = exec.execute("CREATE TEMP TABLE _pool_default_marker (x INT)");
        ASSERT_TRUE(result.has_value()) << result.error().format();
    }
    // Capacity is 1, so the next borrow reuses the same connection — the pool
    // default DISCARD ALL must have wiped the temp table.

    auto exec   = session.with_sync().value();
    auto result = exec.execute("SELECT * FROM _pool_default_marker");
    EXPECT_FALSE(result.has_value()) << "temp table must be gone after pool-default DISCARD ALL";
}
