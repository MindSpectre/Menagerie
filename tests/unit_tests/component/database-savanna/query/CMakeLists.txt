# Database Query Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Query.StaticTableQuery
        test_static_table_query.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Savanna.Primitives
        Menagerie.Component.Savanna.Query.Compiler
        Menagerie.Component.Savanna.Providers.Elephant.Dialect
        Menagerie.Component.Savanna.Providers.Elephant.TypeMapping
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Query.QueryCompilation
        test_query_compilation.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Savanna.Primitives
        Menagerie.Component.Savanna.Query.Compiler
        Menagerie.Component.Savanna.Providers.Elephant.Dialect
        Menagerie.Component.Savanna.Providers.Elephant.TypeMapping
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Query.InsertExpression
        test_insert_expression.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Savanna.Primitives
        Menagerie.Component.Savanna.Query.Expressions
)
