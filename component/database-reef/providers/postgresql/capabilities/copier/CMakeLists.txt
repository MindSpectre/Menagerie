##############################################################################
#
##############################################################################
add_library(${REEF_POSTGRESQL}.Capabilities.Copier STATIC
        postgres_copier.cpp
        postgres_copier.hpp
)

target_include_directories(${REEF_POSTGRESQL}.Capabilities.Copier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${REEF_POSTGRESQL}.Capabilities.Copier PUBLIC

)
##############################################################################