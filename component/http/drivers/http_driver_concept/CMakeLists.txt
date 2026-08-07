##############################################################################
# Http Drivers — HttpDriver concept (header-only)
##############################################################################
add_library(${Http}.Drivers.Concept INTERFACE
        http_driver_concept.hpp
)

target_include_directories(${Http}.Drivers.Concept INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Drivers.Concept INTERFACE
        ${Http}.Types.Enums
)
##############################################################################
