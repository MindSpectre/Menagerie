##############################################################################
# Http Types — Response (stored allocator, fluent setters) — header-only
##############################################################################
add_library(${Http}.Types.Response INTERFACE response.hpp)

target_include_directories(${Http}.Types.Response INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.Response INTERFACE
        ${Http}.Types.Body
        ${Http}.Types.Headers
        ${Http}.Types.Enums
)
##############################################################################
