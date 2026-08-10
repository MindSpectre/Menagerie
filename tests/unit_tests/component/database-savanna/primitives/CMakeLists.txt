# Database Core Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.FieldSchema
        test_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.Table
        test_table_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.Column
        test_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.Field
        test_field.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.StaticTable
        test_static_table.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.StaticFieldSchema
        test_static_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Savanna.Primitives.StaticColumn
        test_static_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Menagerie.Component.Savanna.Primitives
)