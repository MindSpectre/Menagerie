
add_subdirectory(column)

add_subdirectory(field)

add_subdirectory(record)

add_subdirectory(table)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_combined_library(${SAVANNA}.Primitives
        DIRECTORIES
        export/
        SOURCES
        export/db_core_objects.hpp
        LIBRARIES
        ${SAVANNA}.Primitives.Field
        ${SAVANNA}.Primitives.Column
        ${SAVANNA}.Primitives.Record
        ${SAVANNA}.Primitives.Table
)
##############################################################################