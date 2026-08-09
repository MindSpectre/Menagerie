##############################################################################
# Http Drivers — HttpDriver concept (header-only)
##############################################################################
add_library(${Estuary}.Drivers.Concept INTERFACE
        http_driver_concept.hpp
)

target_include_directories(${Estuary}.Drivers.Concept INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Drivers.Concept INTERFACE
        ${Estuary}.Types.Enums
)
##############################################################################
