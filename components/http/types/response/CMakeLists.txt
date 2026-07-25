##############################################################################
# Http Types — Response (stored allocator, fluent setters) — header-only
##############################################################################
add_library(${HTTP}.Types.Response INTERFACE response.hpp)

target_include_directories(${HTTP}.Types.Response INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.Response INTERFACE
        ${HTTP}.Types.Body
        ${HTTP}.Types.Headers
        ${HTTP}.Types.Enums
)
##############################################################################
