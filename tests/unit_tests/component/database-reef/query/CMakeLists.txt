# Database Query Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Reef.Query.StaticTableQuery
        test_static_table_query.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Reef.Primitives
        Menagerie.Component.Reef.Query.Compiler
        Menagerie.Component.Reef.Providers.PostgreSQL.Dialect
        Menagerie.Component.Reef.Providers.PostgreSQL.TypeMapping
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Query.QueryCompilation
        test_query_compilation.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Reef.Primitives
        Menagerie.Component.Reef.Query.Compiler
        Menagerie.Component.Reef.Providers.PostgreSQL.Dialect
        Menagerie.Component.Reef.Providers.PostgreSQL.TypeMapping
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Query.InsertExpression
        test_insert_expression.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Reef.Primitives
        Menagerie.Component.Reef.Query.Expressions
)
