# Database Base Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Reef.Base.TypeMapping
        test_type_mapping.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Reef.Primitives
        Menagerie.Component.Reef.Providers.PostgreSQL.Dialect
        Menagerie.Component.Reef.Providers.PostgreSQL.TypeMapping
)
