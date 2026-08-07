##############################################################################
# Http Types — Request (string_view target, value Body) — header-only
##############################################################################
add_library(${Http}.Types.Request INTERFACE request.hpp)

target_include_directories(${Http}.Types.Request INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.Request INTERFACE
        ${Http}.Types.Body
        ${Http}.Types.Headers
        ${Http}.Types.Enums
)
##############################################################################
