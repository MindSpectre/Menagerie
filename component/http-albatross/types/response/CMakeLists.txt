##############################################################################
# Http Types — Response (stored allocator, fluent setters) — header-only
##############################################################################
add_library(${Albatross}.Types.Response INTERFACE response.hpp)

target_include_directories(${Albatross}.Types.Response INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.Response INTERFACE
        ${Albatross}.Types.Body
        ${Albatross}.Types.Headers
        ${Albatross}.Types.Enums
)
##############################################################################
