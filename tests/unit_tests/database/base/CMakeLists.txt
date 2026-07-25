# Database Base Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.Base.TypeMapping
        test_type_mapping.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Database.Primitives
        Menagerie.Component.Database.Providers.PostgreSQL.Dialect
        Menagerie.Component.Database.Providers.PostgreSQL.TypeMapping
)
