##############################################################################
# Http Drivers — HttpDriver concept (header-only)
##############################################################################
add_library(${Albatross}.Drivers.Concept INTERFACE
        http_driver_concept.hpp
)

target_include_directories(${Albatross}.Drivers.Concept INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Drivers.Concept INTERFACE
        ${Albatross}.Types.Enums
)
##############################################################################
