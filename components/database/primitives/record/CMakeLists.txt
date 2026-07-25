##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${DATABASE}.Primitives.Record STATIC
        db_record.hpp
        db_record_factory.hpp
        db_record.cpp
)
target_include_directories(${DATABASE}.Primitives.Record PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DATABASE}.Primitives.Record
        PRIVATE
        ${DATABASE}.Primitives.Field
        ${DATABASE}.Primitives.Table
)
##############################################################################