##############################################################################
# Core Database Table
##############################################################################
add_library(${SAVANNA}.Primitives.Table STATIC
        db_table.hpp
        db_dynamic_table.cpp
        db_static_table.hpp
        db_dynamic_table.hpp
        detail/db_dynamic_table.inl
)

target_include_directories(${SAVANNA}.Primitives.Table PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SAVANNA}.Primitives.Table
        PUBLIC
        ${SAVANNA}.Features
        ${SAVANNA}.Primitives.Column
        ${SAVANNA}.Primitives.Field
)
##############################################################################