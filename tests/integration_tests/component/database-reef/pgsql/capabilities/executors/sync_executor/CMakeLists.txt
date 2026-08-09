##############################################################################
# PostgreSQL SyncExecutor Functional Tests
##############################################################################

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Capabilities.Executors.Executor.Sync
        postgres_sync_executor_test.cpp
        LINK_LIBS
        Menagerie::Component::Reef::PostgreSQL
        ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
