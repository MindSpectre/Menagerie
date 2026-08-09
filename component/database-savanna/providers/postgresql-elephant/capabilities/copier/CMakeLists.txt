##############################################################################
#
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Capabilities.Copier STATIC
        postgres_copier.cpp
        postgres_copier.hpp
)

target_include_directories(${SAVANNA_ELEPHANT}.Capabilities.Copier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities.Copier PUBLIC

)
##############################################################################