##############################################################################
# Column
##############################################################################
add_library(${DATABASE}.Primitives.Column INTERFACE
        db_column.hpp
        db_typed_column.hpp
)

target_include_directories(${DATABASE}.Primitives.Column INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

#target_link_libraries(${DATABASE}.Primitives.Column PUBLIC
#        ${DATABASE}.Primitives.Field
#)
##############################################################################