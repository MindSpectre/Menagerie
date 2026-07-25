##############################################################################
# Http Drivers — HttpDriver concept (header-only)
##############################################################################
add_library(${HTTP}.Drivers.Concept INTERFACE
        http_driver_concept.hpp
)

target_include_directories(${HTTP}.Drivers.Concept INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Drivers.Concept INTERFACE
        ${HTTP}.Types.Enums
)
##############################################################################
