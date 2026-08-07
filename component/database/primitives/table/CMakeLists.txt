##############################################################################
# Core Database Table
##############################################################################
add_library(${DATABASE}.Primitives.Table STATIC
        db_table.hpp
        db_dynamic_table.cpp
        db_static_table.hpp
        db_dynamic_table.hpp
        detail/db_dynamic_table.inl
)

target_include_directories(${DATABASE}.Primitives.Table PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DATABASE}.Primitives.Table
        PUBLIC
        ${DATABASE}.Features
        ${DATABASE}.Primitives.Column
        ${DATABASE}.Primitives.Field
)
##############################################################################