##############################################################################
# Column
##############################################################################
add_library(${REEF}.Primitives.Column INTERFACE
        db_column.hpp
        db_typed_column.hpp
)

target_include_directories(${REEF}.Primitives.Column INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

#target_link_libraries(${REEF}.Primitives.Column PUBLIC
#        ${REEF}.Primitives.Field
#)
##############################################################################