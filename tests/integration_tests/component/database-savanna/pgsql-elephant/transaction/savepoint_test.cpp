// PostgreSQL Savepoint Integration Tests
// Tests savepoint creation, rollback, release, nesting, and error handling

#include <base/transaction.hpp>
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <postgres_session.hpp>
#include <savepoint/savepoint.hpp>

using namespace menagerie::savanna::elephant;
using namespace menagerie::savanna;
using namespace std::chrono_literals;

// TODO: Extract make_test_config() into a shared test utility — duplicated in transaction_lifecycle_test.cpp.

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

// ============== Savepoint Tests ==============

class SavepointTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());

        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "PostgreSQL unavailable: " << error;
        }
        PQfinish(probe);

        session_ = std::make_unique<LockFreeSession>(
            make_test_config(),
            PoolConfig::Builder{}.capacity(4).min_connections(1).health_check_interval(2s).finalize());

        // Create test table
        auto exec   = session_->with_sync().value();
        auto result = exec.execute(R"(
            CREATE TABLE IF NOT EXISTS sp_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            )
        )");
        ASSERT_TRUE(result.has_value()) << "Setup failed: " << result.error().format();

        auto truncate = exec.execute("TRUNCATE TABLE sp_test RESTART IDENTITY CASCADE");
        ASSERT_TRUE(truncate.has_value()) << "Truncate failed: " << truncate.error().format();
    }

    void TearDown() override {
        if (session_) {
            const auto exec = session_->with_sync().value();
            std::ignore     = exec.execute("DROP TABLE IF EXISTS sp_test CASCADE");
            session_->shutdown();
        }
    }

    std::unique_ptr<LockFreeSession> session_;
};

TEST_F(SavepointTest, RollbackToSavepointUndoesWork) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().has_value());

    // Insert 'Before' before savepoint
    auto insert_before = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Before')");
    ASSERT_TRUE(insert_before.has_value()) << insert_before.error().format();

    // Create savepoint
    auto sp_result = tx.savepoint("sp1");
    ASSERT_TRUE(sp_result.has_value()) << sp_result.error().format();
    auto sp = std::move(sp_result.value());

    // Insert 'After' after savepoint
    auto insert_after = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('After')");
    ASSERT_TRUE(insert_after.has_value()) << insert_after.error().format();

    // Verify both rows visible within transaction
    auto count_before_rb = tx.with_sync().value().execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(count_before_rb.has_value()) << count_before_rb.error().format();
    EXPECT_EQ(count_before_rb.value().get<int>(0, 0), 2);

    // Rollback to savepoint -- undoes 'After'
    ASSERT_TRUE(sp.rollback().has_value());

    // Verify only 'Before' remains within transaction
    auto select = tx.with_sync().value().execute("SELECT name FROM sp_test");
    ASSERT_TRUE(select.has_value()) << select.error().format();
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "Before");

    // Commit and verify final state
    ASSERT_TRUE(tx.commit().has_value());

    auto exec        = session_->with_sync().value();
    auto final_check = exec.execute("SELECT name FROM sp_test");
    ASSERT_TRUE(final_check.has_value()) << final_check.error().format();
    EXPECT_EQ(final_check.value().rows(), 1);
    EXPECT_EQ(final_check.value().get<std::string>(0, 0), "Before");
}

TEST_F(SavepointTest, ReleaseSavepointKeepsWork) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().has_value());

    // Insert 'Keep'
    auto insert1 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Keep')");
    ASSERT_TRUE(insert1.has_value()) << insert1.error().format();

    // Create savepoint
    auto sp_result = tx.savepoint("sp_keep");
    ASSERT_TRUE(sp_result.has_value()) << sp_result.error().format();
    auto sp = std::move(sp_result.value());

    // Insert 'AlsoKeep'
    auto insert2 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('AlsoKeep')");
    ASSERT_TRUE(insert2.has_value()) << insert2.error().format();

    // Release savepoint -- keeps all work
    ASSERT_TRUE(sp.release().has_value());

    // Commit
    ASSERT_TRUE(tx.commit().has_value());

    // Verify both rows persisted
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(select.has_value()) << select.error().format();
    EXPECT_EQ(select.value().get<int>(0, 0), 2);
}

TEST_F(SavepointTest, NestedSavepoints) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().has_value());

    // Insert 'Base'
    auto insert_base = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Base')");
    ASSERT_TRUE(insert_base.has_value()) << insert_base.error().format();

    // Outer savepoint
    auto outer_result = tx.savepoint("outer");
    ASSERT_TRUE(outer_result.has_value()) << outer_result.error().format();
    auto outer = std::move(outer_result.value());

    // Insert 'Middle'
    auto insert_middle = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Middle')");
    ASSERT_TRUE(insert_middle.has_value()) << insert_middle.error().format();

    // Inner savepoint
    auto inner_result = tx.savepoint("inner");
    ASSERT_TRUE(inner_result.has_value()) << inner_result.error().format();
    auto inner = std::move(inner_result.value());

    // Insert 'Deep'
    auto insert_deep = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Deep')");
    ASSERT_TRUE(insert_deep.has_value()) << insert_deep.error().format();

    // Rollback inner -- undoes 'Deep'
    ASSERT_TRUE(inner.rollback().has_value());

    // Release outer -- keeps 'Base' and 'Middle'
    ASSERT_TRUE(outer.release().has_value());

    // Commit
    ASSERT_TRUE(tx.commit().has_value());

    // Verify 2 rows: Base and Middle
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM sp_test ORDER BY id");
    ASSERT_TRUE(select.has_value()) << select.error().format();
    EXPECT_EQ(select.value().rows(), 2);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "Base");
    EXPECT_EQ(select.value().get<std::string>(1, 0), "Middle");
}

TEST_F(SavepointTest, SavepointOnNonActiveTransactionReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    // Transaction is IDLE (begin() not called), savepoint should fail
    auto sp_result = tx.savepoint("should_fail");
    ASSERT_FALSE(sp_result.has_value());
    EXPECT_EQ(sp_result.error().code, ClientErrorCode::InvalidState);
}

TEST_F(SavepointTest, DoubleRollbackSucceeds) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().has_value());

    auto sp_result = tx.savepoint("sp_double");
    ASSERT_TRUE(sp_result.has_value()) << sp_result.error().format();
    auto sp = std::move(sp_result.value());

    // First rollback succeeds, savepoint stays active (PostgreSQL semantics)
    ASSERT_TRUE(sp.rollback().has_value());
    EXPECT_TRUE(sp.is_active());

    // Second rollback also succeeds
    ASSERT_TRUE(sp.rollback().has_value());
    EXPECT_TRUE(sp.is_active());

    // Cleanup: release the savepoint
    ASSERT_TRUE(sp.release().has_value());
    EXPECT_FALSE(sp.is_active());
}

TEST_F(SavepointTest, RollbackRetrySemantics) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().has_value());

    // Insert pre-savepoint data
    auto insert_base = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('base')");
    ASSERT_TRUE(insert_base.has_value()) << insert_base.error().format();

    // Create savepoint
    auto sp_result = tx.savepoint("sp_retry");
    ASSERT_TRUE(sp_result.has_value()) << sp_result.error().format();
    auto sp = std::move(sp_result.value());

    // First attempt: insert and rollback
    auto attempt1 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('attempt1')");
    ASSERT_TRUE(attempt1.has_value()) << attempt1.error().format();
    ASSERT_TRUE(sp.rollback().has_value());
    EXPECT_TRUE(sp.is_active());

    // Second attempt: insert and rollback again
    auto attempt2 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('attempt2')");
    ASSERT_TRUE(attempt2.has_value()) << attempt2.error().format();
    ASSERT_TRUE(sp.rollback().has_value());
    EXPECT_TRUE(sp.is_active());

    // Release savepoint and commit
    ASSERT_TRUE(sp.release().has_value());
    ASSERT_TRUE(tx.commit().has_value());

    // Only 'base' should remain — both attempts were rolled back
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM sp_test ORDER BY id");
    ASSERT_TRUE(select.has_value()) << select.error().format();
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "base");
}

TEST_F(SavepointTest, DestructorReleasesSavepointIfActive) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.has_value()) << tx_result.error().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().has_value());

    // Insert before savepoint
    auto insert1 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('before_scope')");
    ASSERT_TRUE(insert1.has_value()) << insert1.error().format();

    {
        // Create savepoint in inner scope
        auto sp_result = tx.savepoint("sp_scoped");
        ASSERT_TRUE(sp_result.has_value()) << sp_result.error().format();
        auto sp = std::move(sp_result.value());
        EXPECT_TRUE(sp.is_active());

        // sp destroyed here -- destructor should release the savepoint
    }

    // Insert after savepoint scope ended
    auto insert2 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('after_scope')");
    ASSERT_TRUE(insert2.has_value()) << insert2.error().format();

    // Commit
    ASSERT_TRUE(tx.commit().has_value());

    // Verify both rows persisted
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM sp_test ORDER BY id");
    ASSERT_TRUE(select.has_value()) << select.error().format();
    EXPECT_EQ(select.value().rows(), 2);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "before_scope");
    EXPECT_EQ(select.value().get<std::string>(1, 0), "after_scope");
}
