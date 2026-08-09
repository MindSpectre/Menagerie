##############################################################################
# Http Types — protocol-agnostic enums (header-only)
##############################################################################
add_library(${Estuary}.Types.Enums INTERFACE http_enums.hpp)

target_include_directories(${Estuary}.Types.Enums INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.Enums INTERFACE
        Boost::beast
)
##############################################################################
