##############################################################################
# PostgreSQL Session Functional Tests
##############################################################################

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Session
        lock_free_session_test.cpp
        LINK_LIBS
        Menagerie::Component::Reef::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Pool
        pool_test.cpp
        LINK_LIBS
        Menagerie::Component::Reef::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.BlockingSession
        blocking_session_test.cpp
        LINK_LIBS
        Menagerie.Component.Reef.Providers.PostgreSQL.Session.Blocking
        Menagerie::Component::Reef::PostgreSQL
        ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
