##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${REEF}.Primitives.Record STATIC
        db_record.hpp
        db_record_factory.hpp
        db_record.cpp
)
target_include_directories(${REEF}.Primitives.Record PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${REEF}.Primitives.Record
        PRIVATE
        ${REEF}.Primitives.Field
        ${REEF}.Primitives.Table
)
##############################################################################