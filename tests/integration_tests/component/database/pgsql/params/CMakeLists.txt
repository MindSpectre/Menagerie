##############################################################################
# PostgreSQL Params Functional Tests
##############################################################################

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Params
        postgres_params_test.cpp
        LINK_LIBS
        Menagerie::Component::Database::PostgreSQL
        ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
