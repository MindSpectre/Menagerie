
add_subdirectory(column)

add_subdirectory(field)

add_subdirectory(record)

add_subdirectory(table)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_combined_library(${REEF}.Primitives
        DIRECTORIES
        export/
        SOURCES
        export/db_core_objects.hpp
        LIBRARIES
        ${REEF}.Primitives.Field
        ${REEF}.Primitives.Column
        ${REEF}.Primitives.Record
        ${REEF}.Primitives.Table
)
##############################################################################