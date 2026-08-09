##############################################################################
# Column
##############################################################################
add_library(${SAVANNA}.Primitives.Column INTERFACE
        db_column.hpp
        db_typed_column.hpp
)

target_include_directories(${SAVANNA}.Primitives.Column INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

#target_link_libraries(${SAVANNA}.Primitives.Column PUBLIC
#        ${SAVANNA}.Primitives.Field
#)
##############################################################################