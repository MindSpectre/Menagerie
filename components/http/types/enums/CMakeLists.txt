##############################################################################
# Http Types — protocol-agnostic enums (header-only)
##############################################################################
add_library(${Http}.Types.Enums INTERFACE http_enums.hpp)

target_include_directories(${Http}.Types.Enums INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.Enums INTERFACE
        Boost::beast
)
##############################################################################
