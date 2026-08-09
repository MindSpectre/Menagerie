##############################################################################
# Http Types — Request (string_view target, value Body) — header-only
##############################################################################
add_library(${Estuary}.Types.Request INTERFACE request.hpp)

target_include_directories(${Estuary}.Types.Request INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.Request INTERFACE
        ${Estuary}.Types.Body
        ${Estuary}.Types.Headers
        ${Estuary}.Types.Enums
)
##############################################################################
