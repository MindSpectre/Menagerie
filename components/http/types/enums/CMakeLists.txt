##############################################################################
# Http Types — protocol-agnostic enums (header-only)
##############################################################################
add_library(${HTTP}.Types.Enums INTERFACE http_enums.hpp)

target_include_directories(${HTTP}.Types.Enums INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.Enums INTERFACE
        Boost::beast
)
##############################################################################
