# Database Query Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.Query.StaticTableQuery
        test_static_table_query.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Database.Primitives
        Menagerie.Component.Database.Query.Compiler
        Menagerie.Component.Database.Providers.PostgreSQL.Dialect
        Menagerie.Component.Database.Providers.PostgreSQL.TypeMapping
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Query.QueryCompilation
        test_query_compilation.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Database.Primitives
        Menagerie.Component.Database.Query.Compiler
        Menagerie.Component.Database.Providers.PostgreSQL.Dialect
        Menagerie.Component.Database.Providers.PostgreSQL.TypeMapping
)
