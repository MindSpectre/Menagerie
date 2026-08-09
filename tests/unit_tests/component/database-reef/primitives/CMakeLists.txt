# Database Core Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.FieldSchema
        test_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.Table
        test_table_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.Column
        test_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.Field
        test_field.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.StaticTable
        test_static_table.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.StaticFieldSchema
        test_static_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Reef.Primitives.StaticColumn
        test_static_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Reef.Primitives
)