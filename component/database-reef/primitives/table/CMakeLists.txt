##############################################################################
# Core Database Table
##############################################################################
add_library(${REEF}.Primitives.Table STATIC
        db_table.hpp
        db_dynamic_table.cpp
        db_static_table.hpp
        db_dynamic_table.hpp
        detail/db_dynamic_table.inl
)

target_include_directories(${REEF}.Primitives.Table PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${REEF}.Primitives.Table
        PUBLIC
        ${REEF}.Features
        ${REEF}.Primitives.Column
        ${REEF}.Primitives.Field
)
##############################################################################