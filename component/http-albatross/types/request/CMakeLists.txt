##############################################################################
# Http Types — Request (string_view target, value Body) — header-only
##############################################################################
add_library(${Albatross}.Types.Request INTERFACE request.hpp)

target_include_directories(${Albatross}.Types.Request INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.Request INTERFACE
        ${Albatross}.Types.Body
        ${Albatross}.Types.Headers
        ${Albatross}.Types.Enums
)
##############################################################################
