##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${SAVANNA}.Primitives.Record STATIC
        db_record.hpp
        db_record_factory.hpp
        db_record.cpp
)
target_include_directories(${SAVANNA}.Primitives.Record PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA}.Primitives.Record
        PRIVATE
        ${SAVANNA}.Primitives.Field
        ${SAVANNA}.Primitives.Table
)
##############################################################################