##############################################################################
# Http Types — Response (stored allocator, fluent setters) — header-only
##############################################################################
add_library(${Estuary}.Types.Response INTERFACE response.hpp)

target_include_directories(${Estuary}.Types.Response INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.Response INTERFACE
        ${Estuary}.Types.Body
        ${Estuary}.Types.Headers
        ${Estuary}.Types.Enums
)
##############################################################################
