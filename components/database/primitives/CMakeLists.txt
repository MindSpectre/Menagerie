
add_subdirectory(column)

add_subdirectory(field)

add_subdirectory(record)

add_subdirectory(table)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_combined_library(${DATABASE}.Primitives
        DIRECTORIES
        export/
        SOURCES
        export/db_core_objects.hpp
        LIBRARIES
        ${DATABASE}.Primitives.Field
        ${DATABASE}.Primitives.Column
        ${DATABASE}.Primitives.Record
        ${DATABASE}.Primitives.Table
)
##############################################################################