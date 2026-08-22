// PostgreSQL Session Functional Tests
// Tests Session, ConnectionPool, QueuedHolder, FIFO waiter semantics, and the
// no-DB units that ship with the session: PoolConfig and the Connection FSM.

#include <atomic>
#include <chrono>
#include <mutex>
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

// Helper to run an async coroutine to completion in tests
template <typename CoroFunc>
auto run_async(boost::asio::io_context& io, CoroFunc&& func) {
    using awaitable_type = std::invoke_result_t<CoroFunc>;
    using result_type    = awaitable_type::value_type;

    std::optional<result_type> result;
    std::exception_ptr eptr;

    boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            try {
                result = co_await func();
            } catch (...) {
                eptr = std::current_exception();
            }
        },
        boost::asio::detached);

    io.run();
    io.restart();

    if (eptr) {
        std::rethrow_exception(eptr);
    }

    return result;
}

// ============== PoolConfig Unit Tests (no DB required) ==============

class PoolConfigTest : public ::testing::Test {};

TEST_F(PoolConfigTest, ValidateAcceptsValidConfig) {
    const auto cfg = PoolConfig::Builder{}.capacity(16).min_connections(2).finalize();
    EXPECT_NO_THROW(cfg.validate());
}

TEST_F(PoolConfigTest, ValidateRejectsZeroCapacity) {
    EXPECT_THROW((void)PoolConfig::Builder{}.capacity(0).finalize(), std::invalid_argument);
}

TEST_F(PoolConfigTest, ValidateAcceptsNonPowerOfTwoCapacity) {
    // The power-of-two constraint died with the ring-buffer pool.
    EXPECT_NO_THROW((void)PoolConfig::Builder{}.capacity(10).min_connections(2).finalize());
}

TEST_F(PoolConfigTest, ValidateRejectsMinConnectionsExceedCapacity) {
    EXPECT_THROW((void)PoolConfig::Builder{}.capacity(4).min_connections(8).finalize(), std::invalid_argument);
}

TEST_F(PoolConfigTest, FactoryMethodsProduceValidConfigs) {
    EXPECT_NO_THROW(PoolConfig::minimal().validate());
    EXPECT_NO_THROW(PoolConfig::standard().validate());
    EXPECT_NO_THROW(PoolConfig::high_performance().validate());
}

TEST_F(PoolConfigTest, MinimalConfigHasSmallCapacity) {
    const auto cfg = PoolConfig::minimal();
    EXPECT_EQ(cfg.capacity(), 2u);
    EXPECT_EQ(cfg.min_connections(), 1u);
}

TEST_F(PoolConfigTest, HighPerformanceConfigHasLargeCapacity) {
    const auto cfg = PoolConfig::high_performance();
    EXPECT_GE(cfg.capacity(), 32u);
}

// ============== Connection FSM Unit Tests (no DB required) ==============

class ConnectionFsmTest : public ::testing::Test {};

TEST_F(ConnectionFsmTest, DefaultConstructedIsDisconnected) {
    const Connection conn;
    EXPECT_EQ(conn.state(), ConnectionState::DISCONNECTED);
    EXPECT_FALSE(conn.ready());
    EXPECT_EQ(conn.native_handle(), nullptr);
}

TEST_F(ConnectionFsmTest, OpenFailureStaysDisconnected) {
    // Unroutable host with an immediate-failure conninfo: PQconnectdb returns a
    // handle whose status is not CONNECTION_OK, and open() must close it.
    auto conn = Connection::open("host=invalid.invalid port=1 connect_timeout=1");
    EXPECT_EQ(conn.state(), ConnectionState::DISCONNECTED);
    EXPECT_EQ(conn.native_handle(), nullptr);
}

TEST_F(ConnectionFsmTest, GuardedOperationsFailOnDisconnected) {
    Connection conn;
    EXPECT_FALSE(conn.verify());
    EXPECT_FALSE(conn.run_cleanup("DISCARD ALL"));
    EXPECT_EQ(conn.state(), ConnectionState::DISCONNECTED) << "guarded ops must not transition";
}

TEST_F(ConnectionFsmTest, CloseIsIdempotent) {
    Connection conn;
    conn.close();
    conn.close();
    EXPECT_EQ(conn.state(), ConnectionState::DISCONNECTED);
}

TEST_F(ConnectionFsmTest, MoveTransfersOwnershipAndState) {
    Connection a;
    Connection b{std::move(a)};
    EXPECT_EQ(b.state(), ConnectionState::DISCONNECTED);
    // NOLINTNEXTLINE(bugprone-use-after-move) - post-move state is the point under test
    EXPECT_EQ(a.state(), ConnectionState::DISCONNECTED);
    EXPECT_EQ(a.native_handle(), nullptr);
}

TEST_F(ConnectionFsmTest, StateNamesAreStable) {
    EXPECT_STREQ(to_string(ConnectionState::DISCONNECTED), "DISCONNECTED");
    EXPECT_STREQ(to_string(ConnectionState::READY), "READY");
    EXPECT_STREQ(to_string(ConnectionState::BROKEN), "BROKEN");
}

// ============== Session Fixture ==============

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());
        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
        }
        PQfinish(probe);
    }

    boost::asio::io_context io_;
};

// ============== Basic Execution ==============

TEST_F(SessionTest, TryWithSyncExecutesSimpleQuery) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    auto outcome = session.try_with_sync();
    ASSERT_TRUE(outcome.has_value());

    auto exec   = std::move(outcome).value();
    auto result = exec.execute("SELECT 42 AS answer");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().rows(), 1u);
}

TEST_F(SessionTest, WithSyncTimedExecutesQueryOnFreePool) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    auto outcome = session.with_sync(500ms);
    ASSERT_TRUE(outcome.has_value());
    auto exec = std::move(outcome).value();
    EXPECT_TRUE(exec.execute("SELECT 1").has_value());
}

TEST_F(SessionTest, WithSyncBlockingExecutesQueryOnFreePool) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    auto outcome = session.with_sync();
    ASSERT_TRUE(outcome.has_value());
    auto exec = std::move(outcome).value();
    EXPECT_TRUE(exec.execute("SELECT 1").has_value());
}

// ============== Exhaustion + Error Codes ==============

TEST_F(SessionTest, TryWithSyncReturnsPoolExhaustedWhenFull) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    auto second = session.try_with_sync();
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code.value(), static_cast<int>(ClientErrorCode::PoolExhausted));
}

TEST_F(SessionTest, WithSyncTimedReturnsWaitTimeoutOnExpiry) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    const auto start   = std::chrono::steady_clock::now();
    auto second        = session.with_sync(120ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code.value(), static_cast<int>(ClientErrorCode::WaitTimeout));
    EXPECT_GE(elapsed, 110ms);
    EXPECT_LT(elapsed, 500ms);
}

TEST_F(SessionTest, WithSyncZeroTimeoutReturnsImmediately) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    const auto start   = std::chrono::steady_clock::now();
    const auto result  = session.with_sync(std::chrono::milliseconds::zero());
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    EXPECT_LT(elapsed, 20ms) << "zero timeout must not sleep";
}

TEST_F(SessionTest, WithAsyncTimedReturnsWaitTimeoutWhenExhausted) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    auto result = run_async(io_, [&]() -> boost::asio::awaitable<std::expected<AsyncExecutor, ErrorContext>> {
        co_return co_await session.with_async(io_.get_executor(), 100ms);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->has_value());
    EXPECT_EQ(result->error().code.value(), static_cast<int>(ClientErrorCode::WaitTimeout));
}

// ============== Handoff on Release ==============

TEST_F(SessionTest, WithSyncTimedAcquiresSlotReleasedDuringWait) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    auto holder_wrapper = std::make_unique<SyncExecutor>(std::move(first).value());

    std::thread releaser{[&] {
        std::this_thread::sleep_for(80ms);
        holder_wrapper.reset();  // dtor returns the slot
    }};

    const auto start = std::chrono::steady_clock::now();
    auto waited      = session.with_sync(2s);
    const auto took  = std::chrono::steady_clock::now() - start;

    releaser.join();

    ASSERT_TRUE(waited.has_value());
    EXPECT_GE(took, 70ms);
    EXPECT_LT(took, 500ms) << "handoff should be near-instant after release";
}

TEST_F(SessionTest, WithSyncBlockingAcquiresSlotReleasedDuringWait) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first          = session.try_with_sync();
    auto holder_wrapper = std::make_unique<SyncExecutor>(std::move(first).value());

    std::thread releaser{[&] {
        std::this_thread::sleep_for(80ms);
        holder_wrapper.reset();
    }};

    auto waited = session.with_sync();  // unbounded
    releaser.join();

    ASSERT_TRUE(waited.has_value());
}

TEST_F(SessionTest, WithAsyncAcquiresSlotReleasedDuringWait) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());
    auto holder_wrapper = std::make_unique<SyncExecutor>(std::move(first).value());

    std::thread releaser{[&] {
        // Wait until the coroutine has enqueued, then free the slot.
        while (session.pool_waiter_count() == 0) {
            std::this_thread::sleep_for(5ms);
        }
        holder_wrapper.reset();
    }};

    auto result = run_async(io_, [&]() -> boost::asio::awaitable<std::expected<AsyncExecutor, ErrorContext>> {
        co_return co_await session.with_async(io_.get_executor(), 5s);
    });

    releaser.join();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value()) << "coroutine waiter must receive the released slot";
}

// ============== Shutdown Semantics ==============

TEST_F(SessionTest, ShutdownWakesTimedWaiter) {
    Session session{make_test_config(), make_pool_config(1, 1)};
    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    std::atomic<bool> waiter_observed_shutdown{false};
    std::thread waiter{[&] {
        auto result = session.with_sync(5s);
        waiter_observed_shutdown =
            !result.has_value() && result.error().code.value() == static_cast<int>(ClientErrorCode::PoolShutdown);
    }};

    // Wait until the waiter has enqueued
    while (session.pool_waiter_count() == 0) {
        std::this_thread::sleep_for(5ms);
    }

    session.shutdown();
    waiter.join();
    EXPECT_TRUE(waiter_observed_shutdown.load());
}

TEST_F(SessionTest, ShutdownWakesBlockingWaiter) {
    Session session{make_test_config(), make_pool_config(1, 1)};
    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    std::atomic<bool> waiter_observed_shutdown{false};
    std::thread waiter{[&] {
        auto result = session.with_sync();  // unbounded
        waiter_observed_shutdown =
            !result.has_value() && result.error().code.value() == static_cast<int>(ClientErrorCode::PoolShutdown);
    }};

    while (session.pool_waiter_count() == 0) {
        std::this_thread::sleep_for(5ms);
    }

    session.shutdown();
    waiter.join();
    EXPECT_TRUE(waiter_observed_shutdown.load());
}

// ============== FIFO Fairness ==============

TEST_F(SessionTest, FifoFairnessUnderContention) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto initial_outcome = session.try_with_sync();
    ASSERT_TRUE(initial_outcome.has_value());
    auto initial = std::make_unique<SyncExecutor>(std::move(initial_outcome).value());

    constexpr std::size_t N = 6;
    std::vector<std::thread> workers;
    std::vector<std::size_t> acquire_order;
    std::mutex order_mtx;

    // Launch workers one at a time, waiting for each to enqueue before starting the next.
    // This makes enqueue order deterministic — worker i is at waiters_[i].
    for (std::size_t i = 0; i < N; ++i) {
        workers.emplace_back([&, i] {
            if (const auto w = session.with_sync(10s); w.has_value()) {
                std::lock_guard lk{order_mtx};
                acquire_order.push_back(i);
                // Hold briefly so subsequent waiters don't race past us
                std::this_thread::sleep_for(30ms);
            }
        });

        // Wait until this worker has enqueued before launching the next
        while (session.pool_waiter_count() != i + 1) {
            std::this_thread::sleep_for(2ms);
        }
    }

    // All N workers are enqueued. Release the initial holder — the first worker wakes.
    initial.reset();

    for (auto& t : workers)
        t.join();

    ASSERT_EQ(acquire_order.size(), static_cast<std::size_t>(N));
    for (std::size_t i = 0; i < static_cast<std::size_t>(N); ++i) {
        EXPECT_EQ(acquire_order[i], static_cast<int>(i))
            << "FIFO order violated at position " << i << " (got worker " << acquire_order[i] << ")";
    }
}

// ============== Transactions ==============

TEST_F(SessionTest, BeginTransactionRollsBackOnScopeExit) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    // Setup: create a test table in an auto-committed statement
    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.has_value());
        auto exec = std::move(exec_outcome).value();
        ASSERT_TRUE(exec.execute("CREATE TABLE IF NOT EXISTS session_tx_test (id INT)").has_value());
        ASSERT_TRUE(exec.execute("TRUNCATE session_tx_test").has_value());
    }

    // Insert inside a transaction and roll back
    {
        auto tx_outcome = session.try_begin_transaction();
        ASSERT_TRUE(tx_outcome.has_value());
        auto tx = std::move(tx_outcome).value();
        ASSERT_TRUE(tx.begin().has_value());
        {
            auto exec_outcome = tx.with_sync();
            ASSERT_TRUE(exec_outcome.has_value());
            auto exec = std::move(exec_outcome).value();
            ASSERT_TRUE(exec.execute("INSERT INTO session_tx_test VALUES (1)").has_value());
        }
        ASSERT_TRUE(tx.rollback().has_value());
    }

    // Verify the row is gone
    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.has_value());
        auto exec   = std::move(exec_outcome).value();
        auto result = exec.execute("SELECT COUNT(*) FROM session_tx_test");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().rows(), 1u);
    }

    // Cleanup
    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.has_value());
        auto exec   = std::move(exec_outcome).value();
        std::ignore = exec.execute("DROP TABLE IF EXISTS session_tx_test");
    }
}

TEST_F(SessionTest, TryBeginAutoTransactionAutoCommits) {
    Session session{make_test_config(), make_pool_config(2, 1)};

    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.has_value());
        auto exec = std::move(exec_outcome).value();
        ASSERT_TRUE(exec.execute("CREATE TABLE IF NOT EXISTS session_autotx_test (id INT)").has_value());
        ASSERT_TRUE(exec.execute("TRUNCATE session_autotx_test").has_value());
    }

    {
        auto tx_outcome = session.try_begin_auto_transaction();
        ASSERT_TRUE(tx_outcome.has_value());
        auto tx = std::move(tx_outcome).value();
        {
            auto exec_outcome = tx.with_sync();
            ASSERT_TRUE(exec_outcome.has_value());
            auto exec = std::move(exec_outcome).value();
            ASSERT_TRUE(exec.execute("INSERT INTO session_autotx_test VALUES (7)").has_value());
        }
        ASSERT_TRUE(tx.commit().has_value());
    }

    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.has_value());
        auto exec   = std::move(exec_outcome).value();
        auto result = exec.execute("SELECT id FROM session_autotx_test");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().rows(), 1u);

        std::ignore = exec.execute("DROP TABLE IF EXISTS session_autotx_test");
    }
}

TEST_F(SessionTest, BeginTransactionTimedReturnsWaitTimeoutWhenExhausted) {
    Session session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.has_value());

    const auto result = session.begin_transaction(TransactionOptions{}, 100ms);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code.value(), static_cast<int>(ClientErrorCode::WaitTimeout));
}

// ============== Stats ==============

TEST_F(SessionTest, StatsReflectFreeAndActiveHolders) {
    Session session{make_test_config(), make_pool_config(4, 2)};

    EXPECT_EQ(session.pool_capacity(), 4u);
    EXPECT_EQ(session.pool_free_count(), 2u);
    EXPECT_EQ(session.pool_active_count(), 0u);

    auto a = session.try_with_sync();
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(session.pool_active_count(), 1u);
    EXPECT_EQ(session.pool_free_count(), 1u);

    auto b = session.try_with_sync();
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(session.pool_active_count(), 2u);
    EXPECT_EQ(session.pool_free_count(), 0u);

    // Lazy init creates a 3rd
    auto c = session.try_with_sync();
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(session.pool_active_count(), 3u);
    EXPECT_EQ(session.pool_free_count(), 0u);
}
