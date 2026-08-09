# Database Base Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Base.TypeMapping
        test_type_mapping.cpp
        LINK_LIBS
        ${TEST_LIBS}

        Menagerie.Component.Savanna.Primitives
        Menagerie.Component.Savanna.Providers.Elephant.Dialect
        Menagerie.Component.Savanna.Providers.Elephant.TypeMapping
)
