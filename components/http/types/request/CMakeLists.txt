##############################################################################
# Http Types — Request (string_view target, value Body) — header-only
##############################################################################
add_library(${HTTP}.Types.Request INTERFACE request.hpp)

target_include_directories(${HTTP}.Types.Request INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.Request INTERFACE
        ${HTTP}.Types.Body
        ${HTTP}.Types.Headers
        ${HTTP}.Types.Enums
)
##############################################################################
